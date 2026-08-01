# maam-P5 (plans P11.1) results — the self-hosted type oracle

`--types` in the self-hosted compiler: the MAAM abstract interpreter is
compiled into the bootstrap, and the node-hosted and self-hosted oracles
produce byte-identical typed output.  The README's "source-checkout-only"
caveat is gone.

## The seam

- **One static import, two builds.**  `lib/eir/oracle.ts` imports maam
  through the `$maam` import variable (ambient surface in
  `lib/maam.d.ts`; the structural narrowing stays in oracle.ts).  The
  self-hosted compiler resolves it via `-I maam=<workroot path>`
  (buck-stage.sh) to maam's ESM build, which gather-imports follows
  statically and compiles in (~22 modules).  The node-hosted stage0
  resolves it via a buck-gen-js.sh sed to maam's CommonJS build, staged
  into the generated tree.  The `__dirname`-walk lazy `require()` is
  gone; the analysis itself is still only invoked under `--types`
  (flag-off compiles pay module-load cost only).
- **Both maam builds come from the bootstrap.**
  `//external-deps:maam-esm` / `:maam-cjs` run tsc (emit-only,
  `--noCheck`; the maam repo owns typechecking) over the submodule's
  `src/`.  The ESM flavor lands in the srcdir tree
  (`external-deps/echojs-maam/dist/src`), the CJS flavor in
  `//lib:generated`.  Staging `package.json` matters: without its
  `"type": "module"`, tsc's NodeNext mode silently emits CommonJS.
- maam's repo builds/typechecks under TypeScript 7 now (tsconfig.cjs
  moved off the removed `Node10` resolution to `bundler`; `types:
  ["node"]`), matching the echojs tree's toolchain ahead of the planned
  repo merge.

## Prereqs delivered (the P11.1 checklist)

- **`export * from` + `export * as ns from`** — gather-imports records
  star nodes and expands them post-gather to concrete exports via a
  fixpoint (chains work; explicit local exports shadow star names; a
  name reaching a module through two stars is exported only when both
  resolve to the same original export, else dropped with a warning).
  Lowering copies the source module's slots (same snapshot semantics as
  `export { a } from`), and `export * as ns` stores the source module's
  namespace object into the slot.  Tests: exportall1 (diamond,
  shadowing, star-of-star), exportall2 (ns re-export + runtime member
  reads).
- **`.js`-suffixed import specifiers** — module paths are suffix-free
  everywhere; addSource now strips the suffix so `./foo.js` keys the
  same module as `./foo` (the test262 `skip-module`/`_FIXTURE.js` root
  cause).  Test: jsimport1.
