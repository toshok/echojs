# language-P1 results — gap inventory + test262 subset probe

Status: **complete** (2026-07-31).  Closes plans.md P8.1.

The 34-probe census (`test/modernization/`, 2026-07-10) sampled the
gap surface by hand; this phase adds the exhaustiveness check it
couldn't provide: a 26,820-test probe of test262 against the stage1
compiler, and the prioritized feature list for language-P3 derived
from it.

## The probe

Tooling: `test/test262/run-test262.mjs` (+ README there).  Suite:
tc39/test262 @ `5ef1e572`.  Selection: all of `test/language/`
(23,724), a 3-per-leaf-directory stratified sample of
`test/built-ins/` (2,980), all of `test/harness/` (116).  Compiler:
stage1 (`//:ejs.exe.stage1`) in the srcdir layout, default `-O2`,
macos-arm64.  ~90 minutes at 10 jobs.

Probe simplifications (full list in the runner README): unflagged
tests run sloppy-only; `flags: [module]` tests are skipped (824 — the
compiler's static module gathering doesn't resolve test262's
`*_FIXTURE.js` specifiers); negative tests pass on any nonzero exit at
the expected phase, error types unmatched; no `$262` host object.

## Headline

| outcome | count | share |
|---|---|---|
| pass | 9,348 | 34.9% |
| fail-parse | 10,451 | 39.0% |
| fail-runtime | 4,552 | 17.0% |
| fail-compile (compiler error after parse) | 634 | 2.4% |
| fail-crash (binary died on a signal) | 589 | 2.2% |
| fail-negative (illegal program accepted) | 361 | 1.3% |
| timeouts (compile 14 / run 42) | 56 | 0.2% |
| skipped (module / agent) | 826 | 3.1% |
| fail-async | 3 | — |

Of the 9,348 passes, 3,915 are negative tests (illegal programs
correctly rejected) and 5,433 are positive tests that compiled, ran,
and exited clean.

**The parser is the long pole, confirmed and quantified**: 44% of the
language area fails at parse.  The census's 13 parser gaps all
reproduce; the probe shows their blast radius — class bodies with
fields/private members poison the entire ~8.4k-test class area (26%
pass), the async-iteration family (`for await`, async generators) is
~4.9k tests at 7–14%, and dynamic `import()` (~1k tests, 39%) plus
`using`/`await using` (explicit-resource-management, ~250 tests) are
post-census syntax the hand census never probed.

## What the census missed

New findings, beyond confirming `test/modernization/`:

- **Builtin property attributes are wrong everywhere.**  `Math.PI` is
  writable, enumerable, and configurable; so, presumably, is most of
  the builtin surface.  test262's `propertyHelper.js`
  (`verifyProperty`) fails on nearly every use — 2,382 failing tests
  include it, the single largest runtime-failure root cause.  Fixing
  attribute installation in the runtime de-skews the entire built-ins
  column at once.
- **589 signal crashes where a TypeError belongs.**  Top signatures:
  `ToEJSBool` (293), `Construct` (46), `_ejs_String_impl` (27),
  `PropertyKeyHash` (11).  These are runtime asserts reachable from
  JS — e.g. `arguments[Symbol.iterator]`, computed accessor keys
  evaluated at runtime, `Reflect.defineProperty`, `toFixed`.  A
  conformance lane needs these to become thrown TypeErrors; each
  abort() reachable from JS is also a robustness bug in its own
  right.
- **`super` in object literals** (`{ m() { super.x } }`, computed-key
  accessor variants) is an EIR lowering error ("Stack is empty") —
  the compiler accepts the parse and dies later.  634 fail-compile
  total; the other recurring signature is function-declaration in
  case/statement positions (Annex-B adjacent, already pinned as
  fundecl1).
- **Early-error enforcement gaps** (352 illegal programs accepted):
  regexp literal validation is the mass (114 — bad patterns/flags
  parse fine and misbehave at runtime), then block-scope
  redeclaration (60), object-literal and class-body early errors,
  `if`-statement label/declaration restrictions.  The
  `assignmenttargettype` early errors, by contrast, are nearly
  perfect (312/318).
