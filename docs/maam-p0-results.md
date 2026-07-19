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
