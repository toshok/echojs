# sinking-plan: escape analysis + allocation sinking

Bucket plan; the ordering spine lives in `docs/plans.md`.  Phase ids
here are `sinking-P#` (formerly S1/S2/S3 in this doc's first
revision).

Status: sinking-P1 LANDED (2026-07-25) — see "sinking-P1 results" at the bottom.  Owner doc for extending escape analysis +
allocation sinking (docs/plans.md, optimization phase, first bullet)
past what already exists.  Written 2026-07-25, after gc-P2.

## Where we actually are

The plans.md ladder is further along than its checkbox suggests:

- **Rung 1 (`make_env`)** — landed.  `scalarReplaceEnvs`
  (lib/eir/optimize.ts) scalar-replaces non-escaping closure
  environments; the EIR inliner (`inlineDirectCalls`) exposes IIFE envs
  to it.
- **Rung 2 (`make_object`/`make_array` + own-key folding)** — landed
  *for the unshaped ops*.  `sinkAlloc` (optimize.ts:212) folds own-key
  `get_prop_atom` / const-index `get_prop` / `.length` reads to the
  allocation's operands and deletes write-only allocations.
- **Rung 3 (iterator-wrapper peephole)** — landed
  (`foldIteratorWrappers`): dense-array destructuring walks fold to
  direct element reads.
- **Rung 4 (`rest_args`/`args_obj`)** — not started (unchanged).

What broke the ladder: **shapes**.  Under `--types`, every
statically-keyed literal lowers to `make_object_shaped` (P4.4
born-with-shape), and every oracle-typed property read lowers to a
`has_shape` diamond (`slot_load` fast arm, `get_prop_atom` slow arm).
`sinkAlloc` matches neither op, so in exactly the compiles where
performance matters, rung 2 no longer fires.  Constructor results
(`construct` of a born-shaped ctor) were never covered by any rung.

Measured stake (types-bench2, 2026-07-25, nursery default-on): 0.64 s
vs node's 0.06 s warm.  The `alloc()` loop allocates 4M Points × (1
wrapper object + 1 slot-array env) plus fill and guard dispatch, all of
it provably dead — node deletes the allocation outright via escape
analysis + scalar replacement.  This phase rebuilds that ability for
the shaped world.

## Design

### sinking-P1 — shaped-literal sinking (statically sound)

Extend `sinkAllocations` to `make_object_shaped` candidates.  A shaped
allocation's shape is an immediate (`imms.shape` keyed into
`Module.shapes`) and its operands are the field values in shape order,
boxed — there are no separate initializing stores.  Use classification
(fail-closed, mirroring `classifyUses`):

- `has_shape(o, S)` whose **only** consumer is its block's `cond_br` —
  a guard, resolvable statically (below);
- `slot_load(o, S=alloc shape, slot=k)` in base position — own read;
- `get_prop_atom(o, atom)` in base position — own read iff `atom`
  names a shape field, else a prototype read (unfoldable, blocks
  removal, same as unshaped);
- **anything else escapes** — including every write (`slot_store`,
  `set_prop_atom`), edge args, call/return/throw operands, value
  positions, `get_prop` computed reads (v1 keeps writes out entirely;
  the unshaped pass's flow-insensitive written-atom skip doesn't carry
  over because a write would also invalidate guard folding).

**Guard resolution.**  For a non-escaping, never-written shaped
allocation the birth shape is invariant for the object's whole
lifetime — nothing else can transition it, so the verifier's WRITE|CALL
kill inventory (which models *other* code mutating the receiver) does
not apply.  `has_shape(o, S)`:

- `S ≠ birth shape` → fold false (branch to the false edge).
- `S = birth shape` → fold **true only if every f64-repr field's
  operand is provably a number** (a `box_f64` or a number `const`);
  otherwise fold **false**.  Both directions are sound: the fast and
  slow arms of a shape diamond are twins computing the same value, so
  routing to the generic arm never changes semantics — and the folded
  reads collapse to the same operand either way.  The repr condition
  exists because folding true exposes `slot_load repr=f64`, whose
  result we fold to the *raw* source of the operand's `box_f64`;
  feeding that from a non-number would manufacture garbage bits.  (The
  runtime enforces the same invariant dynamically: `fill/make_shaped`
  re-derive the true shape from actual values, so a lying-repr operand
  makes the runtime object's shape differ from the static key — the
  fold-false route is the static mirror of that re-derivation.)

