# MAAM integration plan: a type oracle for EIR

How the `echojs-maam` abstract interpreter (submodule at
`external-deps/echojs-maam`, branch `ejs-integration`) feeds types into EIR
lowering, in independently-landable phases, without restructuring either
codebase.

## Context: what echojs-maam is, as found

`echojs-maam` ("maam-fable") is a ~7k-line TypeScript transliteration of
Darais/Might/Van Horn's MAAM — one definitional CESK* interpreter
(`src/lang/machine.ts`) run under different monads to get concrete evaluation,
k-CFA, and path/flow/flow-insensitive analyses. It consumes **standard ESTree**
(`analyze(program, spec)` in `src/analysis.ts`; zero runtime deps — acorn is
dev-only, behind `src/lang/parse.ts` which is deliberately excluded from the
build). The `ejs-integration` branch already models the shapes echojs's
pre-EIR desugars emit: `%objectCreate`, `%setPrototypeOf`,
`%setConstructorKind*`, `%constructSuper`, and the
`Object.defineProperty` method/accessor patterns (`test/echojs-shape.test.ts`
hand-builds exactly those trees). 192 tests pass under `npm test`.

It is research-quality and honest about it: exceptions are control-only,
cross-module linking is unmodeled (imports degrade to `undefined`), calls to
unmodeled externals degrade and are *counted* (`metrics.unknownCalls`), and the
Octane corpus shows the heavier benchmarks need the widening knobs
(`stateCap`/`shapeCap`) to converge. What it computes is exactly what we want:
type-aware hidden classes per allocation site (`result.layouts()` — field
names, `TypeSig`s, struct offsets), per-function `(param types) → return type`
tables (`result.specializations()`), accessor-dispatch sites, and — via
`concreteEval()` — a genuine concrete interpreter usable as a differential
oracle. What it does **not** yet have is a per-AST-node type query:
`valueOfVar(name)` joins by *name* across all configs, and core locations map
only to source *spans* — which echojs trees don't carry (see mismatches below).
That query is the main new surface this plan adds, on the maam side.

On the echojs side the seams already exist by design: `lib/eir/ops.ts` is the
declared effect-table contract ("the optimizer and the abstract interpreter"),
including an unused low tier (`has_tag`, `unbox_f64`, `box_f64`,
`f64_add/sub/mul/div/lt`); `lib/eir/scopes.ts` keys its `refs` map on AST nodes
(`Map<e.Node, Binding|null>`); `lib/eir/lower.ts` lowers `BinaryExpression` in
one place (`LowerFunction.binary`, the `binops` table) and every variable
read/write through `readVariable`/`writeVariable`; `Inst.type` in
`lib/eir/ir.ts` is an `"any"` placeholder awaiting the lattice. Note:
`lib/eir/emit.ts` does **not** yet implement the low-tier ops — that is a
prerequisite phase, pure echojs work.

## ESTree dialect mismatches (echojs `lib/estree.ts` vs. what maam reads)

Found by inspection; the adapter (Phase 0) must handle each:

1. **`TryStatement.handlers` (array) + `guardedHandlers`** vs. standard
   `handler`. `normalize.ts:313` reads `s.handler` — echojs trees would
   silently drop every catch clause. Fix in maam: accept both.
2. **No `range`/`start`/`end` on nodes.** echojs parses with
   `esprima.parse(src, {loc: true, raw: true})` (`lib/passes/gather-imports.ts:275`);
   maam's `spanOf` falls back to `{0,0}`. Consequence: spans cannot key
   anything; the oracle must be **node-identity** keyed (same tree, in
   process). Synthetic desugar nodes have no positions at all.
3. **Functions carry `defaults`/`rest`** (old-esprima style) instead of
   `AssignmentPattern`/`RestElement` params. maam must evaluate `defaults`
   (echojs EIR handles them natively; the analysis must match).
4. **`MetaProperty.meta/property` are raw strings**, not Identifiers — moot
   post-desugar (`DesugarMetaProperties` removes them).
5. **Toplevel wrapper**: at `collectEIRToplevel` time the Program body is one
   synthetic `FunctionDeclaration` (from `insert_toplevel_func`) whose body
   holds the module statements, including `ImportDeclaration` /
   `ExportNamedDeclaration` wrappers. The adapter analyzes
   `{type:"Program", body: toplevel.body.body}` (preserving node identity)
   and must tolerate export wrappers inline.
