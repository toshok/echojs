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
- [ ] **language-P3 — Feature implementation, payoff-ordered.**  Wire
      probes into CI as they green.  Syntax-only features (optional
      chaining, `??`, `**`, spread/rest in objects) are desugar
      candidates; async/await and class fields need runtime + emitter
      work; BigInt needs a value-representation decision (NaN-boxing
      has no spare tag appetite — likely heap-boxed).
- [ ] **language-P4 — test262 lane.**  Stand up a curated test262
      subset as a CI lane (the kangax harness stays until parity);
      grow toward the full suite as features land.
- [ ] **language-P5 — Un-fork the JS external-deps.**
      esprima/escodegen/estraverse/esutils live in `external-deps/` as
      lightly-patched copies (build-system compatibility).  Move to
      published npm packages where possible — published esprima is
      unmaintained and still lacks the parser-gap features above, which
      is what language-P2 solves; escodegen/estraverse/esutils can come
      from npm as-is if the local patches prove to be build-glue only
      (diff them first).
