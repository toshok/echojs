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

## 0.x — analysis-driven compilation (--types earns its keep)

--types is on by default as of 2026-08 (the fv DAG fix and the
dead-generator GC release turned "never finishes" into 66s
self-compile; --no-types is the escape hatch).  What ships today is
the *platform*: analysis runs everywhere, but cross-module imports
are still ⊤ and specialization on the self-compile is ~zero.  This
milestone is the gap between "analysis runs" and "analysis pays".

Bar: on the fixed benchmarks, default-on analysis buys more execution
speed than it costs in compile time, at self-hosting scale.

- **Cross-module summaries** (echojs-maam
  docs/cross-module-summaries.md, phases C1-C6): export summaries
  consumed at import seams over the module DAG — primitives, then
  structural shapes, then ⊤-argument function results.  Success
  metric: unmapped/degraded-binding declines fall on the
  self-compile; cross-module shape sites start guarding;
  specialized > 0.
- **Self-hosted oracle speed**: analysis is <1s node-hosted but ~30s
  of the 66s exe self-compile (entry module alone 8s).  Close the
  self-hosted gap enough that default-on stays comfortable
  (candidates: the Map/Set-heavy oracle inner loops on the runtime
  side, survivor-hole reuse, EIR generator lowering — the latter two
  are already-queued GC/runtime levers with wins beyond the oracle).
- **Runtime type feedback as gap-filler** (README's original PGO
  story): record-types → persist structurally-keyed shape/type
  feedback → recompile with guards, behind the same oracle interface
  the static analysis serves.  Static stays primary; the profile only
  fills residual ⊤.
- Watch item: analysis-on stress RSS ~910MB (vs ~570MB before) —
  revisit with the heap growth policy.

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

- **Temporal**: IN — implemented natively in C (runtime/ejs-temporal.c,
  2026-08; toshok's call, overriding the earlier post-selfhost framing).
  Gate: 100% of the 4,603 test262 built-ins/Temporal tests.
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
the newest C surface (iterator helpers, Set methods, DisposableStack).
(Temporal was originally slated here but landed as C in 0.x — a future
port to lowered JS is optional, not owed.)

Sequencing note: if the mechanism proves out early, the 1.0
conformance tail gets cheaper, and 2.0 becomes a migration checklist
instead of a rewrite.

## TODO for toshok:
Things I deliberately left for your pass: whether the 0.x perf milestones get version numbers (0.4/0.5?), concrete benchmark numbers for "competitive" (I only cited the baselines already recorded in the docs), — those are your decisions to ink in.  (The Temporal in/out call is now inked: in, as native C.)