6. **Intrinsic coverage gap**: echojs's whitelist (`lib/eir/intrinsics.ts`)
   includes `%arrayFromSpread`, `%constructSuperApply`, `%constructApply`,
   `%getNewTarget`, `%makeGenerator`/`%generatorYield`/…,
   `%createIteratorWrapper`; maam models only the class/prototype set. Unknown
   intrinsics must degrade *soundly* (to ⊤, see below), never throw.

## The interface contract

echojs side, new file `lib/eir/oracle.ts` (the only new echojs surface):

```ts
// what lowering consumes; deliberately smaller than what maam computes
export type TypeTag = "number" | "string" | "boolean" | "undefined"
                    | "null" | "object" | "closure";
export interface EirType {
    tags: ReadonlySet<TypeTag> | "top";   // "top" = no information
}
export interface TypeOracle {
    // type of the value an expression node evaluates to (join over all
    // reached contexts); "top" when unknown/unanalyzed
    typeOfNode(n: e.Node): EirType;
    // true iff metrics.unknownCalls === 0 — required before any
    // UNguarded consumption (guarded fast paths don't need it)
    closedWorld(): boolean;
    describe(): string;                    // stats line for --types logging
}
```

maam side (`ejs-integration` branch), additions to `AnalysisResult`:

```ts
// node-identity keyed; built by having normalize.ts record the source
// node (not just its span) at the same points it records siteSpans,
// plus declaration-node → alpha-renamed core Name for bindings
nodeTypes(): ReadonlyMap<estree.Node, TypeSig>;   // e.g. "num", "num|str", "⊤"
typeOfNode(n: estree.Node): TypeSig | undefined;
```

plus one semantic fix: **degrade to ⊤, not `undefined`**.
`machine.ts` currently binds unknown-call results and unmodeled imports to
`domain.lit(litUndef)` (`machine.ts:1296,1355`) — fine for reachability,
**unsound as a type** ("this is undefined" vs. "this is anything"). Add a
`domain.top` to `ValDomain` and use it in `degrade`. This is the one
non-additive maam change and it lands first.

## Phases

**Phase 0 — plumbing probe (consume nothing).**
Add `--types` to `lib/options.ts` (default off). When on, `compile()` in
`lib/compiler.ts` — after `pre_eir_convert`, before `collectEIRToplevel` —
calls a thin adapter (`lib/eir/oracle.ts`) that imports maam, wraps the
toplevel body as a Program, runs `analyze(prog, kCFA(1, "flow-sensitive",
"call-site", /*shapeCap*/ 64, false, false, false, /*stateCap*/ 512))`,
and logs `result.describe()` + `metrics` + wall time. Nothing downstream reads
it; a crash or `RestrictionError`/`NormalizeError` degrades to a warning, never
a compile error. maam-side deliverables: `handlers` shim, `defaults`/`rest`
handling, unknown-intrinsic tolerance. The real point: **measure** whether
compiler-sized modules converge and at what cost, on our actual sources.
*Gate:* full matrix green with flag off (`buck2 build //:test-eir
//:test-stage0 //:test-stage1 //:test-stage2 //:test-stage3`); stage2≡stage3
per the functional gate under "Validation strategy" (raw binary byte-identity
does NOT hold today even on pristine HEAD — buck stages link in per-genrule
temp dirs, so LC_UUID/embedded-path/signature metadata differs; discovered
during Phase 0); `ejs --types` over `test/*.js` and `lib/*.ts`'s generated JS
reports stats without crashing — run via the node-hosted dev tree:
buck-staged work trees contain no `external-deps/`, so `--types` there
warns-and-skips by design.

**Phase 1 — the node-keyed oracle.**
maam: ⊤-degradation; `normalize.ts` records `Loc → estree.Node`;
`analysis.ts` exposes `nodeTypes()`/`typeOfNode()`; unit tests in maam's suite
(node-identity round-trip through hand-built echojs-dialect trees, extending
`test/echojs-shape.test.ts`). echojs: `TypeOracle` adapter mapping `TypeSig`
strings to `EirType`; `--types` now also prints per-binding types for a
`--types-dump` debug flag. Still consumes nothing in codegen.
*Gate:* matrix green (flag off); new maam tests green; oracle dump on
`test/eir-toplevel1.js` matches hand-checked expectations.