- **14 stdlib methods** — `Object.{entries, fromEntries, hasOwn}`,
  `Array.prototype.{includes, at, flat, flatMap, findLast}`,
  `String.prototype.{padStart, padEnd, trimStart, trimEnd, replaceAll,
  at}`, plus `Object.values` (the oracle's own dump path needs it).
  Spec-shaped C implementations with correct `.length`s; 13 new test
  files.  Two pre-existing `String.prototype.replace` bugs fell out:
  the tail was dropped when a match ended at index len−1, and `` $` ``
  substitution was missing.
- **NUL bytes in source** — `_ejs_string_new_utf8_len` treats U+0000 as
  an ordinary code unit and advances by bytes consumed (it used to
  break at the first NUL and mis-count multibyte input).  maam's
  `state.ts` env-interner separators are raw NULs, the original
  trigger.  Test: nulsource1 (a raw NUL byte inside a string literal).
- **maam ⊤-operand soundness** — in the maam repo: the abstract binop's
  ⊤-operand refinement now applies the one-proven-number-operand rule
  (`- * / % ** & | ^ << >>` claim `anyNum` only when one operand is a
  proven bigint-free primitive; two ⊤ operands stay ⊤; `>>>` is
  number-on-completion unconditionally; `+` needs a proven operand for
  the num|str join and sharpens to `anyStr` under a proven-string
  operand).  Unary `-`/`~` on ⊤ stay ⊤; unary `+` (ToNumber, throws on
  bigints) keeps `anyNum`.  BigInt literals normalize into the machine:
  the concrete domain evaluates them exactly (real bigint arithmetic on
  matching operands; the rejected mixes degrade to ⊤), the abstract
  domain widens them to ⊤ (it has no bigint constituent).
  Async/generator functions are no longer silently modeled as sync
  (a real unsoundness — `f()` claimed the body's return value where JS
  returns a Promise): their bindings hold ⊤ with a visible
  degradedBindings record.  Corpus: bigint-arith, bigint-bitops-compare,
  bigint-flow (containment-lane canaries: the old code's `anyNum` claim
  fails them), async-degrades (visible SKIP).  Differential harness:
  GATE PASS, 0 violations across 2385 containment checks.

## What the stage0-vs-stage1 gate flushed (all fixed)

Running the identical analysis under both hosts is a brutal
differential test; four deep pre-existing echojs bugs surfaced:

1. **Literal fusion through LLVM names** — the emitter's
   `generateEJSValueForString` re-resolved the just-created global by
   NAME (`getOrInsertGlobal`); LLVM names are NUL-terminated C strings,
   so literals differing only past an embedded U+0000 truncated to the
   same name and FUSED into one constant ("a\0b" === "a\0c" ran as
   true; maam's NUL-separated interner keys collapsed, states merged:
   22 reached states vs node's 31).  Both hosts miscompiled this.
2. **NUL-truncating string comparisons** — `SameValue`, `SameValueZero`
   (Map/Set membership!), strict/loose equality and the relational
   operators compared flat strings with the C-string `ucs2_strcmp`,
   stopping at the first NUL.  New length-aware `ucs2_strcmp_len` at
   all seven ejs-ops.c sites.  Test: nulstring1.
3. **Stale namespace-object tag** — `emitEjsvalFromPtr` still OR'd the
   pre-BigInt SHIFTED_TAG_OBJECT (0xFFFC…) after language-P4.2
   renumbered OBJECT to 0x0A (0xFFFD…), so module namespace objects
   were mistagged ("rhs of 'in' must be an object"; property reads
   returned undefined).  Invisible before because ns member reads were
   always compile-time-resolved; `export * as ns` made the runtime path
   live.

4. **`Array.prototype.every` skipped ToBoolean** — it returned false
   only for the literal `false`, so falsy non-boolean callback results
   (null/0/"") passed the predicate.  Surfaced as the last lane
   divergence (optchain1): the specialize pass's
   `returns.every((r) => r.argument && …)` judged a bare `return;`
   differently per host, so only the self-hosted compiler minted (and
   rejected) a wrapper clone for the `?.` desugar arrow — an honest
   one-instruction miscompile-of-the-compiler chain.  Test:
   array-every1.

Plus one compiler-source gap: the oracle's `--types-dump` uses
`Object.values`, which didn't exist in the runtime (the probe's list
missed it because the dump only runs under a flag).

## Gates

- **stage0-vs-stage1 --types lane** (buck-test-types-diff.sh grew a
  host-vs-host mode: same file compiled `--types --types-dump` by both
  hosts; normalized analysis stderr AND run stdout must match): 520
  files, 516 IDENTICAL, 0 divergent, 239 diamonds / 2207 oracle
  queries total.  The 4 remainders (esprima1, esprima-roundtrip1/2,
  typedarray2) exceed the lane's 120s per-file compile cap under the
  self-hosted compiler — the maam analysis of multi-thousand-line
  modules is CPU-bound and stage1 runs it ~2× slower than V8;
  typedarray2 completes in 126s with byte-identical typed output, the
  esprima trio likewise with a longer leash.
- **Bootstrap matrix** — test-eir, test-eir-lowtier, stage0–3,
  stage1-shapes-off: all green.
- **maam repo** — typecheck + 274 unit tests green; differential
  harness GATE PASS with the ejs lane on (51/51 OK, 0 divergences,
  0 stale known-divergence entries — the seven 2026-07-23 entries all
  turned out fixed by runtime-P1 and were removed), 0 violations
  across 2385 containment and 371 shape-witness checks.
- **README caveat deleted.**

## Follow-ons

- The maam repo merge (separate only for the paper) — the `$maam`
  seam and the two tsc genrules collapse further once it's a
  first-class lib/ citizen.
- `ucs2_to_utf8` (the OTHER direction) still truncates at NUL:
  `console.log` of a NUL-embedded string prints the prefix only.
  Harmless for the oracle (keys never print); a pin candidate.
- The t262 module lane (824 skipped `skip-module`/`_FIXTURE.js` tests)
  should now largely unskip — rerun the language-P1 runner when next in
  that area.
