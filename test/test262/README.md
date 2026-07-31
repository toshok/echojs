# test262 subset probe (language-P1 / plans P8.1)

Host tooling that runs a curated slice of [tc39/test262] against a
built `ejs`, classifying every outcome — the exhaustiveness check the
34-probe census in `test/modernization/` can't provide, and the source
of the prioritized feature list for language-P3.  The full CI lane is
language-P4; this probe is deliberately lighter.

## Running it

The runner needs (a) a test262 checkout and (b) a workroot: the
`//:srcdir-tree` layout with `//lib:generated` at `lib/generated/` and
a stage executable copied to `./ejs` (the same assembly
`buck-test-stage.sh` performs).

```sh
git clone --depth 1 https://github.com/tc39/test262.git /tmp/test262
node test/test262/run-test262.mjs run \
    --suite /tmp/test262 --ejs /path/to/workroot \
    --jobs 10 --cap-builtins 3 --out results.jsonl
node test/test262/run-test262.mjs report --in results.jsonl --md report.md
```

`--filter substr` restricts to matching test paths (quick iteration).

## Selection policy

- `test/language/**` — everything (the P8 target area).
- `test/built-ins/**` — stratified: the first `--cap-builtins` (3)
  tests of every leaf directory, so every constructor and method gets
  probed without the full 24k volume.
- `test/harness/**` — everything (validates the harness files
  themselves compile and run).
- `intl402/` and `staging/` — out of scope.

## Probe simplifications (vs a conforming runner)

- Harness files and the test are concatenated into one program (echojs
  compiles one module; there is no multi-script realm).
- Tests without a strictness flag run **sloppy only**; `onlyStrict`
  tests get `"use strict";` prepended to the concatenation.  A
  conforming runner runs unflagged tests in both modes.
- `flags: [module]` tests are **skipped** (`skip-module`): test262
  module tests import `*_FIXTURE.js` specifiers with extensions, which
  the compiler's static module gathering doesn't resolve yet.  Module
  conformance needs its own pass in language-P3/P4.
- Negative tests pass on any nonzero exit at the expected phase (parse
  → compile fails; runtime → binary exits nonzero); the error type is
  not matched.
- No `$262` host object; tests needing it fail at runtime and show up
  bucketed under their feature.

## Outcome classes

`pass`, `fail-parse` (compiler parse error), `fail-compile` (compiler
error/crash after parse), `fail-crash` (binary died on a signal),
`fail-runtime` (uncaught error / assert), `fail-async` (exit 0 but no
`Test262:AsyncTestComplete`), `fail-negative-*` (negative test
accepted), `compile-timeout` / `run-timeout`, `skip-module`,
`skip-agent`.

The report groups failures by frontmatter `features:` — that table,
descending, is the payoff ordering for language-P3.

[tc39/test262]: https://github.com/tc39/test262