- **eval and `with` are structurally absent** (direct eval 2%,
  indirect 5%, `with` 9% — the passes are mostly negatives).  For an
  AOT compiler this is a permanent posture, not a to-do: the
  language-P4 lane should carve these out explicitly rather than
  count them as failures.
- **The 2015-era built-ins have decayed relative to the spec**:
  TypedArray 0/218, DataView 0/79, Date 5%, Reflect 9%, Iterator
  helpers 11%, ArrayBuffer 11%.  Entirely absent: BigInt runtime,
  globalThis, WeakRef/FinalizationRegistry, AggregateError, Atomics/
  SharedArrayBuffer, Temporal, ShadowRealm, Uint8Array base64.

## Prioritized feature list (language-P3 input)

By failing-test count, corrected for co-occurrence (test262 tags every
feature a test *uses*, so e.g. `destructuring-binding` at 4,457
failures is mostly class/async tests re-counted; destructuring itself
largely works — the census's f25/f33 pass).

Syntax, in payoff order:

1. **Class bodies**: public/private fields, private methods, static
   variants (~4.7k failing tests across the tags; the class area is
   8.4k tests at 26%).  Parser + emitter + runtime work.
2. **Async iteration family**: async generators, `for await`,
   `Symbol.asyncIterator` (~4.9k tests at 7–14%).  Needs async/await
   (census f05) as its substrate — async functions alone are ~650
   tests.
3. **Object spread/rest** (`object-rest` 351/355 failing, `object-spread`
   111/135) — desugar candidates, small and self-contained.
4. **Dynamic `import()`** (~1k tests, 39% pass) — interacts with the
   AOT module story; needs a design decision, possibly a
   permanently-carved-out subset like eval.
5. **Optional chaining / nullish / logical assignment / `**`**
   (census confirmed; each a modest desugar: 27, 20, 66, 36 failing
   parse tests respectively in their areas).
6. **Newer literals**: BigInt (needs value-representation decision —
   heap-boxed per language-plan), numeric separators, hashbang
   comments, regexp-modifiers/named-groups/u-v-flags (the regexp
   literal area is 36% with 114 accepted-illegal tests — points at
   PCRE-side validation too).
7. **`using`/`await using`** (explicit-resource-management, ~250
   tests) — post-census, stage-3+, low urgency.

Runtime/stdlib, in payoff order — all of it independent of the parser
and startable before language-P2 lands:

1. **Builtin property attributes** (writable/enumerable/configurable
   per spec) — unlocks propertyHelper, de-skews ~2.4k tests.
2. **Crash-to-TypeError conversion** for the 589 signal crashes
   (ToEJSBool/Construct/String_impl/PropertyKeyHash first).
3. **`super` in object literals** — fix or reject cleanly (currently
   a compiler error after successful parse).
4. **TypedArray/DataView/ArrayBuffer rewrite to current spec** (0%
   areas; also the detachArrayBuffer/resizable dependencies).
5. **Missing globals**: globalThis (census f32), Object.entries/
   values/fromEntries (f14), String/Array method gaps (f12/f13),
   Reflect completion, AggregateError, Iterator helpers.
6. Date conformance (5%) — old, large, low-leverage; late.

## Re-running

```sh
git clone --depth 1 https://github.com/tc39/test262.git /tmp/test262
# assemble a workroot (srcdir-tree + lib/generated + stage exe as ./ejs;
# see test/test262/README.md), then:
node test/test262/run-test262.mjs run --suite /tmp/test262 --ejs <workroot> \
    --jobs 10 --cap-builtins 3 --out results.jsonl
node test/test262/run-test262.mjs report --in results.jsonl --md report.md
```

The probe is deterministic modulo timeouts; re-runs after feature work
give the language-P4 lane its baseline.  The raw results
(`results.jsonl`, 26,820 rows) are not checked in — regenerate as
needed.
