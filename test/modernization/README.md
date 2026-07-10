# JS Modernization census

Probe files for the modernization effort (see `docs/plans.md`). These
are deliberately OUTSIDE the tester's `*<digit>.js` discovery glob —
most don't compile yet. Each file is a single modern-JS feature; run
one with the node-hosted compiler and diff against `node <file>`.

Census as of 2026-07-10 (post legacy-pipeline deletion):

## Parser gaps (the esprima fork is the long pole) — 13

| probe | feature |
|---|---|
| f01 | optional chaining `?.` |
| f02 | nullish coalescing `??` |
| f03 | class fields (instance + static) |
| f04 | private fields `#x` |
| f05 | `async`/`await` |
| f06 | exponentiation `**` |
| f07 | object spread `{...a}` |
| f08 | object rest `{x, ...rest}` |
| f09 | async generators |
| f10 | `for await` |
| f11 | BigInt literals `10n` |
| f22 | trailing comma in function params |
| f23 | optional catch binding `catch {}` |
| f31 | logical assignment `??=` `\|\|=` `&&=` |

## Runtime/stdlib gaps — 4

| probe | missing |
|---|---|
| f12 | `String.prototype.padStart` / `replaceAll` / `at` |
| f13 | `Array.prototype.flat` / `includes` / `at` / `findLast` |
| f14 | `Object.entries` / `values` / `fromEntries` |
| f32 | `globalThis` |

## Behavioral divergences (bugs) — 4

| probe | divergence |
|---|---|
| f20 | `__proto__:` in an object literal doesn't set the prototype |
| f24 | `"aAa".replace(/a/gi, "x")` → `"xAx"` (ignoreCase lost when combined with global) |
| f26 | `generator.return()` not implemented in the runtime |
| f34 | **HAZARD**: `async m() {}` object method PARSES but silently miscompiles (no parse error, wrong behavior) — should be rejected until async lands |

## Already working (17)

Symbol.iterator generators, shorthand props, computed methods, tagged
template `.raw`, labeled blocks, `__proto__`-adjacent getters,
`new.target`, regex `s` flag parse, destructured/defaulted params,
Map/Set, Promise (then-chains), Proxy (get), class getters/static
getters, array-destructuring swap, computed accessor keys
(`{ get [k]() {} }`, as of the same day this census was taken).
