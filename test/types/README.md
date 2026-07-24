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

Shapes probes (shapes-plan P4.3; `shapeGuards=N` from the stats line
counts has_shape diamonds the way `diamonds=N` counts has_tag ones):

| probe | shape | shapeGuards | vs node |
|---|---|---|---|
| types-bench2 | the object-model microbenchmark: monomorphic constructor + p.x/p.y kernel; guarded fast paths + shape-region merging (`shapeGuards=10`, 2 shape regions merged); 2026-07-24 numbers: --types 3.06s vs flag-off 6.56s (2.1×), vs EJS_SHAPES=off 5.82s (~1.9× shapes-attributable); P4.4 born-with-shape (`ctorFills=1`) takes it to **2.03s** vs flag-off 6.76s (3.3×) | 10 | match |
| types-shapeswrong1 | the wrong-oracle shape guard: lib types sumxy's receiver {x: num, y: num} from its one local call; main hands it a repr-mismatched object ("ab"), an extra-field object, and a dictionary-mode (post-delete) object → all route slow with node-identical values; the matching Point goes fast | 4 (in lib) | n/a¹ |
| types-bornshape1 | born-with-shape (P4.4): a static literal is make_object_shaped, the Pt ctor prefix is the empty-shape-guarded fill (`bornShaped=1 ctorFills=1`); keys order, `in`, growth past the born shape, and a repr-differing construction all match node | 0 | match |
| types-bornshapewrong1 | P4.4 edge cases: a reused non-empty receiver (guard fails), an `in`-cut fence, a frozen receiver (runtime re-check), a proto-chain SETTER intercepting the batched store, and a non-writable proto data prop — every one routes sequential with node-identical output (`bornShaped=3 ctorFills=3 fenceDeclined=short-prefix:1`); also found the provenNumberIntrinsic const-join gap (which P4.5's typed stores later dissolved entirely) | 0 | match |
| types-typedslots1 | typed slots (P4.5): the fused kernel fast on the matching shape, slow on repr-mismatched / extra-field / dictionary receivers; -0 (1/x sign), NaN, Infinity bit-survival through raw slot store→load; a mid-kernel repr-flip transition (string into an f64 field) and the boxed-field store paths (`shapeGuards=9 shapeTyped=loads:7,stores:1 bornShaped=3 ctorFills=2`); node-identical incl. under EJS_SHAPES=off and EJS_GC_EVERY_N_ALLOC=7 | 9 | match |

¹ node cannot execute this file's bare-ESM import layout from test/;
the check here is flag-off vs `--types` executables producing identical
output (verified — and the slow-path routing is the probe's point).

Wider context (the `--types` diff lane over all of `test/`, 2026-07-22):
458 files, 457 identical flag-off vs `--types`, 0 divergent, 1 N/A
(tester.js, esprima parse gap), 67 diamonds total across the suite.
Suite files are string/object-heavy by design — the diamond count is
expected to be modest outside numeric kernels.

P4.3 re-run (2026-07-24, shapes guards live): 459 files, 458 identical,
0 divergent, 1 N/A (tester.js), 78 diamonds.  Shape telemetry across
the suite: 13,154 access sites consulted, 809 guarded; declines:
unmapped 7,575 / capped 4,287 / empty 269 / no-field 194 /
polymorphic 12 / union-repr 8 — same story: guards fire in kernels,
the string-heavy suite mostly declines (visibly, per reason).