**Phase 2 — emit the low tier (echojs only, independent of maam).**
Implement `has_tag`, `unbox_f64`, `box_f64`, `f64_add/sub/mul/div/lt` in
`lib/eir/emit.ts` (NaN-boxing checks mirror `LLVMIRVisitor.isNumber` in
`lib/compiler.ts`); teach `lib/eir/verifier.ts` that f64/i1-typed values may
only flow into their consumers (`Inst.type` gets its first real values:
`"f64"`, `"i1"`); unit tests in `lib/eir/tests.ts` (`buck2 build //:test-eir`)
via hand-built `FunctionBuilder` functions asserting printer/verifier/emit
behavior, plus one end-to-end test file exercising a hand-forced fast path.
*Gate:* `//:test-eir` green; full matrix green (no lowering changes yet).

**Phase 3 — typed arithmetic, born in lowering, guarded, flag-gated.**
`LowerFunction.binary()` consults the oracle (threaded through
`lowerAnalyzedFunction` from `collectEIRToplevel`; `null` oracle = today's
behavior). When both operands' types ⊑ number for `+ - * / <`, emit the
guarded diamond (same block-splitting shape as `LowerFunction.logical()`):
`has_tag` both → fast block `unbox_f64/f64_op/box_f64` → join blockparam;
slow block keeps the generic op. **Guarded consumption is correct even if the
oracle is wrong** — the guard decides at runtime; only code size/speed change.
Unguarded (guard-free) emission stays out until `closedWorld()` plus much more
validation. Why born-typed rather than a post-hoc `optimize.ts` pass: the
rewrite needs CFG surgery (block split + join params), which lowering already
does idiomatically, while `optimize.ts` is a flat in-place scanner — a post-hoc
pass would be *more* code, not less. (A post-hoc pass remains attractive later
for typing *optimizer-created* values; nothing here precludes it.)
*Gate:* matrix green flag-off, stage2≡stage3 functional gate; a `--types` lane:
compile the full `test/` suite with `--types` under the node-hosted compiler
and diff every output against the flag-off baselines (byte-identical stdout);
`test/modernization/`-style probe discipline for a new `test/types/` dir
(each file diffed against `node <file>`); an arithmetic microbenchmark
demonstrating the fast path fires.

**Phase 4 (outline only) — shapes.**
`result.layouts()`/`constructors()` give monomorphic allocation sites with
struct offsets. Consuming them (fixed-offset property access) needs a shape
guard op and runtime object-layout support that don't exist; design that as
its own document once Phase 3 has proven the pipeline. Until then, shapes
inform *diagnostics* only (polymorphism warnings under `--types-dump`).

## Self-hosting strategy

The constraint: stage1+ compilers are the compiler compiled by itself, and the
esprima fork parses ES6-era JS only. maam's own source is strict TS 5.x using
`??`, `?.`, and generators; `tsc` at `target: ES2022` leaves `??`/`?.` in the
output, which the esprima fork cannot parse — so maam **cannot run under a
self-hosted compiler today**. Options weighed:

- **(a) Vendored babel-downleveled build** — precedented (docs/plans.md
  proposes exactly this for parser un-forking), and `//lib:generated` already
  runs babel. Viable, but it drags a second-build-of-a-submodule into the
  bootstrap now, for zero benefit while the flag is off.
- **(b) Node-only analysis, flag off during bootstrap** — stages remain
  byte-identical trivially (the flag is off everywhere in the matrix); `--types`
  is available wherever the compiler runs under node (stage0 and dev use).
- **(c) Syntax-downlevel pass in echojs** — that's the modernization project
  (`test/modernization/`, 13 parser gaps), not this one.

**Recommendation: (b) now, (a) when promotion is wanted.** Concretely: Phase 0
imports maam via a `tsc -p tsconfig.build.json`-built `dist/` (an ESM/CJS
interop wrinkle exists — maam is `"type": "module"`, the babel'd compiler tree
is CJS under node 22.4; a `tsconfig.cjs.json` variant in the maam repo is the
one-file fix). Promotion to self-hosted `--types` waits until either the
babel-vendored build (a) or the TS port + parser modernization make it moot.
Until promotion, `--types` in a stage1+ compiler is a no-op with a warning.

