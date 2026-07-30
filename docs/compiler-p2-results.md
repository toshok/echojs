# compiler-P2 results — the TypeScript port, finished (P7.4)

Phase: compiler-P2 (plans.md P7.4).  Branch `eir`, 2026-07-29.

The compiler sources were already strict TS (the EIR work's incremental
port); what remained was the tooling that still ran JS through babel,
and the residual JS entry points.  Both are gone: **babel is no longer a
dependency of anything in the repo.**

## What changed

### 1. `//lib:generated`: the babel step is now tsc

`lib/buck-gen-js.sh` used to run every file of the stage0 tree through
`@babel/cli` (preset-env, `modules: commonjs`), one process per file.
It now stages the `//lib:tsjs` ES-module tree (plus host-config and the
esprima/escodegen/estraverse/esutils externals), applies the
`"@llvm"`→`"llvm"` / `"@node-compat/"`→`""` rewrites with sed on the
way in, and runs **one** tsc invocation over the whole tree:
`--allowJs --module commonjs --esModuleInterop --target es2016` — no
type-checking (no `checkJs`), just the module conversion babel used to
do.  `--esModuleInterop` matches babel's default/namespace-import
interop against CJS modules (the node-llvm addon, glob, ...).
TS 7 note: `--moduleResolution node10` is gone; the default for
`--module commonjs` resolves the extensionless relative imports fine.

### 2. `// generator: babel-node` → `// generator: esm`

Import-syntax tests can't run under plain node (extensionless relative
specifiers); babel-node's require hook used to transpile them during
expected-output generation.  The tester now does it with tsc: transpile
the test plus its relative-import closure to CommonJS in a scratch dir
(`generateExpectedEsm` in tester.ts), copy the harness shim and driver
alongside (unconverted — the serializer runs byte-exact), and run
`node harness-run.js <transpiled test>` as before.  The directive is
renamed in all 114 test files; `esm` names the test's need, not a tool.

Closure resolution mirrors the compiler's: file first, then
`directory/index.js` (modules6).  Two tests import from outside test/
(`esprima-roundtrip{1,2}`, `../external-deps/...`) — both are
`skip-if: true` and were unrunnable under babel-node too; the esm
generator doesn't reach outside test/ (noted in esprima1.js).

**Parity:** all 112 runnable esm tests generate byte-identical output
under babel-node and under the tsc path (the other 2 are the skipped
esprima-roundtrip pair).  End-to-end through the real tester, a deleted
baseline regenerates byte-identical to the committed one.

### 3. `test/tester.js` → `test/tester.ts`

Ported under the repo's strict flag family (strict,
noUncheckedIndexedAccess, noImplicitOverride) with a small
`tester-deps.d.ts` (temp has no types; colors' chained `red.bold` isn't
in its shipped types).  `test/tsconfig.json` holds the compile settings
(CommonJS output; skipLibCheck because glob's path-scurry .d.ts trips
over @types/node 26).  buck-test-stage.sh compiles the staged copy in
place (`tsc -p "$WORK/test"`) before running it; the emitted
test/tester.js is gitignored.  CI typechecks it (`tsc -p test
--noEmit`) next to the root config.

Faithful port, plus: the dead `-s` range check (`< 0 && > 2`) now
actually validates 0..3; the unused CircleCI `running_in_ci` and the
collected-but-unused stderr buffer are gone; stage-index and
tests-array accesses are guarded (noUncheckedIndexedAccess).  The
scheduler, xfail/skip-if/generator directive handling, wrapper
generation, and per-test TMPDIR behavior are unchanged.

### 4. `runtime/gen-atoms.js` → `runtime/gen-atoms.ts`

Compiled by the new `//runtime:gen-atoms-js` genrule (tsc, same strict
family); `//runtime:atoms` and `//ejs-llvm:atoms` consume the compiled
JS.  Output verified byte-identical on both atoms headers.  Included in
the root tsconfig typecheck.

### 5. babel removed

`@babel/cli`, `@babel/node`, `@babel/preset-env` dropped from
package.json (lock refreshed; `grep -c babel package-lock.json` = 0),
`.babelrc` deleted, ci.yml's babel-node PATH export removed, stale
"babel'd tree" comments updated across BUCK files and scripts.

## What stays JS deliberately

- `test/harness-console-shim.js` — must compile under ejs and run under
  node byte-identically; conservative ES5 by contract (runtime-P3).
- `test/harness-run.js` — 8-line node driver; a copy rides into the esm
  generator's transpile dir, so it stays plain CJS.
- `lib/host-config.js.in` — 3-line generated config (has a .d.ts).
- The esprima/escodegen/estraverse/esutils forks — language-P5's
  un-forking is the owner.
- The test corpus itself, and `samples/`.

## Gates

- typecheck: `tsc -p tsconfig.json` and `tsc -p test --noEmit` clean
- test-eir: the standing 11 compiler-P1.1 pins only, no new reds
  (verified same 11 by name against the tsc-converted tree)
- stage0/1/2/3 suites: 424 pass / 21 xfail / 0 fail each
- test-stage1-shapes-off: 424 / 21 / 0
- test-eir-lowtier: OK
- esm-generation parity: 112/112 runnable byte-identical vs babel-node

## Notes / follow-ons

- The stage0 tree is now es2016-level JS (babel's targetless preset-env
  downleveled to ES5); node 22 runs both, nothing observed the change.
- compiler-P3 (TS as compiler *input*) is unchanged by this phase and
  still coordinates with language-P2 at the parser seam.
- `// generator: esm` is tool-agnostic on purpose: if node's own loader
  hooks ever replace the tsc transpile, no directive churn.
