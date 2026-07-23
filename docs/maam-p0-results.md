# MAAM Phase 0 measurement results

Date: 2026-07-19.  Chunk C of the Phase 0 plan (docs/maam-plan.md): run the
`--types` probe over real corpora and turn "does it converge on our sources,
at what cost" into numbers.  Measurement only — no code changes.

## Environment

- echojs `eir` @ d3e3fd1 (probe as committed), submodule `echojs-maam`
  `ejs-integration` @ 1bea5de, maam CJS dist built from that commit.
- node v22.4.0, macOS arm64 (Darwin 24.6.0), 32 GB RAM, llvm at
  `/opt/homebrew/opt/llvm`.
- Compiler under test: node-hosted stage0 (`//lib:generated` babel tree),
  run from a stage0-style work tree (`//:srcdir-tree` copy + `lib/generated`
  + repo `test/`), the same layout `buck-test-stage.sh` assembles.
- Analysis spec (hardcoded in the probe):
  `kCFA(1, "flow-sensitive", "call-site", shapeCap=64, false, false, false, stateCap=512)`.
- Raw logs + orchestrator/aggregator scripts: `~/.cache/maam-p0-logs/`
  (`A-on/`, `A-off/`, `B/`, `*.summary.json`).

## Command shapes

Corpus A, per file (cwd = worktree`/test`, 120 s kill-timeout, concurrency 4):

    node <work>/lib/generated/ejs-es6.js --srcdir \
        --moduledir ../node-compat --moduledir ../ejs-llvm --types <file>.js

Corpus B, one self-compile exactly like stage1 (cwd = worktree root):

    /usr/bin/time -l node lib/generated/ejs-es6.js --srcdir \
        --moduledir node-compat --moduledir ejs-llvm --types ejs-es6.js

`PATH` prepends the llvm bindir; `NODE_PATH` = repo `node_modules` +
`node-llvm/build/Release`; `SDKROOT` from xcrun — mirroring
`buck-test-stage.sh`.  The work tree must live **inside the repo checkout**
so the probe's upward walk can locate `external-deps/echojs-maam` (see the
dev-tree-only note in maam-plan.md); from `/tmp` the probe warns
"could not locate the submodule" and analyzes nothing.

## Corpus A — `test/*.js` (457 files, all run; no sampling needed)

Full run at concurrency 4; the 120 s per-file budget was never approached.

| class | files | notes |
|---|---|---|
| analyzed, degraded-with-warning | 362 | ≥1 module with warnings (see kinds below) |
| analysis-failed (warn-wrapped) | 84 | all `NormalizeError`; compile continues, exit 0 |
| analyzed, clean (`warnings=none`) | 10 | closure-test1, closure4, closure7, eir-interop1-lib, eir-ns1-lib, eir-syntax2-lib, object1, reexport1-lib, reexport2-mid, void0 |
| TIMEOUT (>120 s) | 0 | |
| compile-N/A | 1 | `tester.js` — parse error in the esprima fork, fails identically without `--types` (exit 255 both ways) |

**Zero compile failures caused by `--types`.**  Every failure mode above is a
warning; the one non-zero exit fails flag-off too.  Parity spot-check: 21
files (every 23rd, alphabetically, plus tester.js) compiled flag-off — 21/21
exit codes identical to the flag-on run.

Warning kinds across the 413 analyzed modules (module counts):
`unknown-call` 378, `polymorphic-function` 27.  `unknown-call` is ubiquitous
because nearly every test calls `console.log` (counted as an unknown method
call since the Chunk A metric broadening).

`NormalizeError` breakdown (84 files):

| count | message |
|---|---|
| 26 | only plain identifier parameters are supported (no destructuring/defaults/rest) |
| 22 | unsupported statement: ForOfStatement |
| 8 | unsupported statement: LabeledStatement |
| 7 | unsupported expression: TemplateLiteral |
| 5 | computed object keys |
| 4 | Object.defineProperty requires a string-literal key |
| 3 | TaggedTemplateExpression |
| 3 | object getters/setters |
| 2 | unsupported expression: VariableDeclaration |
| 2 | non-identifier object keys |
| 1 | DebuggerStatement |
| 1 | defineProperties non-literal descriptors |

Wall time (analysis only, per module; n=413): median 5 ms, p90 12 ms, max
5032 ms, total 20.9 s.  Whole-compile wall per file (includes llc + clang):
median 333 ms, p90 367 ms, max 7.3 s.  Flag-on vs flag-off median on the
21-file sample: 335 ms vs 323 ms (~+4%).

Slowest analyses: esprima-es6 (5.0 s; pulled in by esprima1/
esprima-roundtrip1/2 as an import) — 3 appearances; typedarray2.js (2.2 s,
832 states, 32 994 iterations); fib.js (0.4 s, 9 687 iterations).

unknownCalls per module: median 3, p90 15, max 168 (typedarray2), total
2 694.  Top: typedarray2 168, math1 132, error1 70, typedarray14 69,
typedarray15 63.  degradedBindings: 0 everywhere — post-desugar test trees
carrying `rest` never reach analysis (they bounce off the destructuring-
parameter NormalizeError first).

## Corpus B — the compiler's own generated JS (stage1 self-compile input)

