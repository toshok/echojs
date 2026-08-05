# Regexp: property escapes, the v flag, and the road to 75%

Scoping notes for the largest lever in the test262 75% release gate
(`test/test262/full-baseline.json` is at 73.25%; +791 passes needed).
Regexp accounts for ~650 of the ~1,090 measured reachable failures.

## What the engine is today

- The matcher is a vendored **PCRE 8.32 (2012)**, 16-bit API, driven
  from `runtime/ejs-regexp.c`.  The JS pattern text is handed to
  `pcre16_compile` **raw** — there is no translation layer — with
  `PCRE_JAVASCRIPT_COMPAT`, plus `PCRE_UTF16|PCRE_NO_UTF16_CHECK`
  under `/u`.
- The build **does not enable UCP**, so every `\p{...}` is rejected
  outright: "support for \P, \p, and \X has not been compiled".
- The flag parser accepts `gimuy` only: `s` (dotAll), `d` (indices),
  and `v` (unicodeSets) are SyntaxErrors before a pattern is ever
  seen.
- Regexp literals lower to a `_ejs_regexp_new_utf8` call
  (`lib/eir/emit.ts` `make_regexp`), so literals and `new RegExp`
  share the one runtime path — a single fix covers both.  Today a
  literal whose pattern PCRE rejects evaluates to **null** instead of
  throwing a SyntaxError (`.test` then dies with a TypeError); that is
  a robustness bug to fix with the rest, whatever else happens.

## What the failing tests demand

From the full-suite run (all.jsonl, run 30939628437):

| cluster | fails | needs |
|---|---|---|
| `RegExp/property-escapes/generated` | 449 | `\p{Script=X}` 175, `\p{Script_Extensions=X}` 175, `\p{General_Category=X}` 38, binary properties 60 (of which 5 are v-only properties-of-strings: `RGI_Emoji*`, `Basic_Emoji`, `Emoji_Keycap_Sequence`) |
| `RegExp/unicodeSets` | 114 | `v` flag: set operations (`--`, `&&`), nested classes, properties-of-strings |
| `RegExp/prototype` | 229 | mixed exec-side semantics (`Symbol.replace` details, `.groups`, etc.) — mostly not engine work |
| `RegExp/named-groups` | 34 | `(?<name>)` groups surfaced as `.groups` on match results |
| `regexp-modifiers` | 26 | `(?i:...)` inline modifiers |
| `RegExp/escape` | 19 | `RegExp.escape` — a plain stdlib function, not engine work |
| match-indices / dotAll | 29 / 16 | `d` flag (ovector offsets already exist), `s` flag (`PCRE_DOTALL`) |
| lookBehind / CharacterClassEscapes | 12 / 12 | variable-length lookbehind (PCRE 8 is fixed-length only); `\d\w\s` edge semantics |

## Options considered

**Enable UCP in the vendored PCRE 8.32** — rejected.  Its Unicode
tables are 2012-era (wrong answers against a suite generated from
current Unicode: fail-parse merely becomes fail-runtime), its syntax
is `\p{Greek}` not `\p{Script=Greek}` so a rewriter is needed anyway,
and it has no Script_Extensions and almost none of the 60 binary
properties.  Caps out far below the goal.

**Swap to PCRE2** — deferred.  Newer PCRE2 does add
Script_Extensions and variable-length lookbehind, but still lacks
most ECMAScript binary properties, has no v-mode set operations or
properties-of-strings, and the API port risks the ~33k currently
passing tests for coverage that still requires a translation layer on
top.  The translation layer is the work either way; the engine swap
can happen later if its residual wins (lookbehind, modifiers) earn
it.

**Translate in front of the engine (recommended)** — keep PCRE as the
executor and add a C-side pattern translation pass in
`RegExpInitialize`, expanding property escapes into explicit
code-point-range classes from **build-time-generated Unicode tables**,
and desugaring v-mode classes into computed ranges/alternations.
Engine-neutral (survives any later PCRE2/libregexp swap), and covers
exactly what the tests need because we generate exactly the
properties they use.

The table generator runs under node as host tooling.  Data source:
the **`@unicode/unicode-17.0.0`** npm package (dev dependency),
emitting per-property code-point ranges as C source — the same
node-generated-table pattern as `runtime/ejs-builtin-arities.h`.
That package is the very data mathiasbynens' generator built the
test262 property-escapes tests from (each test's header says "Unicode
v17.0.0"), so agreement is by construction.  The obvious alternative
— using node's own `/\p{...}/u` engine as the oracle — is ruled out
by version skew: node 22 ships Unicode 15.1, two versions behind the
suite; it serves only as a sanity cross-check on properties the
versions share.  Properties needed: ~160 Script + Script_Extensions
values, 38 General Categories + aliases, ~55 binary properties.
Estimated table size low hundreds of KB of C.  Bumping `suite.sha`
across a Unicode release means bumping the data package in the same
commit — the same coupling rule the lane already has for
expectations.

## Staging

1. **`\p`/`\P` under `/u`** — table generator + translation of
   `\p{...}` inside and outside classes; fix the literal-null bug.
   ~450 tests.  **DONE**: property-escapes slice 164→565 of 613.
   The residue is the engine-fidelity tail: ~25 tests matching `\P{X}`
   against lone-surrogate subjects (pcre's UTF-16 walker refuses
   them), the surrogate-endpoint classes (`\p{Cs}`), and the v-only
   properties-of-strings.  Those wait for stage 2 / the engine
   endgame, not for more tables.
2. **`v` flag** — flag plumbing + class-set grammar (nested classes,
   `--`/`&&` desugared at translation time, properties-of-strings as
   sequence alternations).  ~115 tests.
3. **Cheap riders** — `s` → `PCRE_DOTALL` (~16), `d` → surface
   ovector pairs as `.indices` (~29), `RegExp.escape` in C (~19).
4. **Exec-side** (independent of the engine): named groups `.groups`
   (~34), then the `RegExp/prototype` residue case by case.

Stages 1–3 are ~630 tests; with the stdlib lever (~250) the 75% gate
clears without touching Temporal.