## Validation strategy

- **Bootstrap matrix, every phase:** `//:test-eir`, `//:test-eir-lowtier`
  (standalone — must be named explicitly; stage-green does not imply it
  ran), `//:test-stage0..3`.
  stage2≡stage3 is a *functional* gate, not raw byte-identity (which fails on
  pristine HEAD from link metadata alone): stage2 and stage3 binaries, run in
  identical work dirs over a fixed corpus, must produce byte-identical
  outputs, and any stage2-vs-stage3 binary diff must be attributable to link
  metadata (`cmp` after `codesign --remove-signature` + masking LC_UUID, or
  diff the `--leave-temp` .ll artifacts). Executable byte-compares are only
  meaningful when both binaries were linked in the same directory. Flag-off
  means MAAM cannot regress the matrix.
- **Concrete interpreter as differential oracle:** `concreteEval()` is real
  and exact (`analyze(prog, concreteEval()).result`). Add a maam-repo harness
  that runs closed-world test files (start with `intrinsics: true` to cover
  `Math`/`Array`/`parseInt`) and diffs the final value against `node` — any
  divergence is a machine bug that would poison the abstract results too.
  Precondition per file: `metrics.unknownCalls === 0`, else skip (degradation
  makes the diff meaningless). Also diff against `ejs`-compiled output for the
  subset both support — that checks *echojs* too, for free.
- **Abstract-vs-concrete containment spot checks:** for files the concrete
  interpreter handles, assert the k-CFA `typeOfNode` at each checked node is ⊒
  the concrete value's type — cheap soundness fuzzing, catches ⊑-direction bugs.
- **`lib/eir/tests.ts`:** low-tier emit/verify (Phase 2), oracle-driven
  lowering shape (Phase 3: assert the printed EIR contains
  `has_tag`/`f64_add` diamonds for a numeric snippet, and does not for a
  string one).
- **`--types` diff lane (Phase 3 gate):** entire `test/` suite compiled with
  and without `--types`; outputs must be byte-identical.

## Risks and unknowns

- **Convergence on compiler-sized inputs.** Octane's heavier files don't
  converge in tens of seconds; `lib/compiler.ts`'s generated JS is bigger.
  `stateCap`/`shapeCap` bound time but cost precision. Phase 0 exists to turn
  this unknown into a number before anything depends on it.
- **Degradation soundness.** Unknown calls/imports currently read as
  `undefined`; consuming that as a type would miscompile. Fixed in Phase 1
  (⊤-degradation) and defended in depth by guarded-only consumption.
- **Unvalidated soundness claims.** The analyzer's soundness is asserted by
  its own tests, not proven against ejs semantics (e.g. ejs's no-TDZ let/const,
  `to_boolean` purity). Guards make Phase 3 immune; anything unguarded needs
  the differential harness first.
- **Node-identity coupling.** The oracle keys on the exact post-desugar tree
  object; any future pass that clones nodes between analysis and lowering
  silently drops types (fail-soft to ⊤, but worth a debug counter).
- **ESM/CJS interop** for importing maam's build from the babel'd tree
  (node 22.4 pinned in CI — no `require(esm)`).
- **Two-repo coordination.** The submodule pin advances with the oracle API;
  phases state which repo each deliverable lands in to keep either repo
  releasable alone.

## Future work: can this architecture become production-grade?

Assessment (Claude, 2026-07-11), recorded here so the Phase 0 numbers get read
against an explicit hypothesis rather than vibes.

**The pessimistic reading is correct about the engine.** Small-step monadic
AAM/CESK* is close to the most expensive known way to compute a flow analysis:
every step pays monad plumbing, the state space is the product of
control × store × continuation abstractions, and the caps that force
convergence (`stateCap`/`shapeCap`) buy termination by discarding exactly the
precision we want to consume. The industrial abstract interpreters that ship
(Infer, Astrée) are compositional/summary-based engines, not small-step
machines. "This exact machine, flow-sensitive, over 100k-line modules, in
seconds" is not a realistic endpoint, and no micro-optimization changes that.

