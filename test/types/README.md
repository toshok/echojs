# --types probe census (Phase 3 / Phase 3.6)

Probe files for the oracle-guided typed-arithmetic fast path
(docs/maam-plan.md, Phase 3) and for function specialization
(Phase 3.6).  Like `test/modernization/`, these live
OUTSIDE the tester's `*<digit>.js` discovery glob in `test/` itself
(subdirectories are not scanned) and are runnable standalone: compile one
with the node-hosted compiler and `--types`, run it, and diff stdout
against `node <file>` (color-free: `NO_COLOR=1`, `FORCE_COLOR` unset).
The whole-suite behavioral gate is `./buck-test-types-diff.sh`.

`diamonds=N` below is the count from the `--types` stats line — how many
guarded has_tag/f64 diamonds lowering emitted for the file.  The guard
makes every diamond correct regardless of oracle accuracy; these probes
document where the fast path FIRES.

Census as of 2026-07-22 (echojs @ 568efc7, maam @ 8d6a157):

| probe | shape | diamonds | vs node |
|---|---|---|---|
| types-locals1 | pure numeric locals (`+ - * / <`) | 6 | match |
| types-params1 | numeric params, module-local call sites (incl. 1/0 → Infinity through the fast fdiv) | 4 | match |
| types-literals1 | literals mixed with typed vars (incl. the unary-minus literal parse `x - -2`) | 5 | match |
| types-widen1 | reassignment widening: num→str and undefined→num bindings do NOT diamond (documented; only exact {number} qualifies) | 0 | match |
| types-loops1 | for/while counters, `<` in loop conditions | 6 | match |
| types-wrongoracle1 | the wrong-oracle guard: lib.js types `inc`'s param {number} from its only local call, main calls `inc("x")` cross-module → slow path, "x1" | 1 (in lib) | n/a¹ |
| types-bench1 | the Phase 3 microbenchmark kernel (adds/muls/divs/compares over typed locals) | 9 | match |
| types-spec1 | Phase 3.6 specialization: module-local looping kernel → f64(f64) clone, exact-arity sites rewritten to call_typed (`specialized=1 specSites=2`); the extra-arg site stays generic | 6 | match |
| types-spec2 | Phase 3.6 cross-function specialization (the hypot2-demo shape): hypot2 called only inside sum, prefix-safe toplevel slot stores → both clone, all four sites rewrite incl. the one inside sum$typed (`specialized=2 specSites=4`) | 7 | match |
| types-specescape1 | Phase 3.6 escape rejection: f LOOKS numeric-closed but its closure is passed as a call argument → NOT specialized (no `specialized=` in stats); the escaped call feeds a string through the generic path | 2 | match |

¹ node cannot execute this file's bare-ESM import layout from test/;
the check here is flag-off vs `--types` executables producing identical
output (verified — and the slow-path routing is the probe's point).

Wider context (the `--types` diff lane over all of `test/`, 2026-07-22):
458 files, 457 identical flag-off vs `--types`, 0 divergent, 1 N/A
(tester.js, esprima parse gap), 67 diamonds total across the suite.
Suite files are string/object-heavy by design — the diamond count is
expected to be modest outside numeric kernels.
