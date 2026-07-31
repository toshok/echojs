# language-P2 results — parser replacement (acorn behind the ESTree seam)

Status: **complete** (2026-07-31).  Closes plans.md P8.2.

The parser was P8.1's quantified long pole: 44% of the test262 language
area failed at parse against the 2015-era esprima fork.  This phase
swaps the fork for **acorn 8.18.0** behind a one-module seam, after a
buy-vs-build probe of the two candidate parsers.

## The seam

`lib/parser.ts` is the single parse entry point (gather-imports'
`parseFile` and the EIR unit tests both go through it).  The contract is
the compiler's ESTree dialect (`lib/estree.ts`); the parser behind the
seam is swappable.  `--parser esprima` keeps the old fork reachable for
bisection; acorn is the default.

## The probe: why acorn, not @babel/parser

language-plan.md had penciled in @babel/parser (+ its estree plugin) as
the default, acorn as the cheap swap.  The probe inverted that.  Both
candidates were bundled flat-ESM, transpiled to ES5-level syntax with
@babel/preset-env (pinned; `external-deps/acorn/regen.sh`), and run two
gauntlets:

- **Parse-correctness (node)**: both parse the full bootstrap corpus —
  the srcdir tree, all top-level `test/*.js`, and their own bundles
  (537 files) — cleanly, and both parse every modern-syntax construct
  the P8.1 census flagged.
- **Self-host (the decisive one)**: compiled by stage1 and run under
  the echojs runtime, **acorn parses the entire 537-file corpus
  byte-identically to acorn-under-node** (AST JSON hash comparison,
  ~25s).  @babel/parser compiles (29s vs acorn's 7s, 481KB vs 233KB
  bundle) but dies at runtime on its first statement: it uses stdlib
  echojs doesn't have yet (`Array.prototype.at`, and whatever lurks
  behind it).

Acorn works today with zero runtime additions; babel needs an unknown
amount of stdlib work for no feature we currently need (its edge is
stage-proposal plugins; the P8.3 payoff list is all stage-4 syntax
acorn ships).  The seam keeps a future babel swap cheap if decorators
or pattern matching ever matter.

## Runtime bugs the probe flushed out (all pre-existing, all fixed)

Running a real parser under the self-hosted runtime was an effective
fuzzer.  Five bugs, each found as a concrete acorn misbehavior:

1. **RegExp compile failures were silently swallowed** —
   `pcre16_compile` errors left a NULL `compiled_pattern` that *matched
   anything*.  Acorn's `lineBreak` regex (`\u2028` in the pattern, which
   plain pcre rejects) matched every string, so ASI fired after every
   `return`.  Now: `PCRE_JAVASCRIPT_COMPAT` (for `\uXXXX` and friends)
   and a loud SyntaxError on compile failure — which also closes part of
   P8.1's "114 accepted-illegal regexps" finding.
2. **Non-`/u` regexes compiled in UTF-16 mode** — pcre rejects lone
   surrogates in UTF mode, but JS non-`u` patterns legitimately contain
   them (every parser's astral identifier tables, the esprima fork's
   included — silently match-anything today).  UTF-16 interpretation is
   now gated on the `u` flag; without it, matching is per code unit,
   per spec.
3. **`String.prototype.indexOf`/`lastIndexOf` ignored `fromIndex`** —
   acorn's block-comment scanner (`input.indexOf("*/", pos)`) got an
   earlier match, moved its cursor backward, and looped forever on any
   file with two block comments.
4. **The UTF-8 decoder had no 4-byte branch** — `0xF0` lead bytes fell
   into the 3-byte case (`0xF0 & 0xE0 == 0xE0`), decoded garbage, and
   the orphaned continuation byte truncated the rest of the string:
   an astral char in a source file *ate everything after it*.  Now
   decodes code points and emits surrogate pairs.
5. **`JSON.stringify`'s hex table was `"012356789abcdef"`** — missing
   the 4 since 2013; every escaped code unit with a hex digit ≥ 4 was
   off by one (`\u000b` printed as `\u000c`).  Plus
   `String.fromCharCode(0)` built a zero-length string (NUL-terminated
   constructor).

## The adapter, and unsupported-syntax gates

Acorn emits standard ESTree; the seam adapts to the dialect (verified
against the fork construct-by-construct, key-order-normalized):

- top-level parameter `AssignmentPattern`s hoist into the aligned
  `defaults` array (nested pattern defaults stay in place — the fork
  did the same);
- `TryStatement` grows `handlers[]`/`guardedHandlers`;
- `MetaProperty.meta/.property` flatten to raw names;
- extra standard fields (`async: false`, `optional: false`,
  `attributes: []`, `directive`, `start`/`end`) ride along harmlessly —
  the visitor switches on `type`.

Syntax acorn parses but the backend can't lower yet **fails at the seam
with a located error** instead of miscompiling silently (the census's
`async m() {}` hazard class): async/await, `for await`, class fields /
private members / static blocks, object spread, optional chaining,
`??` / `**` / logical assignment, BigInt literals, dynamic `import()`,
`import.meta`, catch-without-binding.  In test262 terms this converts
fail-parse into clean fail-compile; language-P3 deletes gates as it
lands features.

```
$ ejs -q -o t t.js     # t.js: async function f() {}
t.js: Error: 1:1: async function syntax is not supported yet
```

## What this buys P8.3

Every syntax feature on the P8.3 payoff list now *parses* (ecmaVersion
"latest", hashbang included); the work remaining per feature is
desugar/emitter/runtime, feature by feature, deleting a gate each time.
Early errors also upgrade wholesale: acorn enforces the spec's parse-
time errors (duplicate declarations, invalid assignment targets,
module-mode export checks) that the fork missed — 352 accepted-illegal
programs in the P8.1 probe.

## Gates

- tsc clean (compiler + test tree)
- full matrix at stage0/1/2/3 + shapes-off: **426/20/0** per lane — one
  better than before: `generator5.js` (kangax generator value-sending,
  `sent = [yield 5, yield 6]`) xpassed under acorn and is un-xfailed
- test-eir 227 + lowtier, both green
- corpus AST-identity: acorn-under-stage1 ≡ acorn-under-node, 537 files
- test262 spot checks: `expressions/optional-chaining` 13 pass +
  25 clean gated fail-compile, **zero fail-parse**; `test/harness` and
  `statements/try` healthy, residual fail-parses are the runner's
  module-strictness artifacts (`with`, strict `delete`)
- `--parser esprima` verified working

## Follow-ons recorded

- The esprima/escodegen/estraverse/esutils submodules stay (escodegen
  still consumes them, and esprima remains a test payload + bisection
  fallback) — un-forking is P8.5 as planned.
- `Array.prototype.at`/`includes`, `String.prototype.at`/`padStart`
  confirmed missing (babel's blocker; P8.1 runtime list already covers
  them).
- The vendored bundle's ES5 transpile step shrinks as language-P3
  features land (`external-deps/acorn/regen.sh` pins the recipe).
