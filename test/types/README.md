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
| types-wrongoracle1 | the wrong-oracle guard: lib.js types `inc`'s param {number} from its only local call, main calls `inc("x")` cross-module → slow path, "x1" (since runtime-P2 lib also reports `specWrapped=1` — the exported inc gets the boundary wrapper; the string still routes generic through its guard chain) | 2 (in lib) | n/a¹ |
| types-bench1 | the Phase 3 microbenchmark kernel (adds/muls/divs/compares over typed locals) | 9 | match |
| types-spec1 | Phase 3.6 specialization: module-local looping kernel → f64(f64) clone, exact-arity sites rewritten to call_typed (`specialized=1 specSites=2`); the extra-arg site stays generic | 6 | match |
| types-spec2 | Phase 3.6 cross-function specialization (the hypot2-demo shape): hypot2 called only inside sum, prefix-safe toplevel slot stores → both clone, all four sites rewrite incl. the one inside sum$typed (`specialized=2 specSites=4`) | 7 | match |
| types-specescape1 | Phase 3.6 escape rejection: f LOOKS numeric-closed but its closure is passed as a call argument → NOT trusted-specialized (`specialized=0`); since runtime-P2 the escapee gets the boundary wrapper instead (`specWrapped=1`), and the escaped string call fails its guard chain onto the generic path | 5 | match |

Shapes probes (shapes-plan P4.3; `shapeGuards=N` from the stats line
counts has_shape diamonds the way `diamonds=N` counts has_tag ones):

| probe | shape | shapeGuards | vs node |
|---|---|---|---|
| types-bench2 | the object-model microbenchmark: monomorphic constructor + p.x/p.y kernel; guarded fast paths + shape-region merging (`shapeGuards=10`, 2 shape regions merged); 2026-07-24 numbers: --types 3.06s vs flag-off 6.56s (2.1×), vs EJS_SHAPES=off 5.82s (~1.9× shapes-attributable); P4.4 born-with-shape (`ctorFills=1`) takes it to **2.03s** vs flag-off 6.76s (3.3×) | 10 | match |
| types-shapeswrong1 | the wrong-oracle shape guard: lib types sumxy's receiver {x: num, y: num} from its one local call; main hands it a repr-mismatched object ("ab"), an extra-field object, and a dictionary-mode (post-delete) object → all route slow with node-identical values; the matching Point goes fast | 4 (in lib) | n/a¹ |
| types-bornshape1 | born-with-shape (P4.4): a static literal is make_object_shaped, the Pt ctor prefix is the empty-shape-guarded fill (`bornShaped=1 ctorFills=1`); keys order, `in`, growth past the born shape, and a repr-differing construction all match node | 0 | match |
| types-bornshapewrong1 | P4.4 edge cases: a reused non-empty receiver (guard fails), an `in`-cut fence, a frozen receiver (runtime re-check), a proto-chain SETTER intercepting the batched store, and a non-writable proto data prop — every one routes sequential with node-identical output (`bornShaped=3 ctorFills=3 fenceDeclined=short-prefix:1`); also found the provenNumberIntrinsic const-join gap (which P4.5's typed stores later dissolved entirely) | 0 | match |
| types-typedslots1 | typed slots (P4.5): the fused kernel fast on the matching shape, slow on repr-mismatched / extra-field / dictionary receivers; -0 (1/x sign), NaN, Infinity bit-survival through raw slot store→load; a mid-kernel repr-flip transition (string into an f64 field) and the boxed-field store paths (`shapeGuards=9 shapeTyped=loads:7,stores:1 bornShaped=3 ctorFills=2`); node-identical incl. under EJS_SHAPES=off and EJS_GC_EVERY_N_ALLOC=7 | 9 | match |
| types-bench3 | the P4.6 polymorphic microbenchmark: the bench2 kernel with two receiver classes ({x,y} / {z,x,y}) alternating at one site — the 2-way guard chain (`shapePolyGuards=4`) runs 0.31s, PARITY with the monomorphic twin, vs 1.67s declined (EJS_NO_POLY_SHAPE_GUARDS=1) and 3.64s flag-off (2026-07-24, M-series) | 4 (poly) | match |
| types-poly1 | the wrong-oracle probe for the 2-way chain: lib's oracle types sum/setx's receiver with BOTH terminal shapes from local calls (`shapePolyGuards=4 shapeTyped=loads:6,stores:2`); cross-module receivers it never saw — repr-mismatched, a third shape, dictionary-mode (post-delete) — all route through the shared slow path; identical output incl. under EJS_SHAPES=off and EJS_GC_EVERY_N_ALLOC=7 | 4 (in lib) | n/a¹ |

P4.6 evidence probes (extensions measured and NOT landed; the numbers
and rationale live in shapes-plan.md's P4.6 entry):

| probe | shape | vs node |
|---|---|---|
| types-accessor1 | proto-getter dispatch kernel, 20M `p.len2` reads (accessor inlining: ~7× headroom recorded, DECLINED pending proto-guard soundness machinery) | match |
| types-array1 | dense-array element kernel, 20M `a[j]` reads (element shapes: 2.4× headroom vs flag-off recorded, DEFERRED — arrays are outside shaped mode) | match |

runtime-P2 probes (export-boundary wrapper + escape-taint fence,
2026-07-29; `specWrapped`/`specFenced` from the stats line count
boundary wrappers installed and call sites the taint fence kept
generic):

| probe | shape | stats | vs node |
|---|---|---|---|
| types-wrapper1 | the exported kernel: never trusted-specialized, but wrapped — has_tag guards at the generic entry dispatch to an UNTRUSTED guarded f64 clone (folds structurally from the entry boxes).  Cross-module number calls take the clone; a string and a missing arg fail the chain onto the generic body | `specWrapped=1` (in lib) | n/a¹ |
| types-wrapperfence1 | the escape-taint fence: module-private g looks closed-world numeric but one call site is hosted in the exported f; maam's constant-propagation domain prunes g's `y>5` branch under the analyzed 3, so a trusted rewrite of that site would unbox `"s"` unguarded on f(7) — the fence keeps it generic (f(7) → NaN, node-identical); the init-time site still rewrites to g$typed | `specialized=1 specSites=1 specFenced=1 specRejected=1`² (in lib) | n/a¹ |
| types-bench5 | the types-bench1 workload with the kernel EXPORTED and called cross-module: flag-off 0.34 s → 0.07 s user with the wrapper (~4.9×), PARITY with types-bench1's closed-world trusted path (0.07 s) — the module boundary costs one has_tag per formal per call | `specWrapped=1` (in lib) | n/a¹ |

² the `specRejected` there is f's own wrapper declining on the payoff
check (its body is a bare delegation call — no diamonds to fold), not a
failure.

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