**But the deployment profile is unusually favorable**, which is why the
architecture is worth keeping anyway:

1. *AOT oracle, not IDE/CI.* Offline, deterministic, whole-program, behind an
   opt-in flag — seconds-to-a-minute of compile time is tolerable.
2. *Wrong answers cost speed, not correctness.* Guarded consumption makes
   precision an optimization, not an obligation.
3. *The input language is tiny.* Not "JavaScript" — the post-desugar echojs
   dialect: no TDZ, whitelisted intrinsics, no eval/with/dynamic loading.

**The durable asset is the definitional machine as reference semantics.**
`concreteEval()` as a differential oracle is something hand-optimized
analyzers never have. The classic path from research analyzer to product is
exactly this split: keep the slow, obviously-correct machine as the spec, and
if (and only if) Phase 0 measurements demand it, grow a fused fixpoint engine
— worklist, flow-insensitive-then-refine, or per-function summaries
(`specializations()` already gestures at summaries) — that shares the domain
definitions and is continuously diffed against the reference. Rewrite the
fixpoint loop, never the semantics.

**The `TypeOracle` interface is the insurance policy.** It is deliberately
smaller than what maam computes and keyed on nodes, not maam internals. If
convergence on compiler-sized inputs is unacceptable, the engine behind
`typeOfNode()` is swappable — a monovariant Andersen-plus-type-lattice pass
would cover the Phase 3 arithmetic use case at a fraction of the cost — and
lowering never knows. Nothing should be built that assumes the MAAM machine
specifically sits behind the oracle.

**Decision rule:** let the Phase 0 measurement, not aesthetics, make the call.
Three outcomes: (a) converges with acceptable cost on our corpus → ship as-is
behind `--types`; (b) converges only with heavy widening → keep it for
diagnostics/differential duty, start the fused engine sharing its domains;
(c) doesn't converge → oracle interface stays, engine is replaced outright.

Smaller forward items surfaced by the Chunk A integration review:

- `metrics.unknownCalls` was broadened during integration (method/apply/
  tailcall degradations now count, not just call/new). This is what the
  Phase 1 `closedWorld()` contract needs, but it changes the metric's
  definition out from under the numbers in the maam repo's docs/paper —
  regenerated tables will shift, and any comparison must say so.
- Rest parameters are degraded (bound to an empty abstract array + counted),
  not modeled. Precise rest needs a core/machine varargs extension — a
  natural Phase 1 companion to ⊤-degradation.
- Guarded-catch guards are modeled per-clause; cross-clause guard side
  effects (guard 1 mutates, clause 2 observes) are dropped. Unreachable from
  echojs output (esprima always emits `guardedHandlers: []`) — revisit only
  if that changes.
- `tryIntrinsic` dispatches on the `%` name prefix with a `scope.has()`
  escape for bound names (`%super`). If echojs ever grows more bound
  `%`-names or direct `%super(...)` calls, the intrinsic whitelist in both
  repos needs to stay in sync — a shared fixture file is the eventual answer.

## What we explicitly will NOT do

- No parser swap or syntax modernization as part of this (tracked separately
  in docs/plans.md).
- No runtime changes beyond what the already-declared low-tier ops need; no
  new object layouts, no shape guards, no GC work.
- No rewrite or restructuring of maam's monad/driver machinery, and no
  EIR-targeting maam frontend (it keeps consuming ESTree; the ANF core stays).
- No post-hoc "type inference pass" duplicated inside echojs — types come from
  the oracle or stay `any`.
- No default-on behavior anywhere until the differential harness and the
  `--types` diff lane have real mileage.

## Phase checklist (for /goal sessions)

- [x] **P0** `--types` flag + `lib/eir/oracle.ts` adapter + maam dialect shims
      (`handlers`, `defaults`/`rest`, unknown-intrinsic tolerance, toplevel
      unwrap); stats logging only.
      *Done 2026-07-19* (maam 1bea5de; echojs d3e3fd1): all gates green,
      numbers in docs/maam-p0-results.md. Headline finding: convergence on
      compiler-sized modules is STILL OPEN — blocked on maam normalizer
      coverage (TemplateLiteral/ForOfStatement/destructuring params = 100%
      of the compiler-module rejects), not on the engine; where analysis
      runs, it converges with zero timeouts and analysis cost is noise next
      to codegen.
      *Gate:* full matrix green (flag off); `--types` runs over `test/*.js`
      without crashing (node-hosted dev tree — buck work trees have no
      `external-deps/`, so `--types` there warns-and-skips by design);
      convergence/timing numbers recorded in the PR.