One `--types` self-compile of `ejs-es6.js`: **45 modules, exit 0, executable
produced and linked.**  Real time 11.35 s vs 6.15 s flag-off (+5.2 s, +85%);
4.74 s of the delta is the single esprima-es6 module.  Max RSS 287 MB
(`/usr/bin/time -l`; 10 s RSS polling agrees) — nowhere near the 4 GB watch
threshold.  No timeouts; per-module analysis never exceeded 4.8 s.

| class | modules |
|---|---|
| analyzed (stats emitted) | 15 |
| analysis-failed (warn-wrapped NormalizeError) | 30 |
| TIMEOUT | 0 |

Failure breakdown: TemplateLiteral 15, ForOfStatement 13,
destructuring/defaults/rest parameters 2.  The compiler's own `lib/` modules
are written in modern JS; their post-desugar form still contains template
literals and for-of (EIR lowers those natively; nothing desugars them away
before the probe), so maam's normalizer rejects 2/3 of the compiler by
module count — including every big module (`compiler`, `eir/lower`,
`eir/optimize`, …).

The 15 analyzed modules (analysis wall): esprima-es6 4 740 ms (102 states,
157 iters, 2 unknownCalls, polymorphic-function+unknown-call warnings),
escodegen-es6 62 ms, estraverse-es6 35 ms, esutils/lib/code 34 ms (739
iters), sret-abi 6 ms, abi 3 ms, common-ids 3 ms (69 unknownCalls),
host-config 3 ms, plus 7 more ≤3 ms, all `warnings=none` or a single
unknown-call.  degradedBindings: 0 on all 15.

Determinism: a second full `--types` self-compile produced byte-identical
stats lines (wall stripped) — reachedStates/iterations/unknownCalls all
stable across runs.

## Cap behavior

Not observable.  Neither `describe()` nor `metrics` exposes stateCap or
shapeCap hit counts; nothing in the output distinguishes "converged
naturally" from "converged because the cap smeared contexts".  (esprima-es6's
5 s / 102-state / 157-iteration profile is wall-heavy but state-light, which
suggests time goes to store joins on very large flow-sensitive stores rather
than state explosion — but that is inference, not measurement.)  **Finding
for Phase 1: surface cap-hit counters (`funcContexts` saturation, shape
widenings) in `metrics`.**

## Reading (factual)

- **Convergence verdict, Corpus A:** converges everywhere it runs; zero
  timeouts; analysis is noise next to codegen (median 5 ms vs 333 ms
  compile).  Decision-rule outcome **(a)** for the gate corpus.
- **Convergence verdict, Corpus B:** converges on everything it can parse,
  at +5.2 s on an 11 s self-compile — tolerable for an opt-in flag.  But the
  measured set excludes every compiler-sized module: the largest thing
  actually analyzed was esprima-es6.  The plan's headline question ("do
  compiler-sized modules converge?") is **still open — blocked on dialect
  coverage, not on the engine.**  The binding constraint Phase 0 found is
  maam's normalizer coverage, not convergence or cost: TemplateLiteral,
  ForOfStatement, and destructuring params account for 100% of Corpus B's
  rejects (30/30), ~65% of Corpus A's (55/84), ~75% combined (85/114; ~77%
  if TaggedTemplateExpression is counted into the template family).
- **Where time goes:** esprima-es6 dominates both corpora — its three
  appearances in Corpus A (imported by the esprima tests) are 71.7% of A's
  total analysis time (24.1% for a single appearance), and in Corpus B it is
  96.9% of analysis time and 91% of the flag-on/off wall delta; everything
  else is ≤62 ms.  Whatever
  makes esprima slow (large flow-sensitive stores is the hypothesis) is the
  first profiling target if bigger modules join the corpus.
- **Implication for the decision rule:** no evidence for (b) or (c); no
  widening pressure observed and nothing failed to converge.  But (a) can
  only be provisionally claimed: the modules that would stress the engine
  never reached it.  The cheapest way to make Phase 0's question answerable
  is normalizer coverage for the three dominant constructs, then re-run this
  measurement — that decision belongs to Phase 1 planning, not this doc.
- `degradedBindings` never fired on either corpus; the rest-parameter
  degradation path is currently exercised only by maam's own unit tests.
- `unknownCalls` is dominated by stdlib/console usage; per the plan's Future
  work note, these numbers are not comparable to the maam repo's pre-
  broadening paper tables.

---

# Phase 1 re-measurement (Chunk E)

Date: 2026-07-19 (runs) / 2026-07-21 (aggregation).  Same protocol,
environment, and command shapes as the Phase 0 measurement above; the only
change is the analyzer: submodule @ 1e09ffd (⊤-degradation; TemplateLiteral /
ForOfStatement / pattern coverage; cap-hit counters; S1 closure-fingerprint
iteration degrade).  echojs @ 3e71ad4 (no compiler changes since d3e3fd1).
Raw logs: `~/.cache/maam-p0-logs/` (`B2/`, `A2-on/`, alongside the Phase 0
`B/`, `A-on/` for diffing).

## Corpus B — compiler self-compile (the headline)

**38 of 45 modules analyzed (was 15), exit 0, executable linked, no
timeouts.**  Every compiler-sized module now reaches the engine and
converges:

