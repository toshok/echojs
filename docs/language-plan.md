# language-plan: JS modernization and conformance

Bucket plan; the ordering spine lives in `docs/plans.md` (milestone
references look like `language-P1`).  Content moved here from the old
plans.md "JS Modernization" section.

JavaScript hasn't stood still while this project was on hiatus: there
are new language features to catch up on (optional chaining, nullish
coalescing, class fields, async/await, BigInt, ...), and the kangax
conformance suite this repo tests against has been superseded — tc39
maintains test262, which is far larger.

Sequenced after the TypeScript port (compiler-P2) — new-feature work is
safer with types underneath it.

## Phases

- [x] **language-P1 — Gap inventory.**  DONE 2026-07-31 —
      docs/language-p1-results.md (26,820-test test262 probe via
      test/test262/run-test262.mjs: 35% pass; the parser is the
      quantified long pole at 44% of the language area failing to
      parse; new beyond the census: builtin property attributes wrong
      everywhere, 589 JS-reachable runtime aborts, `super`-in-object-
      literal lowering error, 352 early-error gaps mostly regexp
      validation; prioritized feature list for language-P3 recorded
      there).  The original census: an initial 34-probe census
      lives in `test/modernization/` (see its README).  Headline: 13
      parser gaps (optional chaining, `??`, class fields, async/await,
      `**`, object spread/rest, BigInt, ...), 4 stdlib gaps
      (padStart/flat/Object.entries/globalThis), 4 behavioral bugs
      (`__proto__:` literal, `/gi` replace, `generator.return()`, and a
      hazard: `async m()` object methods parse but silently
      miscompile).  Remaining work: a test262 subset probe for
      exhaustiveness, and a prioritized feature list from it.
- [x] **language-P2 — Parser replacement.**  DONE 2026-07-31 —
      docs/language-p2-results.md.  The slot is interface-shaped as
      planned (`lib/parser.ts`; the compiler consumes the ESTree
      dialect), but the probe inverted the pencil-in: **acorn 8.18.0 is
      the default**, not @babel/parser — acorn self-hosts byte-
      identically today (537-file corpus proof), while babel's bundle
      needs stdlib echojs lacks (`Array.prototype.at`, ...) plus 4× the
      compile time, for stage-proposal coverage nothing on the P8.3
      list needs.  The seam keeps the babel swap cheap if that changes.
      Vendored as a mechanically-regenerable ES5 transpiled build
      (external-deps/acorn/regen.sh) exactly as planned; `--parser
      esprima` is the bisection fallback.  Syntax acorn parses but the
      backend can't lower gates at the seam with a located error
      (removed feature-by-feature in language-P3).
- [x] **language-P3 — Feature implementation, payoff-ordered.**  DONE
      2026-07-31 — docs/language-p3-results.md.  The payoff list landed:
      `**`/`**=` (real generic binop + spec-correct exponentiate shared
      with Math.pow), `??` (native EIR lowering), logical assignment +
      optional chaining (DesugarModernOps), object spread/rest
      (CopyDataProperties runtime helpers), bare `catch`, class fields +
      private members (#fields/#methods/accessors via per-class weakmaps
      + brand checks) + static blocks (DesugarClasses), and async/await
      + `for await` on the coroutine generators + promises
      (DesugarAsyncFunctions; Symbol.asyncIterator added).  Eight
      pre-existing bugs flushed out and fixed (super.other()
      mis-dispatch, generator-desugar mapping pop, Promise.all stub,
      array/string OwnPropertyKeys/descriptors, array freeze clobber,
      ToEJSBool symbol/NaN, String(symbol), class member attributes).
      Still gated with located errors: async generator functions,
      BigInt (value-representation decision pending, likely heap-boxed),
      dynamic `import()`/`import.meta` (AOT module-story design).
- [x] **language-P4 — test262 lane.**  DONE 2026-07-31 —
      docs/language-p4-results.md.  `test/test262/lane.sh` runs the
      curated selection (every 6th language test, 2 per built-ins leaf
      dir, all of harness) against the suite SHA pinned in
      `test/test262/suite.sha`, checked against
      `test/test262/expectations.txt` — CI fails on regressions
      (expected-pass failing) and stale expectations (expected-fail
      passing), so the file shrinks as a conformance ratchet.  Runs in
      the macOS bootstrap job (expectations are generated on
      macos-arm64).  Grow by shrinking the language stride toward 1 as
      features land; the kangax harness stays until parity.
- [ ] **language-P5 — Un-fork the JS external-deps.**
      esprima/escodegen/estraverse/esutils live in `external-deps/` as
      lightly-patched copies (build-system compatibility).  Move to
      published npm packages where possible — published esprima is
      unmaintained and still lacks the parser-gap features above, which
      is what language-P2 solves; escodegen/estraverse/esutils can come
      from npm as-is if the local patches prove to be build-glue only
      (diff them first).
