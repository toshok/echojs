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
| f34 | `async` object methods (was a silent miscompile; now a loud parse error) |

## Runtime/stdlib gaps — 4

| probe | missing |
|---|---|
| f12 | `String.prototype.padStart` / `replaceAll` / `at` |
| f13 | `Array.prototype.flat` / `includes` / `at` / `findLast` |
| f14 | `Object.entries` / `values` / `fromEntries` |
| f32 | `globalThis` |

## Behavioral divergences — ALL FIXED (2026-07-10)

| probe | divergence | fix |
|---|---|---|
| f20 | `__proto__:` literal didn't set the prototype | lowered as SetPrototypeOf (`_ejs_object_literal_set_proto`); suite test object19.js |
| f24 | regex `i`/`m` flags parsed but never passed to PCRE | `PCRE_CASELESS`/`PCRE_MULTILINE` wired through (and the compiler no longer drops `y`/`u`); suite test regexp-flags1.js |
| f26 | `generator.return()` unimplemented (and `return x` in a generator body lost its value; next/throw on a completed generator resumed a dead context) | return-sentinel unwind through the body (finally runs), completed-state tracking; suite test generator22.js |
| f34 | `async m() {}` parsed and silently miscompiled | root cause was `tolerant: true` parsing — partial ASTs from ANY syntax error were silently compiled; tolerant mode removed, parse errors are loud now (this moves f34 to the parser-gap column) |

## Already working (17)

Symbol.iterator generators, shorthand props, computed methods, tagged
template `.raw`, labeled blocks, `__proto__`-adjacent getters,
`new.target`, regex `s` flag parse, destructured/defaulted params,
Map/Set, Promise (then-chains), Proxy (get), class getters/static
getters, array-destructuring swap, computed accessor keys
(`{ get [k]() {} }`, as of the same day this census was taken).
