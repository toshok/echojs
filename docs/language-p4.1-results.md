# language-P4.1 results: the async/class-fields conformance pass

2026-07-31.  The first ratchet turn on the language-P4 lane: a test262
pass focused on the async family and class fields, the two areas the
goal named.  BigInt stays gated (value-representation decision still
pending).  Everything verified against node on targeted repros; suite
matrix green; lane expectations regenerated (the file only shrinks).

## Headline numbers (stage1, macos-arm64, suite b363f29d)

| area | before (P8.3) | after |
|---|---|---|
| statements/class/elements | 1095/1534 (71%) | 1419/1584 (90%) |
| expressions/class/elements | — | 1350/1477 (91%) |
| statements/for-await-of | 387/1234 (31%) | 1115/1234 (90%) |
| expressions/async-function | 71/93 (76%) | 77/93 (83%) |
| statements/async-function | 60/74 (81%) | 63/74 (85%) |
| expressions/async-arrow-function | 49/60 (82%) | 52/60 (87%) |
| expressions/async-generator | 0 (gated) | 393/627 (63%) |

(Numbers from the pre-final measurement — the last fix batch
(globalThis, hasOwnProperty ToObject, elision-in-assignment,
default-value NamedEvaluation) landed after it.)

The full lane moved 2,584 → 3,196 of 5,977 (43% → 53%): the
expectations file shrank by 612 entries with **zero** newly-failing
tests (the 231 changed lines are status drift on still-failing tests),
and fail-compile collapsed 1,056 → 345 (the async-generator gate).

## Features

- **Async generators** (`async function*`) — the one gated family left
  from P8.3's async tier.  Same coroutine substrate: the body becomes a
  sync generator speaking a marker protocol — `await X` yields
  `{mark, "await", X}`, `yield Y` yields `{mark, "yield", Y}` — consumed
  by a per-wrapper `__ejs_asyncGenDrive` that queues next()/throw()/
  return() requests (AsyncGeneratorEnqueue) and resumes the generator
  synchronously inside the request call, settling each with a promised
  iterator result.  Yielded values are awaited before delivery; their
  rejection throws at the yield.  `yield*` delegates through a sync
  relay generator whose marked yields pass through the outer `yield*`
  untouched, so delegated awaits/yields reach the driver directly —
  async and sync (async-from-sync, each value awaited) sources both
  work.  Class/object-literal async generator methods ride the same
  desugar ordering as async methods.  Parser gate deleted.
  Not spec-complete: return()-through-delegation completes with the
  sent value (spec forwards to the inner iterator's return()), and
  gen.throw() during delegation is not forwarded to inner throw().

## Pre-existing bugs found and fixed

1. **`yield*` was broken in value position** (as old as generators):
   the desugar replaced the yield*-expression with a for-of *statement*
   — `const r = yield* inner()` failed EIR lowering, and even the
   statement form dropped the delegated return value and never forwarded
   sent values into the inner iterator's next().  Now: `yield* x` →
   `__ejs_genDelegate_N(%gen, x)`, a plain call valid in any expression
   position — the generator runs on its own coroutine stack, so the
   helper yields fine from a nested frame.  Forwards sent values,
   produces the return value, and closes the inner iterator
   (IteratorClose) on abrupt completion.  Pinned by test/generator26.js;
   async generators pinned by test/async-generator1.js.
2. **for-in over objects with private fields aborted**
   (`_ejs_primstring_flatten`: type-0 header): the weak-collection
   inverted-rep slot is a *symbol*-keyed property, and for-in key
   collection flattened every key as a string.  for-in now skips
   non-string keys (spec: EnumerateObjectProperties yields string keys
   only), and the inverted-rep slot is defined non-enumerable — it was
   also leaking into `{...obj}` spread and Object.assign.
3. **Function `.length` did not exist** — on any function, anywhere
   (`function f(a,b){}; f.length` → undefined).  The parser records the
   spec length (params before the first default/rest) before desugars
   rewrite param lists; `make_closure` carries it as an imm; compiled
   closures go through the new `_ejs_function_new_closure`, which
   defines `length` with spec attributes.  The async-function wrapper
   keeps its original length via placeholder formals (arguments still
   forward).  Builtin lengths are a separate sweep (still missing —
   the single biggest remaining built-ins lever, with Math.PI-class
   attribute wrongness from P8.1).
4. **Function `.name` gaps**: NamedEvaluation was entirely absent
   (anonymous functions/arrows named by their variable declarator,
   assignment, property, or destructuring/parameter *default*), class
   prototype methods reported the internal qualified name (`"A:m"`),
   and private methods reported the compiler temp.  The qualified id
   stays (it's the LLVM symbol); a parse/desugar-recorded
   `ejs_display_name` now feeds `make_closure`'s name (`"m"`, `"#m"`,
   `"get #m"`).
5. **Destructuring position gaps**: nested patterns under rest
   (`[...[a, b]] = xs`, object-rest equivalents), member-expression
   targets (`[a.b] = arr`), rest-pattern formals (`f(...[a, b])`), and
   elision entries in assignment position (which emitted assignments to
   undeclared temps — `%destruct_tmpN unresolved`).
6. **`globalThis` did not exist**, and
   `Object.prototype.hasOwnProperty.call(undefined, ...)` hit a runtime
   assertion abort instead of ToObject's TypeError.  asyncHelpers.js
   (the async-test harness include) needs both, so this unblocked the
   whole asyncTest-based slice.
7. **Uncaught exceptions printed no message** (`unhandled exception:`,
   blank): the terminate handler has no access to the cxa payload, so
   the throw path now stashes the last-thrown ejsval and the handler
   prints ToString of it (reentrancy-guarded).  This turned every
   opaque `fail-runtime` row in test262 triage into a legible error.

## What the remaining failures are

- **Carve-outs**: direct/indirect `eval` (permanent for AOT), dynamic
  `import()` / `import.meta` (module-story design), `with`.  These
  bound the areas below 100% regardless of other work.
- **Missing early errors** under the sloppy-only runner: "Expected a
  SyntaxError/TypeError/ReferenceError but no exception was thrown" —
  mostly strict-mode semantics, TDZ, and grammar validation (P8.1's
  352-gap census).
- **Async rejection semantics**: ~66 "expected to reject, resolved" —
  parameter-scope errors that should reject the returned promise.
- async-generator area residue: protocol-attribute tests
  (%AsyncGenerator% prototype chain identities), yield*
  throw/return-forwarding semantics.

## Gates

- tsc typecheck clean; full matrix green: test-eir, test-eir-lowtier,
  stage0–stage3 incl. the stage2/stage3 fixed point and the
  shapes-off lane (the self-compile now compiles the new desugars with
  themselves).
- test/generator26.js + test/async-generator1.js added to the suite
  (node-parity baselines); generator15/16 un-xfailed (yield* iterator
  closing works now — the stale xfails were caught by the matrix, the
  suite's own ratchet).
- test/test262/expectations.txt regenerated — the P8.4 ratchet's first
  shrink (3,393 → 2,781 expected failures).