Folding a guard = `condBrToBr` + drop the now-unused `has_shape`
(pure); `sweepUnreachableBlocks` reclaims the dead arm.  Both helpers
already exist in optimize-guards.ts.

**Read folding.**  `slot_load slot=k repr=f64` → the operand of the
field value's `box_f64` (raw f64, type-preserving — verifier needs no
change); `repr=boxed` → the operand itself.  `get_prop_atom` for field
`name` → the operand (boxed, type-preserving).  Removal: when no uses
remain, delete the alloc; `removableWhenDead` gains the shaped ops
next to the existing `make_object`/`make_array` own-storage exemption.

**Semantics note (define vs set).**  Sinking a literal assumes its
field initialization is unobservable.  Literal keys are define-
semantics per ES; the current runtime's shaped fallback uses setprop-
on-fresh, equivalent for every key the shaped lowering admits
(`__proto__` and computed keys are already excluded).  This is the
same judgment the existing `make_object` sinking made; the
differential lane arbitrates.

**Pass placement.**  Inside the existing main fixpoint (round-robin
with inlining/env-replacement), i.e. *before* `optimizeShapeRegions` —
sinking sees per-read diamonds, never merged regions.  Clones from
P3.6 specialization get their shot in the post-specialize
`optimizeModule` round.  Bisect: `EJS_NO_SHAPED_SINK` (the
`EJS_NO_EIR_OPT` mold).  Telemetry: `shape_allocs_sunk` +
`shape_guards_sunk` on the `EIR-opt:` line.

### sinking-P2 — constructor-result sinking (needs a runtime contract; NOT static)

The bench2 alloc loop is `new Point(i, i+1)` — a `construct` of a
module-local born-shaped ctor.  The tempting rewrite (virtualize the
result: field k = argument k, delete the construct) is **unsound as a
static transform**, and the reason deserves recording:

> Constructor body stores are `[[Set]]` semantics.  A setter installed
> on `Point.prototype` — reachable from *any* escaped instance via
> `Object.getPrototypeOf` — must intercept `this.x = x` in every later
> construction.  Deleting the store deletes the interception.  This is
> exactly why P4.4's born-with-shape kept the stores and guarded the
> batched fill with a runtime `shaped_proto_intercepts` check rather
> than eliding anything.  Object literals don't have this problem
> (define semantics), which is why sinking-P1 is static and sinking-P2 is not.

Sound path (designed here, sequenced after sinking-P1): **epoch-guarded
sinking** — the deopt-free analogue of V8's speculative escape
analysis.  The runtime maintains a global accessor epoch
(`_ejs_accessor_epoch`, bumped whenever an accessor property is
installed on any object — defineProperty/defineProperties/
`__defineGetter__`/`__defineSetter__`/class accessor evaluation — and
on `setPrototypeOf`/`__proto__` writes).  A sunk construct site
compiles to:

    %e = epoch_check epoch=<compile-time constant 0-state>   ; load+cmp
    cond_br %e -> virtual arm (no allocation, fields = args),
                  slow arm (the original construct)

The guard is one load + compare against the epoch observed at module
init; the sunk arm saves two allocations, the fill, and the field-read
dispatch.  Accessor installation is rare in the corpus (P4.1 census:
builtin-init dominated) but *not absent* — the epoch must be sampled
after builtin/module init, or kept per-shape-lineage.  Additional sinking-P2
conditions, all fail-closed:

- ctor resolves through the P3.6 promoted-`%self`-slot machinery to a
  module-local `make_closure` whose function passes the P4.4 fence
  *and* whose body is exactly the guarded fill + `return undefined`
  (any trailing code declines);
- fill operands are exactly the formals, in order (computed field
  values would require real inlining — decline in v1);
- construct-site argument count equals formal count (missing-argument
  `undefined` would change the runtime-derived shape);
