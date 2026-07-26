# sinking-plan: escape analysis + allocation sinking

Bucket plan; the ordering spine lives in `docs/plans.md`.  Phase ids
here are `sinking-P#` (formerly S1/S2/S3 in this doc's first
revision).

Status: sinking-P1 LANDED (2026-07-25), sinking-P2 LANDED (2026-07-25),
sinking-P3 LANDED (2026-07-25) — see the results sections at the
bottom.  Owner doc for extending
escape analysis + allocation sinking (docs/plans.md, optimization
phase, first bullet) past what already exists.  Written 2026-07-25,
after gc-P2.

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

### sinking-P3 — flow-sensitive writes, partial escapes, args (P5.3)

Design written 2026-07-25, scoped by two investigations recorded here
so the judgments survive:

**(a) Flow-sensitive field writes** (`lib/eir/sink-flow.ts`).  Lifts
the "any write declines" rule for `make_object` and
`make_object_shaped` candidates (arrays keep the length-write decline;
element writes can't reach a literal anyway).  Two structural facts
make this cheap:

- *Write diamonds are twins.*  `propSet` lowers `o.f = v` to a
  has_shape diamond whose fast arm slot_stores (a possibly-unboxed) `v`
  and whose slow arm set_prop_atoms the same `v` — both arms store the
  same source value, so the after-join tracked value is just `v`.
  Field phis are needed only at REAL control joins (if/else writing
  different values, loop headers), never per diamond.
- *Folding a shape guard FALSE is unconditionally sound* (the P1 twin
  argument), independent of writes.  A written candidate folds every
  foldable guard false and resolves everything through the generic
  arms; the memory ops then vanish entirely, so nothing is lost by
  skipping the typed arms — the post-fixpoint rawJoin/guard-region
  passes recover raw f64 flow on the *values* (which is where the
  arithmetic lives once the object is gone).  This avoids the
  optimistic repr-invariance simulation folding TRUE would require
  under writes (a set_prop_atom storing a non-number into an f64 field
  repr-transitions the runtime shape).

The pass is all-or-nothing per candidate (the ctor-sink discipline):
every use must be a foldable target-less read (own-key
get_prop_atom / const-index get_prop on objects), a deletable
target-less own-key write (set_prop_atom naming a literal key / shape
field — [[Set]] to an own writable data property on an unaliased
object is unobservable, the P1 semantics-note judgment; *non-own-key
writes decline*: a key-adding [[Set]] walks the prototype chain and is
only epoch-guardable, recorded below), a foldable guard, or (mode b)
the single escape.  slot_stores are classified as pending writes in
round 1; guard fold-false unreaches them and the sweep removes them
before flow resolution — one surviving to the resolution round
(hand-built IR only) declines.  Reaching values are computed per field
with a Braun-style renamer over the complete CFG (the builder's
algorithm, minus lazy sealing), minting boxed block params at joins;
plan-before-apply screens decline candidates whose walk region touches
catch blocks (unwind edges never carry the tracked value).  A
slot_store's tracked value strips the store's `unbox_f64` (sound: the
diamond's has_tag proved numberness on that arm, so box(unbox(v)) is
v); reads fold to the reaching value at their program point.  Bisect:
`EJS_NO_FLOW_SINK`.  Telemetry: `flow_allocs_sunk` on the `EIR-opt:`
line.

**(b) Partial escapes / materialization.**  The same pass, one escape
allowed: a candidate whose non-read/write/guard uses are exactly ONE
instruction E materializes the object immediately before E (a fresh
`make_object`/`make_object_shaped` of the reaching field values —
the runtime re-derives the true shape from actual values, so
tracked-write repr drift is immaterial) and substitutes it into E's
operands/edge-args.  Fail-closed screens, each with a recorded reason:

- *No use reachable from E* (forward CFG walk from after-E, treating
  entry into the alloc's block as a fresh-activation barrier): a read
  after the escape would miss external mutations through the alias.
- *The same walk finding E again declines* (at-most-once per
  activation): two materializations of one abstract object would split
  its identity.
- At least one read folded or write deleted (else the rewrite is
  churn — `return {…}` directly is already optimal).
- Own-key writes only, exactly as in (a).

Identity/typeof/=== against the materialized object are correct by
construction: it IS the object, created at its last-possible point.

**(c) `rest_args`/`args_obj` — length folds land; index folds
DECLINED.**  Evidence from the runtime (2026-07-25):

- `_ejs_arguments_new` COPIES argv (ejs-arguments.c:62) and is
  unmapped; `.length` is synthesized from argc on every get;
  callee/caller are poison accessors.  `_ejs_array_new_copy` copies.
  So `.length` of either object is exactly a function of the immutable
  argc — foldable to a new `arg_len` op (imms.index; boxed
  `max(argc - index, 0)`; effect NONE; emitted from the raw argc
  calling-convention value, the rest_args precedent).
- Late argv reads WOULD be GC-safe (the conservative whole-stack scan
  still covers the caller's args scratch and pins win over evacuation
  — ejs-gc.c:1982-1989 — and generator bodies never see caller argv:
  the desugar materializes arguments/rest in the outer function, so
  they reach the body through env capture, which classifies as an
  escape and declines).  But an out-of-bounds `arguments[k]`/`rest[k]`
  read falls through to the ordinary get path — the prototype chain —
  and writable INTEGER DATA properties on Array.prototype /
  Object.prototype do not bump `_ejs_accessor_epoch`
  (ejs-object.c:2594's screen covers accessor/non-writable defines and
  setPrototypeOf only).  A sound `arg_load` therefore needs either a
  new proto-index epoch class in the runtime or an epoch-guarded
  region with an OOB helper (receiver-free data-prop lookup is only
  sound while the accessor epoch is 0).  Corpus census: const-index
  arguments reads are rare and co-occur with uses that decline anyway
  (iteration, aliasing tests); the recurring foldable pattern is
  arity-check `.length`.  Decision: implement `arg_len` only; record
  `arg_load` here as declined-with-design until a workload justifies
  the runtime extension.

`arg_len` joins the inliner's and specializer's frame-op screens
(FRAME_OPS / CLONE_FRAME_OPS — it consumes the raw argc, which
neither an inlined body nor a specialized clone carries; clones can
never contain a minted arg_len since functions using arguments/rest
are never cloned, but the screens keep the invariant explicit).  The
sink itself: a rest_args/args_obj whose every use is a target-less
`get_prop_atom "length"` folds those reads to `arg_len` and removes
the allocation in-pass (args_obj's THROW effect keeps it out of
generic DCE deliberately — the pass, having proven all uses folded,
removes it explicitly).  Any other use — writes, computed reads,
`Symbol.iterator`, callee — declines.  Fires on flag-off compiles too
(like the unshaped sink); the stage matrix is the gate.  Bisect:
`EJS_NO_ARGS_SINK`; telemetry: `args_sunk`.

**Still recorded, not scheduled** (sinking-P4 material):

- Key-ADDING writes on sunk objects (epoch-guarded; subsumes the
  `var o = {}; o.a = …` builder pattern under --types, where the
  literal's birth shape lacks the written key).
- `arg_load` per the design above.
- Cross-block env scalar replacement (the same Braun machinery over
  env slots; today `scalarReplaceEnvs` is same-block only) — belongs
  with compiler-P1's SSA cleanups.
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

sinking-P3: unit tests per feature with refusal attacks (non-own-key
write, use-after-escape, escape-in-loop-without-alloc, two escapes,
catch-block join, surviving slot_store, computed read on args, write
to rest, bisect hooks); semantic probes node-identical incl.
`EJS_SHAPES=off`, gc-stress (`EJS_GC_EVERY_N_ALLOC=101`), and
flag-compiled (`EJS_NO_FLOW_SINK` / `EJS_NO_ARGS_SINK`) exes —
probes must cover write-then-read-across-branches, loop accumulator
objects, escape-site identity (`===`, mutation through the escaped
alias), and arguments-length arity dispatch; --types diff lane
0-divergent; matrix ×7 (args/flow sinking fire flag-off, so the stage
lanes carry real weight here); a flow-sink loop-accumulator kernel as
the phase bench, A/B vs `EJS_NO_FLOW_SINK`.

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

## sinking-P2 results (2026-07-25)

Implementation, in the three pieces the design called for:

- **Runtime** (`_ejs_accessor_epoch`, ejs-object.{h,c}): one global
  counter, `== 0` meaning "no user code has installed anything that
  could intercept a [[Set]] through a fresh object's prototype chain".
  Bumps at the ordinary `DefineOwnProperty` specop for accessor
  descriptors and `writable:false` data descriptors, and at both
  `SetPrototypeOf` implementations (ordinary + proxy trap); zeroed at
  the end of `_ejs_init` so builtin installs never count (the only
  builtin accessor on a fresh ordinary chain is `__proto__`, a name the
  ctor fence never admits).  **The screen that made it viable: only
  defines on ORDINARY receivers bump.**  A virtualized instance's chain
  is `ctor.prototype → Object.prototype`, both ordinary, and any other
  object can only join such a chain through a bumping setPrototypeOf or
  a statically-declined prototype swap — without the screen, every
  closure's non-writable name/length and every module's export
  accessors killed the epoch at startup (found by lldb watchpoint on
  the first bench run: `_ejs_function_new` at module init).
- **EIR** `epoch_check` op (arity 0, READ, i1): emitted as one load of
  the global + compare-to-zero (`emitAccessorEpochCheck`, the
  `_ejs_heap` global-seam precedent).  No verifier change — the op
  table's sig covers it.
- **Optimizer** (`lib/eir/sink-construct.ts`, module pass after
  specialization in integrate.ts): resolves construct callees through
  the promoted-`%self`-slot discipline (single closure store,
  prefix-safe or store-dominated, **every load of the slot used only as
  a call/construct callee — which also closes the `Point.prototype = X`
  replacement hole statically**, so exotic protos need a bumping
  setPrototypeOf); structurally matches the ctor body as exactly the
  P4.4 guarded fill of the formals plus `return undefined`; requires
  argc == formal count and the sinking-P1 use classification on the
  result; computes the single-entry single-exit acyclic use region;
  runs a fold simulation (the sinkShapedAlloc guard rule) proving every
  use folds or dies unreachable — the all-or-nothing guarantee that the
  virtual arm's allocation always drains.  The rewrite splits at the
  construct, closes the head with `epoch_check` + cond_br, keeps the
  original region as the slow arm, and clones the region with the
  construct replaced by `make_object_shaped(args)`; region-defined
  values used past the exit cross through minted join params (rawJoin
  for f64).  The existing shaped-literal sink then drains the clone in
  the post-sink optimizer round.  Bisect: `EJS_NO_CTOR_SINK`;
  telemetry: `ctorSunk=N` on the `--types:` line, `EIR-ctor-sink` debug
  line.

Gate evidence (all green, 2026-07-25):

- 192 EIR unit tests (5 new `sink-ctor`: full sink, live-outs across
  the epoch join, six refusal attacks in one sweep — second store /
  prototype-touching load / trailing ctor code / swapped fill operands
  / argc mismatch / escaping result — non-promoted slot, bisect hook).
- Probes `types-ctorsink1` (epoch coverage: clean run, accessor
  installed mid-loop through Object.prototype at i=5, then a
  non-writable data property mid-loop — slow arm and interception from
  that iteration on) and `types-ctorsink2` (pure-win kernel + escape
  decline + prototype-method decline): node-identical, including under
  `EJS_SHAPES=off`, `EJS_GC_EVERY_N_ALLOC=101`, and an
  `EJS_NO_CTOR_SINK` compile.
- `--types` diff lane: 493 files, 492 identical, 0 divergent, 1 N/A
  (tester.js, standing).  ctorSunk fires in types-bench2 (2),
  types-sink1 (2), and the two new probes — everywhere else the
  fail-closed screens decline.
- Matrix ×7 green (test-eir, lowtier, stages 0-3 at 419 pass /
  22 standing xfail each, shapes-off lane).
- **types-bench2: 0.70s → 0.26s wall (warm, A/B vs EJS_NO_CTOR_SINK
  exes from the same tree); allocations 4,000,501 objects + 4,000,061
  envs → 501 + 61 (EJS_GC_PROFILE).**  The alloc() loop is
  allocation-free — better than the ~0.3s phase target; the residual
  0.26s is kern.

What the sunk loop still pays per iteration: one epoch load+compare,
one `%self` slot load of the ctor (kept live by the slow arm), and two
generic `add` calls (the oracle doesn't type s + p.x, so those adds
never had diamonds) — all noise next to the construct it replaced.
Recorded for later phases: slot-load licm and add-diamond coverage
would shave the rest.

## sinking-P3 results (2026-07-25)

Implementation, per the design above:

- **Flow pass** (`lib/eir/sink-flow.ts`): planOne (classify + all
  screens, zero mutation) → applyPlan (fold guards false, sweep,
  Braun-rename per field with minted boxed join params + trivial-param
  removal, fold reads, materialize at the single escape, delete writes
  + alloc).  Runs last in the optimizeFunction fixpoint round with its
  own use scan, one rewrite per invocation.  Bisect:
  `EJS_NO_FLOW_SINK`; telemetry `flow_allocs_sunk` /
  `allocs_materialized` (guard folds count into `shape_guards_sunk`).
- **Args sinking** (`sinkArgsObjects` in optimize.ts): new `arg_len`
  op (emitted as a call to the new pure `_ejs_arg_length(argc, index)`
  runtime helper — node-llvm has no SIToFP binding, so the int→boxed
  conversion lives in C); rest_args/args_obj whose every use is a
  target-less `.length` read fold and are removed in-pass.  `arg_len`
  joined FRAME_OPS and CLONE_FRAME_OPS.  Bisect: `EJS_NO_ARGS_SINK`;
  telemetry `args_sunk`.  `arg_load` declined per the design section
  (OOB prototype-read hazard uncovered by the epoch; census: rare).
- Two renamer bugs found by the stage1 self-compile, both worth
  remembering: (1) the trivial-param scan judged a MID-FILL param
  (`[null, X]` read as all-equal-X) — unfilled slots now decline
  judgment; (2) a recursion frame's captured param could be forwarded
  by a nested trivial-param cascade before installation (its
  replaceAllUses runs too early to see the use) — a `forwarded` map +
  `resolve()` at every install point closes it.

Self-compile cost, and what it taught (the stage2 build initially ran
~2× slow; each finding below is now in the code):

- **Never read process.env in the fixpoint** — under the self-hosted
  runtime it is a rebuild-the-environment getter.  All sink bisect
  flags are read once per optimizeFunction (`SinkFlags`), which also
  hoisted the pre-existing per-round `EJS_NO_SHAPED_SINK` read.
- **One scan per round** — the driver's `scanRound` gathers the use
  map AND every sink pass's candidate list in a single `forEachInst`
  walk; the flow pass consumes the shared map (type-only imports keep
  optimize↔sink-flow acyclic at runtime) and does no scans of its own.
- **FLOW_REGION_CAP (32 blocks)** — sinking spreads field values
  across the rename region as live SSA values, so a function-spanning
  region trades one heap object for many long-lived gc-frame slots:
  flow-sinking esprima's `scanPunctuator` token literal measurably
  worsened every minor GC's conservative pin scan during parses.
  Small regions (loop accumulators, builder tails) keep the win; the
  self-compile census after the cap is 3 sites → 0–1 per big module.
- The remaining ~1.5–2× stage-self-compile wall delta is NOT the
  passes (it persists with both bisect flags set): it is a
  pre-existing, mmap-layout-bistable conservative-pin-scan cliff that
  any allocation-pattern change (+1.4% allocs here) can tip — fully
  root-caused and recorded as gc-P4's first order of business in
  gc-plan.md, with a partial mitigation (the LOS bounds prefilter,
  ejs-gc.c) landed in this phase.

Gate evidence (all green, 2026-07-25):

- 205 EIR unit tests (new: sink-flow ×8 — cross-branch phi, loop
  accumulator, read-before-write, escape materialization, five-way
  refusal sweep, catch-region decline, shaped partial escape, bisect
  hook; sink-args ×4 — arguments/rest length folds, four-way refusal
  sweep, bisect hook; the two sinking-P1-era "writes decline" pins now
  assert the flow-sunk behavior with EJS_NO_FLOW_SINK variants
  pinning the old decline).
- Probes `test/types-flowsink1.js` (branches, loop accumulator,
  read-before-write, escape identity + mutation-through-alias, fresh
  object per loop iteration, key-adding decline, try-write decline,
  self-reference decline, and an Object.prototype setter intercepting
  the declined key-adding write) and `test/types-argsink1.js`
  (length-only folds incl. rest start index, computed-read /
  forwarding / arrow-capture / generator declines): node-identical
  under --types, flag-off, `EJS_SHAPES=off`,
  `EJS_GC_EVERY_N_ALLOC=101`, and `EJS_NO_FLOW_SINK` /
  `EJS_NO_ARGS_SINK` compiles.  Probe telemetry: 6 flow-sunk
  (3 materialized) / 5 args objects sunk; every refusal case declines.
- `--types` diff lane: 474 files, 473 identical, 0 divergent, 1 N/A
  (tester.js, standing).  (The P2-era 493 count included stale extra
  copies in the old work tree; the tracked corpus is 472 + the two new
  probes.)
- Matrix ×7 green (test-eir, lowtier, stages 0-3 at 421 pass / 22
  standing xfail / 0 fail each — the 419 + the two new probes —
  shapes-off lane) — stage1/2/3 self-compiles carry the flow pass
  live (post-cap it fires on the compiler's own classifier-record
  pattern: object literal of arrays + flag, pushed into and
  returned).
- **Phase bench `test/types/types-bench4.js`** (loop-accumulator
  object, read+write per iteration, plus a partial-escape twin):
  **0.04 s vs 0.15 s under EJS_NO_CTOR_SINK-style A/B
  (`EJS_NO_FLOW_SINK` exes from the same tree), 3.75×; node warm is
  0.20 s** — the win is the per-iteration slot/diamond memory traffic
  (GC profile: 625→585 allocs, the 40 per-call accumulator objects).
- types-bench2 unchanged at 0.27 s (0.26 s landed; noise).

Recorded for sinking-P4 (see the design section's
"still recorded" list): key-adding writes under an epoch guard,
`arg_load`, cross-block env scalarization, multi-escape
materialization (each-path-at-most-once), and forwarding single-pred
join params left behind by the fold (LLVM collapses them today; an
EIR-level cleanup would help downstream passes see through).

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
