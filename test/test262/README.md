# test262: probe + CI lane

Host tooling that runs a curated slice of [tc39/test262] against a
built `ejs`, classifying every outcome.  Two uses:

- **Probe** (language-P1): the full curated selection, reported by
  feature/area — the exhaustiveness check behind the language-P3
  payoff list.
- **CI lane** (language-P4): `lane.sh` — a smaller fixed selection
  against the pinned suite SHA (`suite.sha`), checked against
  `expectations.txt`.  CI (the macOS bootstrap job) fails on any
  regression (expected-pass test failing) or stale expectation
  (expected-fail test passing).

## The CI lane

```sh
git clone https://github.com/tc39/test262.git /tmp/test262
git -C /tmp/test262 checkout "$(cat test/test262/suite.sha)"
./test/test262/lane.sh --suite /tmp/test262           # check
./test/test262/lane.sh --suite /tmp/test262 --update  # regenerate expectations
```

Without `--ejs` the script assembles a workroot from buck2 outputs
(stage1).  The lane selection is every 6th `test/language/**` test
(proportional across directories), 2 per `built-ins` leaf directory,
and all of `harness/` — sized for a CI runner; shrink the stride
toward 1 as features land.  `expectations.txt` is checked by
membership (a listed test may fail any way; an unlisted one must
pass); `skip` entries mark environment-sensitive tests whose outcome
is ignored.  After feature work, rerun with `--update` and commit the
diff — the shrinking file is the conformance ratchet.  Bumping
`suite.sha` requires an `--update` run in the same commit.

## Running the probe

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
- Unflagged tests compile with `--script` (script-goal semantics) and
  run **sloppy only**; `onlyStrict` tests additionally get
  `"use strict";` prepended.  A conforming runner runs unflagged tests
  in both modes.
- `flags: [module]` tests compile under the compiler's module-goal
  default; sibling `*_FIXTURE.js` files are staged next to the
  assembled source so relative imports resolve.  `negative: {phase:
  resolution}` expects a compile-time failure (resolution happens at
  compile time in an AOT world).
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
accepted), `compile-timeout` / `run-timeout`, `skip-agent`.

The report groups failures by frontmatter `features:` — that table,
descending, is the payoff ordering for language-P3.

[tc39/test262]: https://github.com/tc39/test262