- [x] **P1** maam: ⊤-degradation + `nodeTypes()`/`typeOfNode()` (node-identity
      keyed); echojs: `TypeOracle` + `--types-dump`. Per the P0 results, P1
      should FRONT-LOAD maam normalizer coverage for TemplateLiteral,
      ForOfStatement, and destructuring/defaults/rest params (these block
      every compiler-sized module), surface cap-hit counters in `metrics`
      (saturation is currently unobservable), then re-run the P0 measurement
      to close the convergence question.
      *Done 2026-07-21* (maam 8d6a157; echojs this commit). Convergence
      question CLOSED: outcome (a) — compiler-sized modules converge
      naturally (0 stateCap hits; self-compile analyzes 44/45 modules, sole
      remainder lib/runtime's non-literal defineProperty key); numbers in
      docs/maam-p0-results.md. Oracle contract notes: `typeOfNode` on a
      node mapped to a declared variable reports the join over the
      variable's whole lifetime (reassignment-widening — sound, not
      value-at-site); spliced/shared node objects are poisoned to
      `undefined` (consumer degrades to ⊤); `closedWorld()` requires BOTH
      `unknownCalls` and `degradedBindings` zero. Known ⊤ classes on real
      trees: unmodeled imports, unknown intrinsics/method calls
      (intrinsics=false), unreached code, and array patterns in ALL
      positions (declaration, param, assignment) — DesugarDestructuring
      routes every array pattern through `%createIteratorWrapper` before
      the probe, so maam's native pattern paths are exercised only by its
      own tests; modeling that intrinsic (or reordering the desugar) is the
      obvious next precision win for P2/P3.
      *Gate:* maam suite green (incl. new node-identity tests); matrix green;
      hand-checked oracle dump for `test/eir-toplevel1.js`.
- [x] **P2** emit + verify `has_tag`/`unbox_f64`/`box_f64`/`f64_*`;
      `Inst.type` carries `"f64"`/`"i1"`.
      *Gate:* `//:test-eir` green with new low-tier tests; matrix green —
      the matrix line now includes `//:test-eir-lowtier` (standalone
      target; it does NOT ride the stage targets), which compiles
      test/eir-lowtier1.js plain + under `EJS_EIR_LOWTIER=1` injection and
      asserts output parity, binary divergence, and per-op IR presence
      (fadd/fsub/fmul/fdiv/fcmp olt) — also a stale-llvm.node detector.
      *Done 2026-07-21.* Notes: node-llvm needed new FP bindings
      (createFSub/FMul/FDiv/FCmpOLT — only FAdd existed); `has_tag
      "number"` delegates to LLVMIRVisitor.isNumber (icmp ult against
      EJSVAL_SHIFTED_TAG_INT32 — the int32 tag exists in the ejsval layout
      but is never minted, and the threshold excludes it, so
      unbox-as-raw-double is safe by construction); raw f64/i1 may NOT
      cross block boundaries as edge args (Phase 3 diamonds rejoin boxed);
      cond_br accepts i1 or legacy "any" conditions. Runtime backlog item
      found: `_ejs_op_div` aborts EJS_NOT_IMPLEMENTED on non-number LHS
      (ejs-ops.c ~901) — sub/mul coerce, div doesn't.
- [ ] **P3** oracle-guided guarded arithmetic in `LowerFunction.binary`,
      `--types`-gated.
      *Gate:* matrix green + stage2≡stage3 functional gate (flag off);
      full-suite `--types`
      diff lane byte-identical; EIR-shape unit tests; microbenchmark delta
      recorded.
- [ ] **P3.5** differential harness in maam repo (`concreteEval` vs node vs
      ejs on closed-world tests) wired into its CI.
      *Gate:* zero divergences on the curated corpus.
- [ ] **P4** (design doc only) shape-guarded property access: guard op,
      runtime layout, promotion criteria from Phase 3 experience.
