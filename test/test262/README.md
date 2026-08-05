# test262: probe + CI lane

Host tooling that runs [tc39/test262] against a built `ejs`,
classifying every outcome.  Three uses:

- **Probe**: any selection, reported by feature/area — the
  exhaustiveness check behind the payoff list.
- **CI lane**: `lane.sh` — a small fixed selection against the pinned
  suite SHA (`suite.sha`), checked against a per-platform expectations
  file.  CI runs it on macOS and linux-arm64 and fails on any
  regression (expected-pass test failing) or stale expectation
  (expected-fail test passing).
- **Full suite**: `.github/workflows/test262-full.yml` — every
  in-scope test, sharded across parallel Linux runners on each push
  and PR, ratcheted against `full-baseline.json`.

## The CI lane

```sh
git clone https://github.com/tc39/test262.git /tmp/test262
git -C /tmp/test262 checkout "$(cat test/test262/suite.sha)"
./test/test262/lane.sh --suite /tmp/test262           # check
./test/test262/lane.sh --suite /tmp/test262 --update  # regenerate expectations
```

Without `--ejs` the script assembles a workroot from buck2 outputs
(stage1).  The lane selection is every 18th `test/language/**` test
(proportional across directories), 1 per `built-ins` leaf directory,
and all of `harness/` — a per-test smoke that rides along in a
platform's build job; the comprehensive number is the sharded full
suite.  The expectations file is checked by membership (a listed test
may fail any way; an unlisted one must pass); `skip` entries mark
environment-sensitive tests whose outcome is ignored.  After feature
work, rerun with `--update` and commit the diff.  Bumping `suite.sha`
requires an `--update` run in the same commit.

Each lane platform owns an expectations file — `expectations.txt`
(macOS arm64, the dev platform, the default) and
`expectations-linux-arm64.txt` — because crash and timeout classes
vary by platform even where semantics don't.  Regenerate on the
platform that checks it: locally with `--update` for macOS, or run CI
via workflow_dispatch with `update-lane-expectations` and commit the
uploaded `lane-expectations-<platform>` artifact.

## The full suite

`test262-full.yml` is a reusable workflow with no triggers of its own:
`build-and-test.yml` calls it on the platform whose `test262-suite`
input is `full` — Linux x86_64 — once that platform's build is done,
so it runs
inside the same workflow run, waits on no other platform, and nothing
builds the compiler twice.  That build uploads its stage1 workroot and
the suite checkout, the shard matrix extracts that archive and runs
`--shard K/N` slices of it, and the collect job concatenates the
results, checks that every shard reported, and posts the report to the
run summary.  The shard count lives in `SHARDS` at the top of that
file, alongside the matrix list it has to agree with.

The ratchet is `full-baseline.json` — `{evaluated, pass, tolerance}`.
Per-test expectations are the lane's contract and don't scale to 45k
rows, so the full run holds two numbers instead: coverage must not
shrink and the pass count must not drop by more than `tolerance`.
When a run beats the committed floor, the report rewrites the file
unprompted and says so — every improving run's
`test262-full-results` artifact carries a ready-to-commit baseline,
and committing it is the (deliberately manual) act that raises the
floor.  Lowering it — accepting a regression, e.g. after a scope
change — requires running CI with the `update-test262-baseline`
input.  A regression without that reddens CI, and because
`release.yml` runs the same matrix, blocks a release.

The baseline is a Linux x86_64 number, like the lane expectations
files it sits alongside: regenerate each on the platform that checks
it.

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

- `test/language/**` and `test/annexB/language/**` — every
  `--stride-language`th test of the sorted walk (1 = everything).
- `test/built-ins/**` and `test/annexB/built-ins/**` — stratified: the
  first `--cap-builtins` (3) tests of every leaf directory, so every
  constructor and method gets probed without the full 24k volume.
  `--cap-builtins all` takes the lot.
- `test/harness/**` — everything (validates the harness files
  themselves compile and run).
- `intl402/` (no `Intl`) and `staging/` (not normative) — out of scope.

The whole suite is `--stride-language 1 --cap-builtins all`: ~48.7k
tests, of which ~3.6k are skipped as out of scope for AOT (below) and
~45.2k are evaluated.  `--shard K/N` runs slice K of N over a sorted
list, so N runners partition the selection without coordinating.

## Out of scope for AOT

Some tests no ahead-of-time engine can pass, whatever echojs
implements: they need a compiler at run time, or a host hook that has
no AOT meaning.  They are classified `skip-unsupported` before
compiling — otherwise each costs a compile+link to reach a foregone
failure — with a `needs` tag on the row, and they are excluded from
the pass rate and never enter `expectations.txt`.  A test is out of
scope when it

- is tagged `cross-realm`, `ShadowRealm`, or `dynamic-import`;
- lives under `language/eval-code/`, `annexB/language/eval-code/`,
  `built-ins/eval/`, or `language/statements/with/`;
- calls `eval(...)` or `Function(...)` in its body, uses a `with`
  statement (dynamic scope — the same compile-time-unknowable bindings
  as eval), or reaches `$262.agent`;
- includes a harness file that does either — `fnGlobalObject.js` is
  `Function("return this;")()`, so its dependents are out too.  That
  set is derived from the suite, not listed, so it tracks SHA bumps.

Unimplemented features are *not* out of scope: `Temporal`, `Atomics`,
`SharedArrayBuffer` and friends stay in the denominator, because an
AOT engine could implement them.

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
- Only the part of `$262` echojs can honor (`global`, `gc`,
  `detachArrayBuffer`, `destroy`); tests reaching for the rest are out
  of scope above.

## Outcome classes

`pass`, `fail-parse` (compiler parse error), `fail-compile` (compiler
error/crash after parse), `fail-crash` (binary died on a signal),
`fail-runtime` (uncaught error / assert), `fail-async` (exit 0 but no
`Test262:AsyncTestComplete`), `fail-negative-*` (negative test
accepted), `compile-timeout` / `run-timeout`, `skip-unsupported`.

The report groups failures by frontmatter `features:` — that table,
descending, is the payoff ordering for feature work.

[tc39/test262]: https://github.com/tc39/test262