| module | wall | states | iterations | caps |
|---|---|---|---|---|
| ejs-es6.js (driver) | 4007 ms | 1050 | 29 615 | none |
| esprima-es6 | 4609 ms | 106 | 210 | none |
| lib/passes/desugar-classes | 120 ms | 68 | 68 | shapeCap 1 |
| escodegen-es6 | 68 ms | 166 | 231 | shapeCap 1 |
| lib/eir/ops | 66 ms | 212 | 212 | none |
| lib/eir/optimize | 39 ms | 14 | 14 | none |
| lib/eir/lower | 29 ms | 115 | 115 | shapeCap 1 |
| lib/compiler | 10 ms | 74 | 74 | shapeCap 1 |

Aggregates (n=38 modules): analysis wall median 3 ms / p90 68 ms / max
4609 ms / **total 9.1 s**.  Real time 15.28 s flag-on vs 5.73 s flag-off
(+9.6 s; esprima-es6 + the ejs-es6 driver account for 8.6 s of it).  Max
RSS 575 MB (was 287 MB) — well under the 6 GB watch line (raised from
Phase 0's 4 GB for this run).  Totals:
unknownCalls 202, degradedBindings 136.

**Cap behavior (now observable):** stateCap (512): **0 hits anywhere** —
including the 29 615-iteration driver analysis.  shapeCap (64): exactly
**1 hit in each of 10 modules** (triple, abi, compiler, eir/emit,
eir/builder, eir/lower, eir/scopes, escodegen, estraverse, desugar-classes)
— one megamorphic collapse per module, consistent with a single object
built up field-by-field under weak updates.  Convergence is natural, not
cap-forced, everywhere it matters.

**Remaining rejects (7, was 30):** 6 × "only plain identifier or
destructuring-pattern parameters" + 1 × `Object.defineProperty` non-literal
key (lib/runtime).  Root cause of the 6, identified by inspection: echojs's
`DesugarDestructuring` keeps a trailing **`RestElement` in `params`** ("a
trailing ...rest stays in place — EIR handles it natively",
lib/passes/desugar-destructuring.ts:244) — maam's `compileFunction` accepts
the old-esprima `.rest` *field* but not a RestElement param.  A one-line
coverage item (treat a trailing RestElement param exactly like the dialect
`rest` field); affected: ast-builder, node-visitor, consts,
desugar-metaproperties, desugar-spread, desugar-destructuring.

## Corpus A — test/*.js (457 files, full rerun)

| class | Phase 0 | now | Δ |
|---|---|---|---|
| analyzed, degraded-with-warning | 362 | 391 | +29 |
| analysis-failed (warn-wrapped) | 84 | 55 | −29 |
| analyzed, clean | 10 | 10 | — |
| TIMEOUT | 0 | 0 | — |
| compile-N/A (tester.js, flag-independent) | 1 | 1 | — |

Zero `--types`-caused compile failures again (only tester.js exits
non-zero, identically flag-off).  Remaining reject histogram: param-kind 28
(the same RestElement gap as Corpus B), LabeledStatement 8, defineProperty
non-literal key 5, computed object keys 5, getters/setters 3, misc 6.
TemplateLiteral, ForOfStatement, TaggedTemplate, and
destructuring-declaration rejects are **gone** (22 + 7 + 3 + the pattern
share of the old 26-count bucket in the Phase 0 histogram).

Wall time is unchanged: analysis per module (n=442) median 6 ms / p90
13 ms / max 5105 ms (esprima again) / total 21.0 s; per-file compile wall
median 334 ms (was 333), p90 362, max 7.2 s.  Cap hits: shapeCap 1 in 6
modules across the 3 esprima-importing files; stateCap 0 everywhere.

## unknownCalls / degradedBindings deltas (S1 caveat quantified)

- Corpus A total unknownCalls 2694 → 2935 (+241).  Decomposition: **+218
  from the 29 newly-analyzed files** (code the engine never saw before —
  console/stdlib externals plus iterator-protocol degradations; not
  separable per-kind in current metrics); **+36 across 9 of the 372
  previously-analyzed files — all increases** (set3 +17, array30 +7,
  typedarray10 +4, … — none containing for-of; this is ⊤-propagation
  reaching branches a false `undefined` used to kill).  The **−13 is a
  double-count correction**, not a decrease on any file: eir-promo1.js was
  analysis-failed in Phase 0 but had emitted partial stats (13 unknownCalls
  already inside the 2694 total), and its full re-count now sits inside the
  +218 bucket.  2694 + 218 + 36 − 13 = 2935.
- Corpus B: the 15 previously-analyzed modules are **stable — zero changed
  unknownCalls**; the +128 rides on the 23 newly-analyzed modules (driver
  98, lib/types 26, everything else ≤2).
- **S1 (closure-fingerprint iteration degrade): no measurable inflation on
  previously-analyzed code in either corpus.**  The feared
  for-of-over-function-arrays cost did not surface at corpus scale; if
  per-kind attribution is ever needed, a degradation-kind counter is the
  follow-up.
- degradedBindings: 0 → 66 (Corpus A) / 136 (Corpus B) — now counting
  unmodeled imports and rest parameters as designed; an imports-only module
  no longer masquerades as a closed world.

## Reading against the decision rule

- **The Phase 0 open question is closed: outcome (a) — ship as-is behind
  `--types`.**  Compiler-sized modules reach the engine and converge
  naturally: 0 stateCap hits corpus-wide, shapeCap touched exactly once in
  each of 10 of the 38 modules, the largest analysis (29 615 iterations)
  finishes in 4 s, total
  self-compile overhead +9.6 s on an opt-in flag, RSS 575 MB.  No evidence
  for (b) heavy-widening or (c) non-convergence anywhere in either corpus.
- Where time goes is unchanged in kind: esprima-es6 (wall-heavy,
  state-light — store-join cost hypothesis stands) plus, now, the ejs-es6
  driver (iteration-heavy, converges clean).  Everything else ≤120 ms.
- The binding constraint has shrunk from "three constructs blocking every
  big module" to **one one-line gap (RestElement params) plus a small
  tail** (labels, non-literal defineProperty keys, computed keys,
  accessors-in-literals) — with only the RestElement gap blocking any
  module of consequence.

---

# Phase 3 gates (Chunk J)

Date: 2026-07-22.  echojs @ 568efc7 (oracle-guided guarded arithmetic in
lowering), maam @ 8d6a157.  Same environment as the earlier measurements
(node v22.4.0, macOS arm64, llvm @ /opt/homebrew/opt/llvm); everything runs
color-free (`NO_COLOR=1`, `FORCE_COLOR` unset — a colored-env buck daemon
poisons regenerated expected files; lesson institutionalized in the lane
script).  Raw logs: `~/.cache/maam-p0-logs/J*` (diff-lane per-file logs +
results.jsonl; the microbenchmark timings below are recorded here only —
the timing runs left no separate artifact).

## The --types diff lane (the behavioral gate)

`./buck-test-types-diff.sh <work-tree> <log-dir> [conc]` — every test/*.js
compiled flag-off AND with `--types`, both executables run, RUN STDOUT
byte-compared (stderr excluded by design: `--types` stats lines, and the
debug runtime's `EXCEPTIONS:` traces on normally-handled exceptions).
Per-file 120 s timeouts, concurrency 4, per-worker TMPDIRs (concurrent
compiles never share temp space).

| files | identical | divergent | N/A | timeouts |
|---|---|---|---|---|
| 458 | 457 | **0** | 1 (tester.js, esprima parse gap — fails flag-off too) | 0 |

Aggregates from the `--types` stats lines: **diamonds 67**, oracleQueries
1319, oracleUnknown 1035.  The high unknown share is expected on this
corpus: operands inside functions the per-module analysis never reaches
(exported-only / callback-only bodies, dead branches) query as unknown →
top → no diamond — the guard-shaped degradation working as designed.  The
suite is string/object-heavy; 67 diamonds concentrate in the numeric
files.

## test/types/ probes

Seven standalone probes (see test/types/README.md for the per-file table):
diamond-eligible shapes fire (locals 6, params 4, literals 5, loops 6,
bench kernel 9); reassignment-widened bindings do NOT diamond (0, by
design — only exact {number} qualifies); and the wrong-oracle case — a
cross-module call handing a string to a parameter the callee's module
analysis typed {number} — routes through the has_tag guard to the slow
path and prints the correct "x1" with flag-off/--types outputs identical.

## Microbenchmark

test/types/types-bench1.js: 40 × 1 M-iteration kernel of
`s = s + i*i - i/2; i = i + 1` under a `<` loop guard — all module-local,
everything oracle-typed {number}; diamonds=9, oracleUnknown=0.  Compiled
flag-off vs `--types`, run 7× each interleaved (`/usr/bin/time -p`, same
machine, no other load; distributions were tight — no GC-outlier rerun
needed):

| build | median | min | max |
|---|---|---|---|
| flag-off | 3.19 s | 3.18 s | 3.20 s |
| --types | 0.31 s | 0.31 s | 0.32 s |

**10.3× median speedup**, identical program output (13333303333341514000).
Honest caveats: this kernel is the best case — per-iteration generic
runtime binop calls dominate the flag-off build, and the typed build
replaces essentially all of them (9 diamonds cover the kernel's every
operator).  Real modules keep their surrounding generic ops; the suite-wide
effect is bounded by the 67-diamond density above, and unbox/box round
trips still go through memory (the bits_alloca idiom), so further headroom
remains for a Phase 4-era register-level cleanup.

## Matrix + stage2 ≡ stage3 (flag off)

Full serial matrix green: //:test-eir (incl. the typed-arith EIR-shape
tests), //:test-eir-lowtier (the injected low-tier e2e), //:test-stage0..3.
Functional stage2 ≡ stage3 gate, per the reading this document establishes
(raw binary byte-identity does not hold on macOS for linker-metadata
reasons): stage2 and stage3 each compile and run the ENTIRE test corpus in
identical buck-assembled work dirs with per-test expected-output
comparison — both green constitutes the corpus-level functional-identity
check.  Flag-off byte-purity of the Phase 3 lowering itself was
additionally proven at Chunk I review time (pre- vs post-chunk flag-off
executables byte-identical).

## Reading

Every P3 gate item holds: zero behavioral divergence across the suite with
the flag on; the probe dir documents exactly which shapes fire and which
degrade (widening, wrong oracle — both by design); the mechanism-level
proof (//:test-eir-lowtier) is now backed by a magnitude measurement (10×
on a pure-numeric kernel, a ceiling not a promise); matrix unaffected flag
off.  Phase 3 is complete pending sign-off.

# Phase 3.4 gates (diamond pre-work)

Date: 2026-07-22.  echojs @ eir (this commit; passes in
lib/eir/optimize-guards.ts), maam @ 8d6a157.  Same environment and
color-free protocol as the Phase 3 measurements.

## The passes (what changed)

Two trust-free optimizer passes over the Phase 3 diamonds — nothing here
consumes an oracle claim; every fact is proven from the IR, so a wrong
oracle still only costs speed:

- **(a) dominated-guard elimination + guard-region merging.**  Dominance
  reasoning: the CHK dominator tree (shared with the verifier) plus the
  sole-predecessor-TRUE-edge condition — entering such a successor is
  equivalent to its guard having held, and SSA number-ness is immutable —
  combined with value-intrinsic proofs (const number, box_f64, and
  generic mul/div/sub results, which are always numbers in both ES and
  runtime/ejs-ops.c).  Region merging structurally VERIFIES (never
  assumes) the diamond shape — effect-free fast side, whitelisted
  {add,sub,mul,div,lt} slow chain — then fuses adjacent regions into one
  guard region with ONE slow path (the full generic computation in
  program order).  Guard failures after partial fast execution re-enter
  the slow chain from the top; the merge first proves that re-execution
  is pure and value-identical (operands guard-proven numbers), else it
  refuses.
- **(b) raw f64 block params for optimizer-rewired joins.**  A param
  whose every incoming argument is a strippable box_f64 / f64 value /
  converted param becomes an f64 phi (double in the emitter), killing
  the bits_alloca box/unbox round-trips between merged diamonds; any
  remaining boxed use re-boxes exactly once at the region exit.  The
  verifier's P2 rule is lifted ONLY for params carrying the new
  `rawJoin` marker, and the marker is provenance rather than trust: the
  verifier independently re-checks type-f64, all-args-f64, non-catch,
  no-unwind-edge — an f64 param WITHOUT the marker is rejected, so every
  lowering-created edge keeps the strict boxed rule.

hypot2 acceptance shape (see the regenerated
`~/src/echojs/hypot2-types-before-after.txt`): three diamonds / six
has_tags as lowered → ONE region with one has_tag per distinct value
(2), one slow chain (mul/mul/add), fast side unboxed end-to-end through
`phi double` joins, one box_f64 at the region exit.

## The --types diff lane (behavioral gate)

Clean re-assembled work tree, identical protocol:

| files | identical | divergent | N/A | timeouts |
|---|---|---|---|---|
| 458 | 457 | **0** | 1 (tester.js, unchanged) | 0 |

Aggregates: **diamonds 67**, oracleQueries 1319, oracleUnknown 1035 —
byte-for-byte the Phase 3 numbers.  The lane counts LOWERING's diamonds
and the passes run post-hoc, so the count is unchanged by design; the
lane's expectations needed no touch.  Both vacuous-pass guards
re-verified to trip: an empty work tree exits 1 ("zero files compared"),
and an outside-the-repo tree (dead oracle) exits 1 ("diamonds total is
0").  An additional superset run (467 files: the 458 plus probe/demo
copies) was also 0-divergent.

## EIR-shape unit tests

//:test-eir green, 114 tests, including the new Phase 3.4 shapes:
merged hypot2 (exactly 2 has_tags, a single guard-failure target, the
generic mul/mul/add surviving on the one slow path, box_f64 exactly
once, f64 rawJoin params on the intermediate joins); the bench-kernel
statement chain merging across pure const prefixes (six diamonds → 2
has_tags, 1 slow path, 1 box); a negative shape (guards in an if-branch
do not dominate a later re-test: nothing folds, nothing merges, no raw
params); and the verifier triple (rawJoin accepted; f64 param without
the marker rejected; boxed arg into a rawJoin param rejected).

## Microbenchmark (types-bench1, deltas vs Phase 3)

Same kernel, same protocol (7× interleaved, /usr/bin/time -p):

| build | P3 median | P3.4 median | note |
|---|---|---|---|
| flag-off | 3.19 s | 3.21 s | unchanged (five runs 3.17–3.37; two hit background-load noise at 4.96/5.70 — kept in, the median absorbs them) |
| --types | 0.31 s | **0.23 s** | −26% typed runtime |

**Speedup 14.0× median (was 10.3×)**; diamonds=9, oracleUnknown=0,
output identical (13333303333341514000).  Remaining headroom is the
region BOUNDARIES: loop-carried params and call arguments still box
(entry args are consts/params, not box_f64 — deliberately outside pass
(b)'s proof), which is P3.6's typed-calling-convention territory.

## hypot2 demo (deltas vs Phase 3)

diamonds=7 (unchanged — lowering's count).  Wall time unchanged within
noise (flag-off 2.71/2.51/2.60 s, --types 0.51/0.34/0.35 s, ~7×): the
demo is dominated by the boxed call/closure/loop overhead around
hypot2, which P3.4 does not touch.  What changed is the emitted shape —
2 NaN-box checks instead of 6, `phi double` fast pipeline, one generic
chain, one box — recorded with before/after EIR and LLVM excerpts in
the regenerated dump file.

## test/types probes

All seven probes still match (`node` diff / flag-off≡--types for the
wrong-oracle case), per-file diamond counts identical to the census
(6/4/5/0/6/9; wrongoracle lib=1).  The wrong-oracle keystone still
routes the cross-module string through the guard to the slow path and
prints "x1" with identical flag-off/--types output.

## Matrix + stage2 ≡ stage3 (flag off)

Full serial matrix re-run on the final code: //:test-eir,
//:test-eir-lowtier, //:test-stage0..3 — six of six BUILD SUCCEEDED
(grep-verified in the buck logs, never tail exit).  stage2 ≡ stage3
functional gate (both stages compile and run the entire corpus with
per-test expected-output comparison) green.  Flag-off the new passes
bail before touching anything: optimizeGuardRegions scans for number
guards and returns (none exist without --types), so flag-off output is
untouched by construction and the stage gates confirm it.

## Reading

Both P3.4 items hold with zero behavioral divergence: dominated guards
fold and adjacent diamonds merge into single-slow-path regions on real
dominance reasoning; the raw-f64-join lift is scoped by a
verifier-re-checked marker rather than a global weakening; the
microbenchmark ceiling moves 10.3× → 14.0×, and the remaining box/unbox
traffic sits exactly where P3.6 (typed calling convention) picks up.

## Review fixes (adversarial pass over the merge)

Two latent unsoundnesses were found by adversarial review on
verifier-valid IR (neither constructible from JS through today's
lowering, both violations of the "structurally verified, never assumed"
contract) and are fixed with localized pre-checks in `tryMergeAt`, each
with a refusal unit test:

- **J1 predecessor exhaustiveness**: j1's predecessors must be exactly
  region1's exits (mirroring the existing j2 check).  A foreign edge
  into j1 made region2's guards reachable without region1 having run,
  while the merge substituted region2's slow operands with region1's
  slow-side values — wrong on the foreign path.
- **Generic-twin verification** (`verifyGenericTwin`): region2's slow
  chain must be the generic rendition of its fast side — same
  arithmetic ops in the same order, operands corresponding under the
  box/unbox mapping, join-exit args corresponding slot for slot.  The
  reroute sends executions whose region2 guards would have passed
  (e.g. a guard on a mul result) through the slow chain; twin-ness is
  what makes that value-identical.  One deliberate narrowing: a region2
  containing `f64_lt` now declines to merge (the boolean-twin
  correspondence buys nothing measurable; lt regions still merge as
  region1) — hypot2/bench shapes and stats are unaffected
  (regions_merged and all measured numbers unchanged; bench re-verified
  at 0.23 s).

Also from review: routing explicitly refuses raw-typed (i1/f64)
j1-values live past region2 (fail-closed, now documented + enforced up
front); the loop-carried rawJoin conversion (a fully-proven f64 loop
param) gained a dedicated unit test; ir.ts's rawJoin comment now states
the actual contract (structural qualification, verifier-checked — not
provenance-linked).

# Phase 3.5 — differential harness (concreteEval vs node vs ejs)

Date: 2026-07-23.  echojs @ eir (P3.4 head), maam @ c69fc81, revised same
day to maam @ 3e64ca1 (review round 2) and maam @ c3d1aed (round-3 nits
R1/R2; the review subsections below record what changed — numbers in this
section are the FINAL c3d1aed figures).  Deliverable
lives in the maam repo: `test/differential/harness.ts` + a 40-file
closed-world corpus, run by `npm run diff-harness` and wired into the new
maam CI workflow (`.github/workflows/ci.yml`, node pinned 22.4.0).  The
concrete interpreter — `analyze(prog, concreteEval() + intrinsics)` — is the
reference semantics; the harness diffs it against node, against
ejs-compiled output, and against the abstract oracle configs.

## Comparison semantics (the deliberate choices)

The corpus convention is that a file's last top-level statement is an
ExpressionStatement; its value is the file's *final value* (for that shape
it coincides with the completion value).  Comparison is **value-level**,
not host-stringification: the harness wraps that expression in an injected
ES5 renderer (−0 renders `"-0"`, NaN `"NaN"`, strings escaped by hand) and
the same renderer is applied to the concrete CVals, so number→string is
the identical algorithm on both sides.  node's printed value must be a
MEMBER of the concrete result set; singletons must match exactly.
Documented blind spots: object/function finals compare by type only
(corpus files project structure into primitives), and non-singleton sets —
from the machine's two deliberate over-approximations, the smashed array
`elements` bucket and the always-reachable nondet catch handler — are
reported as PASS-CONTAINS, never silently.  Analysis runs per file in a
worker subprocess under a 30 s budget: genuine concrete-machine divergence
(nondet for-of/for-in × unbounded concrete time) becomes a *visible* skip.

## Gate results

- Corpus 46 files.  node lane: **37 exact, 3 membership, 0 divergences**;
  6 skips, all deliberate and printed with reasons (Math.random
  nondeterminism; array prototype methods degrade under the concrete
  domain; two files that prove the for-of/for-in divergence timeout path;
  one each proving the nested-block-var and pattern-leaf-capture visible
  degradations).  The
  differential lane exercises **zero iteration-protocol semantics** —
  for-of/for-in are exactly the skip files, because their nondet iteration
  never converges under unbounded concrete time.
- Containment lane: **1935 node checks, 0 violations** across two abstract
  configs — the echojs oracle spec verbatim
  (`kCFA(1, flow-sensitive, call-site, shapeCap=64, stateCap=512)`) and the
  same + `intrinsics: true`.  Checked-node set: every source node BOTH the
  concrete and the abstract run map (the concrete entries are exactly what
  a real execution produced).  **Census of the exempt remainder** (57
  concrete-mapped nodes unmapped abstractly, summed over both configs; the
  harness prints the count so growth is visible): these are NOT all dead
  code — under config A (intrinsics off) they include LIVE coercion
  arithmetic whose receiver/operand chain passes through an unbound global
  (`Math.PI * 2 * 2`, `Number.MAX_VALUE * 2`) plus coercion forms the
  normalizer maps but config A's ⊥-receiver paths kill (`true+1`, `""-1`,
  `-"3"`, `+true` in the coercion files).  Honest reading: the oracle
  currently produces NO facts for such nodes (fail-soft ⊤ at the
  consumer), so containment there is vacuous — they are exempt, not
  verified.
- ejs lane (dev tree only; `MAAM_DIFF_EJS_TREE` = a stage0 work tree —
  `//:srcdir-tree` copy + `lib/generated`; the lane skips loudly when
  unset, e.g. in maam CI): **32 ok, 1 N/A (esprima `**` family), 7
  known-divergent, 0 new, 0 stale**.

## What the harness found (the product)

Fixed in maam (each with a pinned test; suite 241 → 258):

1. **Closure-capture unsoundness** — a closure created textually at or
   before a variable's declaration in the same statement list (a hoisted
   function declaration — and, per review round 2, equally a function
   expression, arrow, or object-literal method) referencing that variable
   left the name un-renamed; closure writes silently missed the binding
   (`var f = function () { n = "x"; }; var n = 0; f(); n` reported `num`
   with zero degradation — a mapped-and-wrong oracle fact an unguarded
   consumer would miscompile on).  normStmts now detects capture with a
   syntactic over-approximate scan over ALL function-creating subtrees,
   positionally (capture at statement i ≤ declaration j), pre-binds
   captured names to `undefined` above everything, and turns their
   declarations into `setVar` writes.  Declare-then-capture shapes keep
   the precise fresh-binding path (no `undefined` widening), pinned by a
   typeOfNode unit test.
2. **⊥-receiver property reads fabricated `undefined`** — with intrinsics
   off, `Math.PI` read as a *confident* undefined (the containment lane
   caught this as `num ⋢ undefined`).  ⊥ receivers now propagate ⊥.
3. **String relational comparison was numeric** — `"a" < "b"` was false.
4. **ToNumber(null) was NaN in binops** — `1 + null` computed NaN, JS says 1.
5. **`s.length` read as confident undefined in both domains** — now exact
   under the concrete domain, `anyNum` abstractly, ungated from the
   intrinsics knob (the echojs oracle runs intrinsics-off).
   Plus: `Infinity`/`NaN` identifiers were unbound (path-killing ⊥); they
   are dialect literals now.

Also built: exact concrete intrinsics — the plan's `intrinsics: true`
silently degraded under the concrete domain (seeded globals were ⊥).  The
domain gained an optional `concretize` capability whose presence is the
exactness contract: pure-primitive intrinsics compute their real JS result
or the call degrades visibly through `unknownCalls`; summary models never
run concretely.

Found in echojs, root-caused by minimal probes, recorded in
`ejs-known-divergences.json` (a listed file that *stops* diverging fails
the gate as stale, so the list can only shrink by fixing echojs):

1. `typeof null` → `"null"` (spec: `"object"`).
2. `-0 === 0` → false (NaN-boxed bit comparison; `1/-0` is correct).
3. `Math.round(-2.5)` → −3 (C `round()` half-away-from-zero; JS: −2).
4. `Number("  7  ")` → NaN (ToNumber(string) does not trim whitespace).
5. `-8 >>> 28` → 0 (ToUint32 on negative shift operands).
6. `1 + null` → runtime abort (`ejsval ToNumber(ejsval)`,
   runtime/ejs-ops.c:260 "not implemented", exit 134).
7. esprima cannot parse `**` (arith-basic.js is the lane's one N/A).

## Known model limits (documented, visible, tracked)

- try/catch: handler modeled as always-reachable nondet with a ⊤ caught
  value (sound over-approximation; membership-checked).  `return` through
  `finally` skips the finalizer in the model — corpus avoids the shape.
- `F.prototype = Object.create(...)` (prototype REASSIGNMENT) is
  unmodeled and degrades visibly; the dialect shape is
  `Object.setPrototypeOf`, which is modeled.
- for-of/for-in accumulation diverges under concrete time (nondet
  iteration); the harness's worker timeout makes it a visible skip.
- Nested-block `var` hoisting is not modeled; when such a var is captured
  by a function in the enclosing scope the normalizer now COUNTS it as a
  degraded binding (review F2), so the harness precondition trips and the
  file skips visibly instead of computing on ⊥.  Destructuring-pattern
  LEAVES captured at-or-before their declaration are likewise unmodeled
  and, without the round-3 accounting, were SILENTLY WRONG (writes
  dropped, zero counters) — they now count as degraded bindings too
  (review R1).  Re-declared (`var x` twice) captures ARE modeled: both
  declarations assign the one pre-minted binding.
- Captured-by-closure vars now (correctly) include `undefined` in their
  nodeTypes join from the hoisted pre-binding; non-captured and
  declare-then-capture vars are unaffected.  Oracle-fact impact measured
  by the `--types` diff lane re-run below.

## Review round 2 (adversarial pass over the harness commit)

The review confirmed the harness mechanics (wrap seam, gate teeth under
perturbation, sigLeq, ejs-lane authenticity, CI viability) and rejected on
one confirmed HIGH finding plus process items; all addressed at maam
3e64ca1:

- **F1 (the blocker): capture fix was FunctionDeclaration-only.** A
  closure created textually at-or-before a later same-scope `var` via a
  function expression / arrow / object-literal method still dropped its
  writes silently — concrete `{num 0}` with zero degradation for
  `var f = function () { n = "x"; }; var n = 0; f(); n;` while node says
  "x", and the oracle reported a mapped-and-wrong `num`.  Fixed by
  replacing the compiled-freeVars detection with a syntactic
  over-approximate scan over ALL function-creating subtrees (positional:
  capture at statement i ≤ declaration j; declarations count as i = −1).
  Three corpus probes (capture-fnexpr/arrow/objmethod.js) now PASS
  exactly — the fix computes, it does not degrade.
- **F2: nested-block `var` capture now counts.**  Previously concrete ⊥
  with zero accounting; the normalizer pushes a degradedBinding so the
  harness skip precondition trips (skip-nested-var-capture.js proves it).
- **F3: unit pins independent of the harness** (review showed reverting
  normalize.ts kept all 258 then-tests green): hoisted/expression/arrow/
  method capture, declare-then-capture precision (typeOfNode stays exact
  `num`), nested-var visible degradation, bare NaN / Infinity literals,
  and the ⊥-receiver read, each flipping if its fix is reverted.  Suite
  258 → 266.
- **F4: known-divergence entries participate in staleness even when
  unvalidatable** — a listed file that goes compile-N/A, is skipped, or
  leaves the corpus is warned about by name (warning, not hard failure:
  N/A means the run-behavior claim cannot be tested in either direction,
  and hard-failing would let an esprima parse gap flip a semantics gate).
  Entries are now structured ({symptom, rootCause}, enforced).
- **F5: the containment-exempt census is documented above** (the
  57-node remainder includes live coercion arithmetic under config A —
  exempt, not verified — with the count printed every run).
- **F6: compound assignments corpus file added** (esprima-clean, so it
  has full three-lane coverage; the `**` family lives in the expected-N/A
  arith-basic.js); the zero-iteration-protocol statement is in the gate
  results above.

Final harness numbers at 3e64ca1 (all lanes): corpus 45 — node 37 exact +
3 membership + 5 visible skips, 0 divergences; containment 1935 checks, 0
violations; ejs 32 ok / 1 N/A / 7 known / 0 new / 0 stale.

## Review round 3 (nits R1/R2, maam c3d1aed)

- **R1**: destructuring-pattern leaves captured at-or-before their
  declaration were still silently wrong with zero counters
  (`var f = function () { a = 9; }; var [a, b] = [1, 2]; f(); a;` →
  concrete 1, real JS 9).  Same remedy as F2: the normalizer records a
  degradedBinding (harness precondition trips; skip-pattern-leaf-capture.js
  proves the visible skip; declare-then-capture leaves pinned as
  non-degrading; identifier-declared names excluded — the modeled path
  owns them).  Suite 266 → 268; corpus 45 → 46.
- **R2**: the capture scan early-returned after params + body, missing
  closures inside old-esprima/echojs-dialect `defaults` expressions —
  unreachable via acorn but reachable through echojs post-desugar trees.
  The scan now covers `defaults`, treats dialect `rest` as a parameter,
  and collects param BINDING names via pattern leaves (an ES6 default's
  right-hand side is an expression, not a binding).  Pinned with a
  hand-built dialect tree (a default-closure writing a later var now
  computes, instead of silently dropping the write).

Final harness numbers at c3d1aed (all lanes): corpus 46 — node 37 exact
+ 3 membership + 6 visible skips, 0 divergences; containment 1935
checks, 0 violations; ejs 32 ok / 1 N/A (arith-basic.js, esprima `**`) /
7 known / 0 new / 0 stale.  Suite 268.

## The `--types` diff lane re-run (oracle facts changed ⇒ re-measured)

The P3.5 normalizer/machine fixes change what the oracle reports, so the
lane was re-run on the final pin (maam 3e64ca1; work tree assembled from
`//:srcdir-tree` + `//lib:generated` + repo `test/`, conc 4, logs
`~/.cache/maam-p0-logs/P35-types-diff/`).  These numbers SUPERSEDE the
Phase 3 figures (67 diamonds / 1319 queries / 1035 unknown) and the
review's interim c69fc81 run (66 / 1306 / 866):

| files | identical | divergent | N/A | timeouts |
|---|---|---|---|---|
| 458 | 454 (+3 serial re-verifies = 457) | **0** | 1 (tester.js, standing esprima gap) | 3 transient (closure2/4/7, concurrency artifact — each re-verified IDENTICAL serially, same as the review run's 4) |

Aggregates: **diamonds 69** (baseline 67, interim 66), oracleQueries 1320,
**oracleUnknown 866** (baseline 1035).  Reading: the ⊥-receiver fix and
exact string `.length` give the oracle MORE precise facts (unknown down
~16%, two extra diamonds); the hoisted-capture `undefined` widening on
captured vars did not cost a diamond on this corpus.  The behavioral gate
is unchanged: zero divergence, flag-off untouched.

Re-run once more on the round-3 pin (maam c3d1aed, logs
`~/.cache/maam-p0-logs/P35-types-diff-r3/`): **458 files, 457 identical,
0 divergent, 1 N/A (tester.js), 0 timeouts — LANE PASS**, aggregates
byte-for-byte the same (diamonds 69 / queries 1320 / unknown 866): the
R1 accounting and R2 defaults-scan changed no oracle facts on this
corpus.
