# The road to 1.0 (and beyond)

Tent-pole releases between here and a self-hosted runtime.  Each
milestone is a capability bar, not a date; the per-phase working plans
(language-plan.md, gc-plan.md, compiler-plan.md, selfhost-plan.md)
carry the mechanics, this doc carries the destinations.

## 0.3.0 — conformance jump

Bar: the test262 CI lane at **>= 75%** pass.

- Headline: lane 54.5% -> 75%+ in one pass (Date API, typed arrays,
  iterator helpers, Atomics/SharedArrayBuffer, property attributes,
  ReferenceError/TypeError semantics, arguments-object rework,
  destructuring iterator protocol, missing constructors).
- Ship with: expectations.txt regenerated (the ratchet only shrinks),
  CHANGELOG framing the conformance jump as the release.

## 0.x — performance milestones (named, not bundled into 1.0)

Two separate bars, each with fixed benchmarks so "competitive" is
measurable:

- **Compilation speed**: self-compile wall-clock (current baseline:
  ~42s stage1 self-compile) vs a target multiple of tsc/node startup
  for equivalent input.
- **Execution + GC**: the bench suite vs node (bench2 et al; shapes
  fast paths already at/near node parity on kernels), plus GC pause
  and footprint targets from gc-plan.md (generational mover).

## 1.0 — the compatibility release

Bar, in three parts:

1. Competitive compilation/execution (the 0.x milestones, held).
2. Competitive GC (ditto).
3. **100% of the declared conformance target.**

"Declared conformance target" — not literal 100% of test262.  An AOT
compiler excludes, by design: `eval`/`Function()` constructor, `with`,
`$262.createRealm`/`evalScript`, multi-agent tests.  1.0 ships a
CONFORMANCE.md that enumerates the carve-outs; everything not carved
out passes.  Decisions to make while writing that document:

- **Temporal**: in or out for 1.0 (478 lane tests; huge surface —
  cheaper post-selfhost, see 2.0).
- **Lane scope**: widen the runner beyond sloppy-only (strict-mode
  double runs) and unskip modules (the .js-suffix fix already
  unlocked most of the 824 module skips).
- The hard tail is compiler semantics, not builtins: TDZ, full strict
  mode, mapped-arguments aliasing, Annex B.

## 2.0 — self-hosted runtime

Bar: the JS-visible runtime written in (lowered) JS, C shrunk to leaf
intrinsics (selfhost-plan.md).

Prefer **incremental** over big-bang: land the lowered-JS builtins
mechanism early (possibly during the 1.0 conformance push), then write
new builtins once, in JS, instead of twice.  Candidate first tenants:
the newest C surface (iterator helpers, Set methods, DisposableStack)
and — if in scope — Temporal, which should never be written in C.

Sequencing note: if the mechanism proves out early, the 1.0
conformance tail gets cheaper, and 2.0 becomes a migration checklist
instead of a rewrite.

## TODO for toshok:
Things I deliberately left for your pass: whether the 0.x perf milestones get version numbers (0.4/0.5?), concrete benchmark numbers for "competitive" (I only cited the baselines already recorded in the docs), and the Temporal in/out call — those are your decisions to ink in.