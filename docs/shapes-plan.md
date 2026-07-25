# Shapes: shape-guarded property access, co-designed with the GC (maam P4)

This is the maam-plan **P4 design document** — the phase the plan scoped as
"design doc only" and deferred "until Phase 3 has proven the pipeline."
Phase 3 has: typed arithmetic (P3), trust-free guard-region optimization
(P3.4), the differential harness that validates the oracle against concrete
execution (P3.5), and function specialization with native unboxed
signatures (P3.6, types-bench1 ~46×, hypot2 demo ~90×).  What is left on
the table after all of that is exactly one thing: **the object model**.
Every property access is still a hash lookup through an out-of-line
malloc'd map behind two indirect calls, every object literal is built by N
generic inserts, and the P3.6 benchmarks' residual wall time is boxed slot
traffic.  Shapes are where that goes away, and — per gc-plan.md §"Object
header, forwarding, and shapes" — they are "the single highest-leverage
item" in the GC redesign too.  This document is the joint design the two
plans each point at, written against the GC plan's Phase 1 header layout,
as instructed there ("The GC must not ship a header layout that shapes
then has to break").

Deliverables of this doc: the runtime shape model, the object layout
migration, the EIR ops and their verifier/trust rules, how the oracle's
`layouts()`/`constructors()` facts are consumed and *when they may be
trusted* (the promotion criteria, distilled from Phase 3 experience), a
phased implementation checklist with gates, and the validation story.

## What we have today, as found

Runtime object model (all file:line refs current at this writing):

- `EJSObject` = `{ GCObjectHeader gc_header; EJSSpecOps* ops; ejsval
  proto; EJSPropertyMap* map; }` (`runtime/ejs-object.h:229-234`).  The
  header is a bare `uint32_t` (`ejs-types.h:30`) whose low bits are the
  `EJSScanType` and whose high byte holds user flags (extensibility)
  (`ejs-gc.h:14-23`, `ejs-object.h:220-227`).  On 64-bit targets 4 bytes
  of padding follow it.  **No inline property slots exist.**
- The property map is a per-object, malloc'd (non-GC-heap), separately
  chained hash table (`ejs-object.h:116-136`) whose entries point at
  individually malloc'd `EJSPropertyDesc` descriptors — flags + value or
  getter/setter (`ejs-object.h:13-40`).  Insertion order is a second
  linked list threaded through the entries (enumeration depends on it).
  It rehashes through a prime ladder and aborts past 4099 buckets
  (`ejs-object.c:388-391`).
- A property get is: `_ejs_object_getprop` → `ToObject`/checks → indirect
  `OP(obj,Get)` → `ToPropertyKey` (may `ToString`) → indirect
  `GetOwnProperty` → hash, modulo, bucket-chain walk with
  `_ejs_op_strict_eq` per candidate — repeated per prototype level
  (`ejs-object.c:830-863`, `2039-2094`).  A property add is a descriptor
  malloc + map insert (`ejs-object.c:533-572`, `2098-2168`).
- Allocation is `ops->Allocate()` = `_ejs_gc_alloc(sizeof(EJSObject))`
  followed by `_ejs_init_object`, which **calloc's the map** — every
  object is two allocations, one outside the GC heap
  (`ejs-object.c:752-772`, `2400-2403`).
- There are **no hidden classes, no shapes, no inline caches** anywhere
  in the runtime (grep-confirmed).  Class identity is the `ops` pointer.
- Compiled code's view of `EJSObject` lives in `lib/types.ts:88-103` and
  must move in lockstep with any runtime layout change (the gc-plan's
  atomic-land rule).

Compiler/oracle side:

- maam computes **type-aware hidden classes**: a `Shape` is an interned
  *set* of `(name, TypeSig)` fields — order-insensitive by design (an AOT
  compiler picks its own layout; order-sensitivity cost splay 109,603
  shapes vs 258), hash-consed with stable ids, with a megamorphic `⊤`
  under a per-address cap (`echojs-maam/src/lang/shapes.ts`).
- `layouts()` reports, per allocation site, the **terminal** shapes its
  objects settle into (construction intermediates are subsumed away) plus
  a C-style struct layout per shape under a pluggable size model;
  `monomorphic` = exactly one terminal shape.  `constructors()` gives the
  same per `new F()` callee.  `accessorSites()` marks getter/setter
  dispatch sites and their target sets (`echojs-maam/src/layout.ts`,
  `src/analysis.ts:38-97`).  All of these are **`Loc`-keyed**; the
  node-identity discipline the compiler consumes (`typeOfNode`,
  P1's oracle contract) does not cover them yet.
- EIR lowers property access to `get_prop_atom`/`set_prop_atom`
  (GENERIC_OP runtime calls), and `make_object` to `_ejs_object_create`
  plus one full generic `_ejs_object_setprop` per key
  (`lib/eir/emit.ts` "make_object").

## The design in one paragraph

The runtime grows a global, interned, **type-aware shape tree** that
mirrors maam's abstraction one-for-one; every ordinary object carries a
shape index in its (gc-P1-widened) header and stores its plain data
properties in a **slot array at shape-determined offsets**, falling back
to today's map ("dictionary mode") the moment anything exotic happens —
deletes, non-default attributes, symbol keys, cap overflow.  The compiler,
exactly as in Phase 3, consumes oracle shape facts **guarded**: a property
access on an oracle-monomorphic receiver lowers to a `has_shape` diamond
whose fast arm is a fixed-offset slot load/store (typed, when the shape's
field representation says so) and whose slow arm is today's generic call —
correct regardless of oracle accuracy, because the guard decides at
runtime.  Allocation sites with known terminal shapes are **born with
their shape** (one sized allocation, direct slot stores, no map, no
descriptor mallocs) under the same structural fences P3.6 built for
specialization.  The GC's Phase 5 then consumes the same shapes for
per-shape trace bitmaps and memcpy evacuation; nothing in this document
waits for the mover, and nothing here may break its header.

## Runtime design

### Shapes are type-aware, and mirror maam exactly

A runtime shape is `(parent, name, repr)` — a transition edge appended to
a parent shape, where `repr` is the field's representation: one of
`{unboxed-f64, boxed}` initially (finer tags later if profitable).  maam
made representation part of class identity because an AOT compiler gets no
deprecate/migrate second chance; the runtime must agree, or a compiled
guard could pass while the field representation lies.  A type-changing
store (`o.x = "s"` where x was num) is therefore a **transition** like a
property add: the object moves to the sibling shape with `x: boxed`, and
compiled fast paths guarding the old shape correctly fail to the generic
path.  This is the load-bearing choice of the whole design: **a passed
shape guard proves both structure (offset) and representation (how to
load)**, so in typed regions a field the oracle typed `num` is one
compare + one 8-byte load away from a raw `double` — no `has_tag`, no
unbox — and the P3.4/P3.6 raw-value machinery applies unchanged
downstream.

Two deliberate divergences from maam's shape table, both mechanical:

- **Insertion order.**  maam interns order-insensitively; ES enumeration
  is insertion-ordered, and today's runtime honors that via the map's
  insert list.  Runtime shapes get order for free — the transition chain
  *is* the insertion order — so enumeration walks the shape's field
  chain.  The correspondence rule for the compiler: a *runtime* shape is
  an ordered witness of a maam shape (same field set, same reprs).  The
  compiler must therefore know the ORDER, not just the set, to intern the
  guard's expected shape — see "deriving ordered shapes" below.
- **Interning is global and cross-module.**  One process-wide shape
  table, append-only, sharded by parent (transition lookup:
  `parent × name × repr → child`, one hash hit per property add).  Shape
  ids are stable within a process, NOT across processes or modules at
  compile time — compiled code never embeds a numeric id.  Instead each
  module interns the shapes it guards on at module init (exactly the
  atom-table precedent: `getAtom`/`_ejs_module` machinery) and guards
  compare against the module-global's loaded value.  Cross-module
  structural identity falls out of interning.

### Object layout, in two steps

**Step A (before gc-P5, works on today's non-moving collector):**

    EJSObject:
      u32  gc_header        (unchanged low bits: scan type, user flags)
      u32  shape_index      (the gc-P1 reserved bits; 0 = dictionary)
      EJSSpecOps* ops
      ejsval      proto
      union { EJSPropertyMap* map;    // dictionary mode (shape 0)
              ejsval*         slots;  // shaped mode: GC-heap slot array
            }

  The shape index takes the 4 padding bytes the gc-plan's Phase 1
  earmarks (gc-plan.md:291-300) — this doc claims 24 bits of them for the
  shape index plus a mode bit; forwarding/age/mark/card bits own the
  rest, allocated jointly with gc-P1 in one atomic
  `runtime/` + `lib/types.ts` change.  If shapes land before gc-P1, the
  same commit simply widens the header first and gc-P1 inherits it; the
  two plans agreed this is one layout, written once.
  Slot arrays are GC-heap allocations (`EJS_SCAN_TYPE` of their own,
  scanned as ejsval ranges — precise tracing needs no per-shape bitmap
  yet), sized to the shape's field count rounded to the allocator's size
  class, grown by copy on transition past capacity.  Objects lose the
  calloc'd map entirely in shaped mode; dictionary mode keeps today's map
  code verbatim.

**Step B (gc-P5, the fused future):** slots move inline —
`shape id + contiguous inline slots`, fixed-size, memcpy-copyable, traced
by per-shape pointer bitmaps, born from the bump allocator.  Nothing in
this document's compiler-visible contract changes at that point except
the addressing base (slot array pointer → object-interior offset); the
EIR ops below deliberately take a slot *index* immediate so the emitter
owns that switch.

### Semantics: what is shaped, and what falls back

Shaped mode covers **plain data properties with default attributes
(writable, enumerable, configurable) and string keys on ordinary
objects** (`ops == &_ejs_Object_specops`).  Everything else is dictionary
mode, entered by a one-way `to_dictionary(obj)` migration (allocate map,
insert fields in shape order, shape_index := 0):

- `delete` of a shaped field (`ejs-object.c:2199-2220` path);
- `Object.defineProperty` with any non-default attribute, or
  data↔accessor conversion (`ejs-object.c:2224-2397`);
- accessor definition (`ejs-object.c:886-903`);
- symbol keys; numeric/index keys (arrays keep their own storage;
  indexed access on plain objects is rare enough to eat the map);
- transition-cap overflow (a per-object add-count cap, the runtime twin
  of maam's `shapeCap`) and any shape-table pathology;
- `preventExtensions`/`freeze`/`seal` keep shaped mode (they only toggle
  the extensibility flag and attribute bits conceptually — but a
  non-writable field breaks the "plain store" invariant, so freeze/seal
  ALSO migrate; `preventExtensions` alone does not).

`[[Set]]` on a shaped object: field present with same repr → slot store;
present with different repr → transition (sibling shape, same offsets,
new repr), then store; absent + extensible → transition (append), grow
slots if needed, store; anything else → migrate, then today's path.
`[[Get]]`/`GetOwnProperty` on shaped objects synthesize the default
descriptor from the slot; the specops keep their signatures — shapes are
an implementation detail *behind* `_ejs_Object_specops`, invisible to
every other class and to the spec algorithms above it.  Proto mutation
(`__proto__` setters, `_ejs_object_literal_set_proto`) does not affect
the shape (shapes describe own-property structure only; proto stays a
per-object field), so no Crankshaft-style proto-in-shape complexity.

The insertion-order list, `OwnPropertyKeys`, `Enumerate`, and the
for-in iterator read shaped objects by walking the shape chain
(`ejs-object.c:639-660`, `1103-1118` become mode-switched); the
scan/finalize specops likewise (`ejs-object.c:2406-2434`).

### Born with their shape

`make_object` at a site whose literal keys are static becomes: intern the
ordered shape at module init; allocate object + slot array in one runtime
call `_ejs_object_new_shaped(shape, proto)`; store each value at its
fixed offset (initializing stores — barrier-elidable when gc-P2 lands).
This is correct *unconditionally* for object literals — the literal's key
order and count are the site's static truth, no oracle involved; the
oracle only adds field *representations* (typed slots) and the terminal
shape when later code appends more fields.  Constructor bodies are the
oracle-and-fence case: see promotion criteria.

## EIR design

### Ops

    // i1: does obj's shape index equal the module-interned shape?
    // imms.shape names the module's shape-table entry (a link-time
    // global, like atoms).  Effect NONE — a pure header compare.
    has_shape:   { arity: 1, effects: NONE, imms: ["shape"],
                   sig: { params: ["ejsval"], result: "i1" } }

    // fixed-slot access.  imms.slot is the field index within the
    // guarded shape (the emitter turns it into slot-array/inline
    // addressing); imms.repr ∈ {"boxed","f64"} selects the load/store
    // type — "f64" produces/consumes raw f64 (the P2 typed-flow rules
    // apply; only reachable behind a has_shape proving that repr).
    slot_load:   { arity: 1, effects: READ,  imms: ["slot", "repr"] }
    slot_store:  { arity: 2, effects: WRITE, imms: ["slot", "repr"] }

    // allocation with a known shape: operands are the initial slot
    // values in shape order (imms.shape, imms.reprs).  GC|WRITE like
    // make_object; replaces make_object at statically-shaped sites.
    make_object_shaped: { arity: -1, effects: GC|WRITE,
                          imms: ["shape", "reprs"] }

Verifier rules, in the P2/P3.4/P3.6 lineage: `slot_load`/`slot_store`
with `repr:"f64"` produce/take raw f64 and are subject to the existing
raw-values rules; a `slot_*` op must be dominated by a `has_shape` on the
same value for the same shape **when carrying `repr:"f64"`** (the boxed
case is memory-safe under any shape of at least `slot+1` fields, but the
verifier still requires the guard — structural discipline over cleverness,
same as rawJoin's re-checked marker).  `has_shape` on a non-object value
is simply false at runtime (the emitter folds the NaN-box object check
into the shape-index load exactly as `isNumber` backs `has_tag`).

### Lowering: the shape diamond

`o.x` where the oracle types `o`'s site monomorphic with terminal shape S
(and `x` present in S):

    %t = has_shape %o, shape="S"
    cond_br %t -> ^fast, ^slow
    ^fast:  %v = slot_load %o, slot=k, repr=…   (raw f64 when S says num)
    ^slow:  %g = get_prop_atom %o, atom="x"     (today's generic call)
    join boxed — or raw, via the existing rawJoin machinery

— the same diamond skeleton as `numericDiamond`, the same join
conventions, the same "guarded consumption is correct even when the
oracle is wrong" contract.  Stores dual.  `optimize-guards.ts` extends
its dominator facts: a dominating passed `has_shape %o, S` proves (a)
later `has_shape %o, S` guards fold, (b) `%o`'s field reprs — so a
`slot_load repr:"f64"` needs no `has_tag`, and consecutive accesses to
the same receiver merge into ONE guard region with one slow path,
exactly the hypot2 shape.  SSA immutability makes receiver-value facts
sound the way number-ness was; **stores are the new wrinkle**: a
`slot_store`/`set_prop_atom`/call/construct between accesses can
transition the receiver's shape, so shape facts are killed by
WRITE|CALL-effect instructions on any path — the fact table gains an
effect-kill rule the number facts never needed.  (A same-region
`slot_store` that does not add a field and matches repr provably does
NOT transition — the one exception the fact engine may keep.)

`new F()` with a monomorphic `constructors()` report lowers `construct`
unchanged (the runtime allocates via F) in the guarded phase;
born-with-shape construction is a promotion (below), not a lowering
default.

### Oracle interface additions (maam side, small)

The compiler consumes shape facts through the node-identity discipline
every prior phase used; `Loc`-keyed tables don't survive the desugar
pipeline's node surgery.  maam grows (mirroring `nodeTypes()`):

- `layoutOfNode(objectLiteralNode)` → SiteLayout | undefined;
- `constructorReportOfNode(fnNode)` → ConstructorReport | undefined;
- `receiverShapesOfNode(memberExprObjectNode)` → Shape[] — the shapes
  the *receiver value* of a property access may have (join over reached
  configurations), which is what access-site guarding actually needs
  (allocation-site layouts alone don't cover parameters/loads);
- ordered-shape witnesses: for literals the compiler orders fields
  itself; for constructor reports maam must ALSO expose the terminal
  shape's field order as first-write program order per analyzed path, or
  decline (order ambiguity ⇒ no born-with-shape, guards still fine since
  guards compare interned ordered shapes the RUNTIME built — see open
  question 1).

The compiler-side `TypeOracle` (lib/eir/oracle.ts) grows the same three
queries plus pass-through of `shapeCapHits`/megamorphic flags for the
promotion gates.  `--types-dump` grows a per-site shape census
(diagnostics first — the plan's original P4 note — which doubles as the
instrumentation the P4.1 gate needs).

## Promotion criteria — what Phase 3 taught us

The trust ladder, restated as policy for shapes:

1. **Guarded by default.**  Shape diamonds are emitted wherever facts are
   *exact* — monomorphic, non-megamorphic, `shapeCapHits==0` for the
   site, every guarded field's repr a single tag.  Wrong oracle = slow
   path taken = speed lost, never correctness — the P3 contract.
2. **Exact facts only, no near-misses.**  Three or more terminal shapes
   ⇒ no diamond; exactly two lower to the P4.6 2-way chain (measured and
   landed — see the P4.6 entry), and only when EVERY shape in the answer
   passes the same exactness screen and carries the accessed field;
   union-repr fields load boxed; anything the oracle degraded
   (`degradedBindings`, unknown calls touching the receiver) declines.
   This is `operandIsNumber`'s "exactly {number}" rule transplanted.
3. **Unguarded consumption only behind structural fences.**
   Born-with-terminal-shape construction asserts facts (in-ness of
   not-yet-assigned fields is observable: `"b" in this` mid-construction
   must be false, but a terminal-shape-born object would say true).  So
   it requires the P3.6 fence pattern, compiler-side and oracle-free:
   the constructor's `this` never escapes before the last field store
   (no calls, no stores of `this`, no `in`/`delete`/enumeration — a
   straight-line store prefix), checked structurally on the lowered EIR
   like the escape analysis checked closures.  Object literals need no
   fence (their construction is atomic in the source).  P3.6 clones may
   additionally drop shape guards on receivers their own escape analysis
   proves site-local — later, measured, never first.
4. **Trust-free optimizer, provenance-not-trust markers.**  Guard-region
   merging and fact folding must re-verify structure (the P3.4 verifier
   discipline: a marker can tighten checking, never admit); the
   effect-kill rule for shape facts is part of the verifier's soundness
   inventory from day one — it is THE new hazard class this phase adds,
   and the adversarial-review focus (P3.4's review found 4 miscompiles
   in exactly this kind of machinery; assume this phase's review will
   too).
5. **Visible degradation.**  Every declined promotion has a counted
   reason (`shapes: declined polymorphic=N megamorphic=M capped=K
   escaped=E`), printed on the stats line; the diff-lane scrapes stay
   additive-only.
6. **Bisect hooks per mechanism.**  `EJS_NO_SHAPE_GUARDS`,
   `EJS_NO_BORN_SHAPED`, runtime `EJS_SHAPES=off` (dictionary-only mode)
   — the EJS_NO_EIR_OPT/EJS_NO_EIR_SPEC mold.

## Validation

- **The differential harness is the precondition again.**  Before any
  unguarded consumption (born-with-shape), the P3.5 harness grows a
  shapes lane: per allocation site, the concrete machine's object
  field-sets must be contained in the abstract terminal+intermediate
  shape sets (`abstract ⊒ concrete`, the containment-lane pattern), and
  `ejs` runs must agree with node on shape-sensitive observables
  (`Object.keys` order, `in` during construction, delete-then-readd,
  freeze/seal, accessor conversion).  Guarded-phase work (P4.3) does not
  wait for this; born-with-shape (P4.4) hard-requires it — the P3.5/P3.6
  sequencing, replayed.
- **Runtime differential mode.**  P4.1/P4.2 land behind `EJS_SHAPES=off`;
  the whole test suite runs both modes and byte-compares (the old-
  collector A/B discipline from gc-plan).  A transition-storm stress test
  (add/delete/type-flip churn) and the collect-every-N stress compose.
- **The --types diff lane** stays the behavioral gate for every compiler
  phase, unchanged: flag-off untouched, `--types` byte-identical stdout.
- **Wrong-oracle probes.**  A probe whose runtime shape diverges from the
  oracle's claim (cross-module mutation of a "monomorphic" site's object,
  the types-wrongoracle1 pattern) must route through the guard's slow
  path with identical output; a looks-fenced-but-isn't constructor (mid-
  construction escape via a call) must be *rejected by the structural
  check*, pinned at unit level with a lying stub oracle — the P3.6
  wrong-oracle discipline, transplanted.
- **EIR-shape unit tests** for every op/verifier rule (guard-dominance
  for f64 slots, effect-kill of shape facts, merge refusals), and probes
  in test/types/ with census entries.

## Benchmarks

- **types-bench2** (new): constructor + field-access kernel — allocate N
  points in a loop, sum `p.x*p.x + p.y*p.y` — the object-model twin of
  types-bench1; measured at every phase gate (guarded, born-shaped,
  typed-slots deltas recorded like 10.3×→14.0×→46× was).
- **splay** (the shape-stress classic; maam's own shape work was tuned on
  it) as the polymorphism/transition stress once P4.2 lands.
- The gc-plan Phase 0 allocation profile doubles as the object-size/
  field-count census that sizes slot-array classes.

## Phased plan

Same bias as eir/maam/gc: small phases, matrix green after each, each
revertable, runtime phases A/B-able against the old path.

- [x] **P4.1 — Runtime shape tracking, behind the scenes.**  DONE
      2026-07-23.  Shape table + transition cache
      (`runtime/ejs-shapes.{h,c}`); ordinary objects get shape indices
      maintained on insert/delete/type-flip; the MAP REMAINS the store
      (dual bookkeeping, zero behavior change); `EJS_SHAPES=off` kills
      it, `EJS_SHAPES_CENSUS=1` dumps the census at exit,
      `EJS_SHAPE_CAP` overrides the per-object field cap (default 64).
      Header bits landed as the gc-P1 joint layout: `GCObjectHeader` is
      now `uint64_t` (ejs-types.h documents the split — low 32 unchanged,
      bits 32-55 shape index, bit 56 P4.2 mode bit, 57-63 reserved gc);
      `EJSObject`/`EJSPrimString`/`EJSPrimSymbol` sizes unchanged
      (padding absorbed), `EJSClosureEnv` +8; `lib/types.ts` mirrored in
      the same commit (header as two i32 fields so P4.3's `has_shape`
      can load the shape half directly).
      *Gate results:* matrix green (test-eir, lowtier, stages 0-3);
      stage1 suite green with shapes on AND under EJS_SHAPES=off — the
      off-mode run is a standing buck lane, `//:test-stage1-shapes-off`
      (buck-test-stage.sh grew a TEST_ENV arg; the P4.2 both-modes
      byte-identical gate extends this lane);
      property-insert micro-overhead **2.1%** (mean of 5 interleaved
      runs, 300k objects × 8 fresh atom-keyed inserts — the worst case;
      needed the header-inlined transition-memo fast path, which serves
      99.99% of bench transitions: a memo-hit name was vetted when the
      memo's shape was interned, so the whole check collapses to one
      ejsval compare).  Census (3-site probe: literal loop, delete,
      accessor, repr-flip): 1052 objects born tracked, 160 shapes
      interned, max depth 56 (a runtime-init builtin), transitions 3206
      of which 95% memo hits, 1 repr flip, 42 migrations (attrs 28 /
      accessor 11 / symbol-key 2 / delete 1 — runtime-init builtins
      dominate; user objects stay shaped).  Death census needs a
      collection to fire (finalize-driven), so short probes report 0
      deaths — the shapes analog of gc-P0's numbers lands with real
      workloads in the P4.2 gate.
- [x] **P4.2 — Slot storage for shaped objects.**  DONE 2026-07-24.
      The union flip landed: `EJSObject`'s fourth word is now
      `union { EJSPropertyMap* map; ejsval slots; }` — shaped-mode
      objects store plain data property values in a **closureenv** slot
      array (already GC-allocated, ejsval-range-scanned, and traceable
      via its ejsval tag: zero GC changes, `lib/types.ts` untouched
      since the word stays pointer-sized and compiled code never
      dereferences it).  Slot storage is lazy (`_ejs_null` until the
      first property; grow-by-doubling from 4), so ordinary-object
      allocation lost the map calloc entirely.  ejs-shapes.c became a
      pure transition/query API (`_ejs_shape_lookup` / `_fields` /
      `_transition_add(+memo fast path)` / `_transition_set`); the
      storage engine and the one-way `_ejs_object_to_dictionary`
      (materialize map from shape+slots, malloc-only, no GC points)
      live in ejs-object.c.  Specops mode-switched: get (fast-path slot
      load), set (fast-path store on the receiver incl. repr-flip
      transition), define (shaped routing for plain default-attr data
      props; everything else migrates then falls into the untouched
      generic algorithm), delete (migrate then map-remove),
      GetOwnProperty (synthesizes the default data descriptor into a
      32-entry gc-rooted static ring — safe because every
      descriptor-mutating path migrates first), scan/finalize, plus the
      map-walking sites: collect_keys (for-in), OwnPropertyKeys (shared
      classification loop keeps the two modes byte-identical),
      getOwnPropertyNames/Symbols, Object.assign, defineProperties.
      **The stage2 lesson (found at this gate, the hard way):** the
      first cut hung stage2's self-compile for hours at 100% CPU inside
      GC marks.  Two causes, both fixed here: (1) slot arrays of
      capacity 32+ exceed the page allocator's largest cell — which is
      **128 bytes**, not the 256 its comment claims (`ffs(256)=9 > 8`
      LOS-routes exact-256 allocations) — so every wide object's storage
      landed in the LOS, whose **per-reference linear lookup** made
      marking quadratic (multi-minute marks of a 183MB heap; lldb kept
      landing on the los_list walk at ejs-gc.c:550).  Fix:
      `EJS_SHAPE_FIELD_CAP_MAX = 14` (16B env header + 14×8 = exactly
      128B; growth 4→8→14); 15+-field objects drop to dictionary mode.
      Revisit when gc-plan gives the LOS an O(log n) lookup or a 256B
      size class.  (2) the collection trigger was a **fixed 60MB of
      allocation** — quadratic total GC work on a growing live set now
      that property storage lives in the GC heap.  Fix in ejs-gc.c: the
      trigger scales to max(60MB, post-sweep-footprint/2); programs
      under 120MB footprint keep the old cadence exactly.  With both
      fixes stage2's self-compile completes normally (ejs-process CPU:
      92s shapes-on vs 62s off on the same binary — the ~1.5× is env
      alloc churn plus wide-object migrate-through; the raw win arrives
      with P4.3's guarded fast paths, and P4.5/gc-P5 own the layout
      end-state).
      *Gate results:* matrix green — test-eir, lowtier, stages 0-3, and
      the `//:test-stage1-shapes-off` A/B lane (no kangax runner exists
      in-repo; the stage suite + the new probe stand in).  New
      `test/shapes-storm1.js` transition-storm probe (adds, repr flips,
      deletes, attrs/accessor/symbol/index migrations, freeze/seal,
      enumeration order, assign/defineProperties/JSON): node-identical,
      byte-identical across EJS_SHAPES on/off, and green under
      EJS_GC_EVERY_N_ALLOC=7 in both modes.  Microbench (300k objects ×
      8 atom-keyed fields, 20 passes, interleaved runs, post-fix):
      **set 3.2× faster** than the map (6.35s vs 19.9s — no hash, no
      strict-eq chain, no descriptor churn), **get 1.09×** (5.95s vs
      6.47s; the generic-call overhead still dominates — the raw win is
      P4.3's guarded fast paths), insert 8×N **~3% slower** (1.93s vs
      1.88s: one closureenv alloc + one grow-copy per 8-field object —
      within the P4.1 <5% bar, and the shaped path now does real work
      instead of dual bookkeeping).  Census on the storm probe: 383
      born tracked, 315 shapes, 1365 transitions (48% memo fast hits),
      210 repr flips, migrations correctly attributed.
- [x] **P4.3 — Guarded fast paths under --types.**  DONE 2026-07-24
      (gate results below).  As built:
      - **Ops** (`lib/eir/ops.ts`): `has_shape` (NONE, i1),
        `slot_load` (READ) / `slot_store` (WRITE) with imms
        `shape`/`slot`/`repr` — the ops carry the shape KEY too (a small
        deviation from this doc's sketch) so the verifier compares
        against the guard instead of inferring, and `Module.shapes`
        (ir.ts) holds each module's interned field lists (`internShape`,
        key = `name:repr,...` in insertion order — printed IR is
        self-describing).
      - **Verifier** (`verifier.ts`): the effect-kill soundness inventory
        lives at the top of the file with the engine itself —
        `computeShapeFacts`, a forward must-dataflow (facts born on TRUE
        edges of same-block-fresh has_shape cond_brs, killed by every
        WRITE|CALL, intersected at joins, dead across unwind edges).
        Every slot op must sit under an un-killed fact for its exact
        (value, shape); `slot_store` additionally needs a dominating
        has_tag fact matching the field repr (true-edge for f64,
        false-edge for boxed) OR — f64 only — a value-intrinsic number
        proof (const/box_f64/mul-div-sub), because foldProvenGuards
        legitimately deletes a has_tag on a proven number (found by the
        wrong-oracle probe at this gate, fixed by mirroring the
        optimizer's intrinsic proofs — dominance-fact folds never delete
        the edge the store rule needs).  Slot bounds + repr are checked
        against Module.shapes.
      - **Runtime** (`ejs-shapes.{h,c}`): `_ejs_shape_intern(nfields,
        names, f64_mask)` walks/interns the ordered shape at module init
        (the atom precedent); `EJS_SHAPE_NOMATCH` (0xFFFFFF) is reserved
        (shape_alloc stops one short) so an unfilled/off-mode shape
        global can never match any header — under `EJS_SHAPES=off` every
        guard is false and the slow paths serve everything.
      - **Emitter** (`emit.ts` + compiler.ts): has_shape folds the
        NaN-box object check into the header-high-half compare against a
        per-shape i32 module global (`isObject`/`objectPointer` live
        beside isNumber in compiler.ts); `slotRef` is THE addressing
        seam (P4.2 closureenv slot arrays today, gc-P5 inline slots
        later); interns flush into the literal-init function's return
        block after all atom inits (`emitShapeInterns`).
      - **maam**: `receiverShapesOfNode` (terminal-filtered, node-
        identity, fail-soft) + `fieldOrderOfShape` (the ordered witness =
        first-interning insertion order; a runtime object built in
        another order just misses the guard).  `layoutOfNode`/
        `constructorReportOfNode` are P4.4 consumers and wait there.
      - **Lowering** (`lower.ts` propGet/propSet): diamonds at every
        atom-keyed member get/set incl. compound assign, ++/--, method
        loads, and destructuring reads.  Exact facts only (criterion 2):
        monomorphic, non-⊤, shapeCapHits==0, all reprs single-tag,
        ordered witness, field present — every miss a counted decline.
        Stores guard has_shape AND has_tag oriented by the field repr
        (a repr-flipping store owes a transition, so it routes generic).
        `EJS_NO_SHAPE_GUARDS=1` is the compile-time bisect hook.
      - **optimize-guards**: `optimizeShapeRegions` — strict linear
        get-region matching, twin verification against Module.shapes
        (fast slot_load ↔ slow get_prop_atom, atom==field-at-slot,
        receiver identity, exit args slot-for-slot), the numeric merge's
        mutation mechanics, then fact-based folding (same-block-fresh
        compares only — a stale earlier-block compare can be FALSE where
        the fact holds, pinned by a unit attack).  Consecutive gets on
        one receiver become one guard + one slow path (`p.x + p.x` ⇒ 1
        guard, 2 slot_loads).  Module-toplevel receivers reload their
        slot per access (distinct SSA values), so merging fires inside
        functions — fine: kernels are functions; revisit with slot-load
        CSE if telemetry ever says otherwise.
      - **Telemetry**: stats line grows `shapeSites/shapeGuards/
        shapeDeclined=reason:n,...` (additive; the diff-lane scrape
        regex untouched); `--types-dump` prints a per-site census line
        (`.atom @line:col: guarded shape=... slot=N | declined reason`);
        EIR-opt debug line grows shape guard/region counts.
      Boxed slot ACCESS only in round one, as planned — but repr stays
      part of guard identity and the imms, so P4.5 flips only the
      emitter seam + typed-flow rules.
      *Gate results (2026-07-24):* matrix green (test-eir + new shape
      unit tests incl. hand-built attack IR for every verifier rule and
      merge refusal, lowtier, stages 0-3, `//:test-stage1-shapes-off`);
      --types diff lane **0-divergent** (459 files, 458 identical, 1 N/A
      = tester.js standing esprima gap; suite-wide telemetry: 13,154
      sites consulted, 809 guarded, declines unmapped 7,575 / capped
      4,287 / empty 269 / no-field 194 / poly 12 / union-repr 8 — the
      suite is string-heavy by design, kernels are where guards fire);
      wrong-oracle probe `types-shapeswrong1` (repr-mismatched,
      extra-field, and dictionary-mode receivers cross-module) routes
      slow with node-identical output, incl. under EJS_SHAPES=off and
      EJS_GC_EVERY_N_ALLOC=7; **types-bench2 guarded delta: 2.1×**
      (--types 3.06s vs flag-off 6.56s; vs 5.82s with every guard
      failing under EJS_SHAPES=off ⇒ ~1.9× attributable to the slot
      fast paths, the rest to P3 arithmetic + P3.6); telemetry additive
      (the lane's scrape regex untouched).  Notables found at the gate:
      (1) foldProvenGuards deleting a has_tag on a const stored value
      exposed the verifier/optimizer proof-mismatch fixed via
      provenNumberIntrinsic; (2) the shape-intern emitter originally
      reused the literal-init function and could emit past its
      terminator when a shape named an atom no access ever interned —
      shapes now get their own init function, called right after
      literal init.
- [x] **P4.4 — Born with their shape.**  DONE 2026-07-24.
      PRECONDITION FIRST: the differential harness grew its shapes lane
      (maam submodule @d8610d3) — (a) per-allocation-site shape
      containment in the analysis worker (every concrete hidden class
      needs an abstract witness at its site: ⊤, or same field-name set
      with pointwise ⊒ field types; order-insensitive interning on both
      sides makes write order a non-issue; 350 witness checks across 2
      abstract configs, 0 violations), and (b) `shapes-obs-*.js`
      observable probes run node+ejs ONLY (maam models `delete` as a
      no-op and doesn't model Object.keys/freeze/defineProperty):
      Object.keys order, `in` during construction, delete-then-readd,
      freeze/seal, accessor conversion — each compiled BOTH default and
      `--types`, both byte-matching node.  Gated + vacuous-pass-guarded.
      IMPLEMENTATION (design settled here, deviating from the sketch
      above where the runtime's construct path forced it):
      - **Literals**: statically-keyed literals lower to
        `make_object_shaped` (operands = values in key order, imms.shape
        = the interned ordered field list; static reprs from
        operandIsNumber).  Computed keys, accessors, `__proto__:`,
        duplicate keys, index-looking keys, and >cap field counts keep
        today's lowering.
      - **Constructors are a body-side FILL, not an allocation**: the
        runtime's construct path allocates `this` before the body runs,
        so the batched prefix lowers to a diamond guarded by
        `has_shape(this, "")` — the EMPTY shape (one compare; interning
        zero fields now returns EJS_SHAPE_ROOT) — whose fast arm is
        `fill_object_shaped [this, values...]` and whose slow arm is the
        original sequential set_prop_atom run.  The guard makes
        correctness oracle-INDEPENDENT (no maam constructor query is
        needed at all — constructorReportOfNode never got built);
        monomorphism affects only speed.  The structural fence
        (oracle-free, unit-pinned): plain non-arrow function, prefix =
        maximal leading run of `this.<name> = <Literal | local
        Identifier>` statements (effect-free values ⇒ nothing can
        observe the receiver mid-batch), distinct non-index names, count
        in [2, cap].  `in` mid-prefix, call-valued stores, escaping
        receivers, computed keys all CUT the prefix (fence_declined
        counted by reason).
      - **The runtime re-derives the true shape from the ACTUAL values**
        (`_ejs_object_new_shaped` / `_ejs_object_fill_shaped` in
        ejs-object.c take argc + names[] + values[] and walk the
        transition memo, ~one compare per field when monomorphic) — a
        wrong static repr can never mint a lying shape.  Off-script
        cases fall back to today's sequential `_ejs_object_setprop`
        loop byte-for-byte: EJS_SHAPES=off, non-empty/dictionary/
        non-extensible receivers, index keys, cap — and
        `shaped_proto_intercepts`: a proto-chain ACCESSOR or
        non-writable data property must run assignment ([[Set]])
        semantics, so the batch declines (shaped-mode protos can't
        carry either, so only dictionary-mode protos probe their maps).
      - **Verifier**: operand count == shape field count (+receiver for
        fill); fill requires an un-killed EMPTY-shape fact on its
        receiver through the same computeShapeFacts engine as slot ops
        (attack IR pins: unguarded, killed-fact, wrong-shape guard,
        wrong arity).  The optimizer's region/fold machinery structurally
        ignores the new ops (WRITE effects fail its purity screens).
      - `EJS_NO_BORN_SHAPED` is the bisect hook; telemetry:
        `bornShaped=N ctorFills=N fenceDeclined=reason:n,...`
        (additive).
      FOUND AT THE GATE: a pre-existing P4.3 proof-strength mismatch —
      optimize-guards' provenNumberAt proves const-number JOINS
      (`c ? 1 : 0`) and folds the has_tag over one, but the verifier's
      provenNumberIntrinsic didn't accept blockparams, so the uncovered
      slot_store rejected a VALID optimized module (compile failure, not
      a miscompile; exposed by types-bornshapewrong1's ternary-valued
      ctor store, pinned by born-verify unit tests both directions).
      provenNumberIntrinsic now mirrors the blockparam case.
      *Gate results (2026-07-24):* harness shapes lane green (see
      above) incl. the `in`-during-construction probe under `--types`;
      probes types-bornshape1 / types-bornshapewrong1 node-identical
      (the latter exercises guard-fail reuse, frozen receivers,
      proto-setter interception, non-writable proto swallowing —
      `bornShaped=3 ctorFills=3 fenceDeclined=short-prefix:1`); full
      matrix ×7 green; --types diff lane 0-divergent (459 files, 458
      identical, 1 N/A tester.js; suite-wide **bornShaped=417
      ctorFills=9**, fence declines all short-prefix/value-not-local —
      visible); **types-bench2 3.06s → 2.03s** (--types, median of 3;
      flag-off 6.76s ⇒ **3.3×** total, the new 1.5× step being the
      allocation batching: `ctorFills=1` covers the ctor in both the
      kern and alloc loops).
- [x] **P4.5 — Typed slots × specialization × GC (compiler half).**
      DONE 2026-07-24.  The gc-P5 half (trace bitmaps, inline slots,
      memcpy evacuation, barrier/trace elision) stays sequenced behind
      the mover per gc-plan; the compiler contract it needs was finished
      here.  As built:
      - **The seam flip** (the P4.3 plan, executed): `slot_load
        repr:"f64"` produces a RAW f64 (lowering stamps `Inst.type`,
        boxes once at the fast exit — the join stays boxed since its slow
        edge is the generic get); `slot_store repr:"f64"` consumes a raw
        f64 (lowering unboxes under the existing has_tag guard).  The
        emitter loads/stores the slot as a machine double — same address,
        same 8 bytes (the NaN-box stores doubles raw), so the flip is
        pure type-flow, zero runtime change.  slot ops are typed by their
        repr immediate the way call_typed is typed by its callee (a
        per-op sig can't express either) — the verifier checks the
        result stamp against the repr and requires an f64-typed operand
        for f64 stores.  **The typed store dissolves P4.3's
        proof-strength hazard class**: the store's repr proof is now the
        operand TYPE, which no guard-folding can strip —
        provenNumberIntrinsic (the P4.4 escape hatch that mirrored
        optimizer folds) is deleted; boxed-repr stores keep the
        has_tag=false dominance rule.  No off switch for the seam: it is
        a contract change the verifier owns.
      - **Fusion** (`shape facts feeding the raw-value machinery`): the
        shape-region machinery generalizes to MIXED regions — the slow
        chain admits the numeric whitelist ops, the twin check pairs
        loads↔gets AND f64-ops↔generic-ops (a box_f64 of an f64
        slot_load corresponds to the load's paired get: doubles are
        stored raw, so the get returns bit-for-bit the boxed rendition),
        and `tryMergeShapeNumericAt` merges the NUMERIC region at a
        shape region's join into it (the heterogeneous merge).  After a
        het merge r2's has_tag params are fed only by fast-side box_f64
        values, so foldProvenGuards (now run inside the shape fixpoint)
        deletes them, rawJoinParams turns the joins raw, and the next
        round's matcher grows the region — the cascade ends at ONE
        has_shape guard, raw loads, raw arithmetic, one generic slow
        path (`p.x*p.x + p.y*p.y` ⇒ 1 guard, 4 raw loads, 0 has_tag —
        pinned at unit level).  Re-executing r1's slow chain may now
        re-run generic arithmetic: sound when each operand is
        proven-number at the fast exit OR is one of r1's own paired gets
        naming an f64-REPR field (an f64 slot holds a number by the
        shaped-world invariant; the boxed-field version of that attack
        is unit-pinned to refuse).  `EJS_NO_SHAPE_FUSION` is the bisect
        hook (criterion 6).
      - **Clones**: typed slots reach P3.6 clone interiors through the
        existing machinery with no new code — clone bodies lower against
        `box_f64(formal)`, so the typed store's `unbox(box(p))`
        annihilates into a raw store and slot loads are raw everywhere.
        Clone-internal UNGUARDED slot access (dropping has_shape via the
        escape fence) is NOT built: criterion 3 says later-measured-
        never-first, and the measurements below show the guarded typed
        path already at parity with the trusted clone — there is
        currently nothing for unguardedness to win.  Revisit only on
        benchmark evidence (P4.6 discipline).
      - **Telemetry**: stats line grows `shapeTyped=loads:N,stores:M`
        (additive); EIR-opt debug line grows the het-merge count.
      *Gate results (2026-07-24):* matrix ×7 green (test-eir + new
      typed/fusion/re-exec attack unit tests, lowtier, stages 0-3,
      `//:test-stage1-shapes-off`); --types diff lane 0-divergent (460
      files incl. the new probe); probe `types-typedslots1` (fused
      kernel on matching + repr-mismatched + extra-field + dictionary
      receivers; -0/NaN/Infinity bit-survival through raw slot traffic;
      repr-flip transition mid-kernel; boxed-field stores) node-identical
      in all modes incl. EJS_SHAPES=off and EJS_GC_EVERY_N_ALLOC=7.
      **Measured honestly**: types-bench2 total is UNCHANGED (2.04s vs
      P4.4's 2.03s) because 1.71s of it is the allocation loop — the
      gc-P5 half owns that.  The kernel itself: a variable-receiver
      20M-iteration kernel runs 0.31s under --types vs 3.28s flag-off
      (10.6×), IDENTICAL between P4.4-boxed, P4.5-typed, fused, unfused,
      and specialized — Apple-Silicon OoO + LLVM already hid the boxed
      round-trips, so the typed/fusion wall-time delta on this hardware
      is ~0.  What the seam DOES buy today: an invariant-receiver kernel
      (types-bench2's literal `kern(new Point(3,4), 1e6)` shape) now
      CONSTANT-FOLDS COMPLETELY (0.31s → 0.00s; the boxed form never
      could — LLVM can finally see the loads are pure doubles), the
      guarded path reaches parity with the trusted P3.6 clone, and the
      IR meets gc-P5 with one addressing seam, slot-index immediates,
      and straight-line raw regions to point inline-slot addressing at.
- [x] **P4.6 — Measured extensions.**  DONE 2026-07-24.  The phase ran
      as its own discipline dictates: an evidence probe per candidate
      FIRST, implementation only where the numbers and a sound design
      both existed.  Verdicts:
      - **2-way polymorphic guards: LANDED.**  The evidence probe (two
        Point classes {x,y} / {z,x,y} alternating through one kernel
        site) first exposed a maam precision bug: `receiverShapesOfNode`
        ran the `terminalShapes` subsumption filter over the JOINED
        shape list, so one class's terminal ({x,y}) was absorbed by
        another class's superset ({x,y,z}) exactly as if it were a
        construction intermediate — 2-shape sites reported as
        MONOMORPHIC on the bigger shape (sound only because the runtime
        guard made the {x,y} half run generic; the suite's
        "polymorphic 12" decline census was a large undercount).  Fix
        in maam (`analysis.ts` + pinned test): terminal-filter PER
        OBJECT ADDRESS, then union — an object's own intermediates are
        still subsumed, distinct classes both survive.  Compiler side:
        `ShapeQuery` carries 1-2 exact shapes (>2 declines
        "polymorphic"; every shape must pass the full exactness screen
        AND carry the accessed field — criterion 2, no near-misses;
        structural duplicates dedupe to mono), and propGet/propSet
        lower a guard CHAIN — the second has_shape tests on the first's
        miss edge, so each fast arm sits under its own same-block-fresh
        fact and the verifier's P4.3/P4.5 rules apply per arm unchanged
        (typed f64 arms box at their own exits; stores split has_tag
        per arm, oriented by that arm's field repr).  The mono path
        emits byte-identical IR to P4.5.  The optimizer's region/fold
        machinery is mono-strict and refuses chains wholesale (pinned:
        4 guards survive `p.x + p.x` un-merged, module re-verifies) —
        chain-aware merging is future measured work, and wall time
        says it can wait.  `EJS_NO_POLY_SHAPE_GUARDS=1` is the bisect
        hook (2-shape sites decline "polymorphic" exactly as before);
        telemetry grows `shapePolyGuards=N` (additive).  **Measured**
        (M-series, types-bench3 = the bench2 kernel with alternating
        receivers): chain **0.31s — parity with the monomorphic twin
        (0.32s)** — vs 1.67s declined (the bisect flag) and 3.64s
        flag-off: **5.4×** for the chain over the decline, and the
        pre-P4.6 false-mono world's 0.99s (half the receivers missing
        the guard) is beaten 3.2×.  Probe types-poly1 (both arms fast,
        typed stores per arm; cross-module repr-mismatched / third-
        shape / dictionary receivers all through the shared slow path)
        is identical across --types/flag-off/EJS_SHAPES=off/gc-stress.
      - **Accessor inlining: DECLINED, evidence recorded.**  The probe
        (defineProperty proto getter, 20M dispatches — getter LITERALS
        are still a maam NormalizeError) measures 2.31s under --types
        vs 5.44s flag-off; the same arithmetic through P4.3 guarded
        slots runs 0.32s, so ~7× headroom exists.  But a receiver
        has_shape proves NOTHING about the proto that carries the
        getter (accessor-bearing protos are dictionary-mode by P4.2
        design — mutable maps), so sound inlining needs proto-identity
        /proto-shape guard machinery plus maam-side accessor modeling
        that does not exist.  That is new soundness surface, not a
        measured extension; revisit as its own designed phase.
      - **Pretenuring hooks: DEFERRED — no consumer.**  The
        generational mover (gc-P2+) is not built; there is no nursery/
        tenured split for an oracle hint to steer.  gc-plan owns it.
      - **Array element shapes: DEFERRED, evidence recorded.**  The
        element-kernel probe (64-element dense f64 array, 20M reads):
        0.57s under --types vs 1.38s flag-off vs node 0.06s.  Real
        headroom, but arrays are exotics outside shaped mode by scope
        (P4.x is plain objects), maam smashes element types, and typed
        element storage is its own runtime subsystem — routed to a
        future phase alongside the gc-plan storage work.
      *Gate results (2026-07-24):* matrix ×7 green (test-eir + 7 new
      poly unit tests incl. the optimizer-refusal pin, lowtier, stages
      0-3, `//:test-stage1-shapes-off`); --types diff lane
      **0-divergent** (476 files, 475 identical, 1 N/A = tester.js;
      suite telemetry: 13,323 sites, 865 guarded of which
      **shapePolyGuards=25** — poly chains fire in real suite files
      (eir-syntax4, shapes-storm1), not just the probes; declines:
      unmapped 7,705 / capped 4,263 / empty 272 / no-field 199 /
      union-repr 16 / polymorphic **3** — down from 12: the survivors
      are genuine >2-shape sites, and the old count was an undercount
      built on the false-mono maam reports).  types-bench2 (mono world)
      regression-checked bit-identical stats/output/wall-time.

P4.1/P4.2 are pure runtime and can proceed independently of maam; P4.3+
are compiler phases in the P3 mold.  gc-P1 and P4.1 share one atomic
layout change whichever lands first.

## Risks, named

- **Dual-bookkeeping overhead (P4.1)** on shape-oblivious programs: one
  transition-cache hit per property add, on every program.  Measured at
  the P4.1 gate with a hard <5% bar; the mitigation is that the
  transition cache is one hash hit against an interned table vs the
  map's existing hash+chain work, and P4.2 deletes the duplication.
- **Shape explosion from type-aware transitions.**  maam's answer (caps
  → megamorphic ⊤) transplants: per-object transition caps → dictionary,
  global table growth monitored; splay is the canary.  Order-sensitive
  runtime shapes intern more than maam's order-insensitive ones — the
  order-canonicalization trick is NOT available at runtime (enumeration
  order is semantics); the census (P4.1 gate) tells us the real fanout
  before any compiler work depends on it.
- **The effect-kill soundness class (P4.3).**  Shape facts die at
  WRITE|CALL effects; a missed kill is a silent miscompile of exactly the
  kind P3.4's adversarial review kept finding.  It gets the same
  treatment: a written soundness inventory in optimize-guards, hand-built
  attack IR in the unit tests, and the review loop before promotion.
- **Semantic fidelity of shaped mode.**  Enumeration order, `in` during
  construction, delete-readd patterns, freeze/seal, accessor conversion
  — each has a dictionary-migration answer, and each needs a probe.  The
  runtime differential mode (EJS_SHAPES=off) is the backstop that turns
  any miss into a visible diff instead of a shipped bug.
- **Cross-module shape identity** rests on module-init interning
  (atom-table precedent).  A module compiled against different oracle
  facts than its neighbor still agrees on runtime shapes (they're
  interned by structure, not by compile-time claim) — guards just fail
  more often; correctness is untouched.  The IR-in-manifest future
  (cross-module oracle facts) only widens what qualifies.
- **Header layout coupling with gc-P1.**  One layout, one atomic change,
  both plans reviewed against it (gc-plan.md:317-320 owns the rule; this
  doc's Step A is written to it).

## Alternatives considered

- **Structure-only shapes (V8-classic), representation checked per
  access.**  Cheaper transitions, but every typed load keeps a
  `has_tag`+unbox and every guard proves less; maam already pays for
  type-aware classes and P3 built the raw-f64 world this feeds.  The
  premium of type-aware transitions is measured at P4.1 (census) before
  P4.3 commits — if type-flip churn is pathological in real code, reprs
  can degrade to `boxed` per-field without changing the design.
- **Inline caches / PICs without static shapes.**  A JIT's answer; AOT
  echojs has no code patching and DOES have an oracle.  Module-init-
  interned guard globals ARE the static IC.  Runtime-fed feedback could
  come later via manifests; not this phase.
- **Per-class C structs from `layouts()` (full monomorphization, no
  guards).**  The seductive shortcut — and exactly the unguarded leap
  the P3 ladder exists to prevent.  Everything unguarded here rides
  behind fences and the harness, or doesn't ship.
- **Deprecation/migration (V8's in-place repr rewrites).**  Requires
  patching compiled offsets; AOT has no second chance — this is why
  reprs are in the class identity, per maam's own design note.

## Open questions (tracked, not blocking P4.1/P4.2)

1. **Ordered-shape witnesses from maam for constructors.**  RESOLVED at
   P4.3: maam's ShapeTable records each class's first-interning
   insertion order (`fieldOrderOfShape`) — first-write program order
   along the first analyzed path, for literals AND constructors alike.
   A runtime object built in a different order interns a different
   runtime shape and simply misses the guard (slow path, never wrong).
   P4.4's born-with-shape constructors may still prefer the fence's
   straight-line store prefix as the witness; decide there.
2. **Slot-array growth policy** (size classes vs exact +
   copy-on-transition) — informed by the P4.1 census.
3. **How much of `Array`/`Function`/module exotics join shaped mode
   later** — out of scope for P4.x entirely; plain objects first.
4. **`repr` lattice granularity** (`f64`/`boxed` vs finer `bool`/`str`
   tags) — start minimal; the census + types-bench2 decide.

## Coordination

- **gc-plan.md**: Phase 1 header bits (joint, atomic), Phase 2 inline
  allocation (born-shaped literals become bump-alloc clients), Phase 5
  (consumes shapes for tracing/evacuation; this doc's Step B).
- **maam-plan.md**: P4 checklist ticks "design doc" with this document;
  P4.1+ items live HERE (this doc is the phase's checklist owner, the
  gc-plan pattern).  The differential-harness shapes lane extends the
  P3.5 asset in the maam repo.
- **plans.md escape analysis / allocation sinking**: sinking deletes
  allocations shapes would otherwise accelerate — run the P4.1 census
  with the optimizer ON (the gc-P0 lesson).

## Phase checklist (for /goal sessions)

- [x] **P4.1** runtime shape table + tracking, dual bookkeeping, header
      bits (joint with gc-P1), EJS_SHAPES=off, census instrumentation.
      Gate: matrix ×3, off-mode diff, <5% insert overhead, census
      recorded.  DONE 2026-07-23 — see the phased-plan entry above for
      the numbers (2.1% insert overhead via the inlined transition
      memo).
- [x] **P4.2** slot storage + dictionary migration, specops mode-switch.
      Gate: both-modes byte-identical suite+kangax, stress green,
      microbench recorded.  DONE 2026-07-24 — see the phased-plan entry
      above (set 3.2×, get 1.09×, insert -3%; storm probe + gc-stress
      green both modes; no in-repo kangax, suite+probe stand in; NOTE
      the stage2 GC lesson recorded there: shaped field cap 14 keeps
      slot arrays out of the LOS, and the gc trigger now scales with
      heap footprint).
- [x] **P4.3** EIR ops + verifier inventory + emitter + maam
      node-identity queries + guarded diamonds + shape facts in
      optimize-guards.  Gate: matrix, lane 0-divergent, wrong-oracle
      probes, unit tests, types-bench2 delta.  DONE 2026-07-24 — see the
      phased-plan entry above (types-bench2 2.1×, lane 459 files
      0-divergent, all attack IR pinned at unit level).
- [x] **P4.4** born-with-shape (literals unconditional; constructors
      fenced).  HARD PRECONDITION: harness shapes lane.  Gate: harness +
      lane + probes + delta.  DONE 2026-07-24 — see the phased-plan
      entry (harness shapes lane green, types-bench2 3.06s → 2.03s,
      ctor batching = the empty-shape-guarded body-side fill; no maam
      constructor query needed).
- [x] **P4.5** typed slots × clones × gc-P5 consumption (compiler half;
      gc-P5 consumption waits on the mover).  Gate: typed delta measured
      and recorded, all lanes green.  DONE 2026-07-24 — see the
      phased-plan entry above (raw f64 slot ops + heterogeneous region
      fusion; bench2 total unchanged at 2.04s because the residual is
      the alloc loop; invariant-receiver kernels now constant-fold;
      guarded path at parity with trusted clones).
- [x] **P4.6** measured extensions — evidence-gated, all four candidates
      probed and measured.  DONE 2026-07-24: 2-way poly guard chains
      LANDED (kernel 5.4× vs decline, mono parity; required the maam
      per-object terminal-filter fix — the false-mono finding); accessor
      inlining declined (7× headroom recorded, blocked on proto-guard
      soundness machinery); pretenuring deferred (no mover yet — gc-plan
      owns it); array element shapes deferred (numbers recorded; arrays
      are outside shaped mode by scope).  See the phased-plan entry.