- result non-escaping under the sinking-P1 classifier;
- all-or-nothing per site: partial folding with a surviving construct
  is unsound (the surviving execution may be intercepted, diverging
  from folded reads).

sinking-P2 touches runtime (epoch maintenance), lowering (epoch_check op or a
call_runtime), and the optimizer; it is its own gated step with its
own differential evidence.  Until then `new`-heavy loops keep their
allocations — gc-P2's nursery makes that a bump-pointer + minor-GC
cost rather than a free-list cost, which is the composition the two
plans always intended.

### sinking-P3 — recorded, not scheduled

- Flow-sensitive field writes on sunk objects (SSA renaming per field;
  today any write declines the candidate).
- Partial escapes / materialization points (allocate lazily on the
  escaping path only) — subsumes the "options object passed onward
  sometimes" pattern.
- `rest_args`/`args_obj` when only indexed or `.length`'d (plans.md
  rung 4).
- Cross-function sinking via inlining heuristics beyond the current
  single-block IIFE inliner (a multi-block inliner would let sinking-P2's
  "fill operands are formals" restriction relax to arbitrary ctor
  prefixes).

## Gates

sinking-P1: unit tests (fold + refusal attacks: escaping uses, written
fields, wrong-shape guards, non-number f64 operands folding false,
prototype reads blocking removal, `===` identity, typeof); the
existing suite byte-identical under `EJS_NO_SHAPED_SINK` vs default
for flag-off compiles (shaped ops only exist under --types); types
diff lane 0-divergent; matrix ×7; telemetry counts on the suite
recorded here; probe types-sink1 node-identical incl. EJS_SHAPES=off
and gc-stress.  Perf: a shaped-literal kernel (sink-probe2-style)
should reduce to pure arithmetic — verify via `--dump-after eir-opt`
and wall time.

sinking-P2 (when built): everything above plus epoch-bump coverage tests
(accessor installed mid-loop → slow arm taken from that iteration on),
and types-bench2 as the phase bench — target is the alloc() loop at
kern parity (~0.3 s total, from 0.64 s).

## sinking-P1 results (2026-07-25)

Implementation: `sinkShapedAlloc` in lib/eir/optimize.ts, wired into
the existing `sinkAllocations` under the main fixpoint; guard branches
resolve via `condBrToBr` + `sweepUnreachableBlocks` (now exported from
optimize-guards.ts and swept each fixpoint round); shaped allocs join
the own-storage DCE exemption, and a shaped alloc reaching DCE counts
as the sink completing (`shape_allocs_sunk` / `shape_guards_sunk` on
the `EIR-opt:` stats line).  Bisect: `EJS_NO_SHAPED_SINK`.

The canonical reduction (types-sink2 kernel, `--dump-after eir-opt`):
`f$typed(a) { var o = {a: n, b: n+1}; return o.a + o.b }` compiles to
two `f64_add`s and a return — allocation, guards, boxes, and slot
loads all gone; the raw-join machinery (P3.4/P4.5) carries the folded
operands through the emptied diamond joins.

Note on repr provability in practice: unit-lowered IR feeds field
values as raw params (never `box_f64`), so guards there resolve to the
generic arm — reads still fold to the same operands and the alloc
still drains; in real compiles the specialized clones box their
formals, guards resolve true, and the raw path folds.  Both routes
were pinned by tests.

Gate evidence: 187 EIR unit tests green (8 new: full sink, escape /
call-operand / write / prototype-read / wrong-shape / hand-built
unprovable-repr refusals, bisect hook); --types diff lane 485
identical / 0 divergent / 1 N/A over 486 files (including new suite
tests types-sink1/2, node-identical); matrix ×7 green; flag-off
lowering unchanged (shaped ops only exist under --types; the
unreachable-block sweep now also prunes builder-era dead blocks in
flag-off compiles — semantically inert, LLVM dropped them anyway).
types-bench2 unchanged at 0.65 s as predicted (its allocations are the
sinking-P2 constructor case); the sinking-P1 payoff lands on non-escaping literal
patterns — destructuring returns, options objects — throughout the
suite and the compiler itself.
