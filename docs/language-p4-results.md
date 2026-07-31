# language-P4 results: the test262 CI lane (plans P8.4)

2026-07-31.  The conformance harness from the language-P1 probe is now
a gating CI lane: a fixed curated selection of test262, run against a
pinned suite SHA, checked against a checked-in expectations file.
Regressions (an expected-pass test failing) and stale expectations (an
expected-fail test passing) both fail CI — the expectations file only
shrinks, a conformance ratchet.

## The pieces

- **`test/test262/lane.sh`** — the lane entry point, local and CI.
  Verifies the suite checkout matches `test/test262/suite.sha`,
  assembles a workroot from buck2 outputs (srcdir-tree +
  `//lib:generated` + the stage1 executable — the same layout
  `buck-test-stage.sh` stages) unless `--ejs` supplies one, runs the
  selection, and checks (or with `--update`, regenerates)
  `test/test262/expectations.txt`.
- **Selection** — every 6th `test/language/**` test of the sorted walk
  (a global stride samples every directory proportionally), 2 tests
  per `built-ins` leaf directory, all of `harness/`: 6,121 tests,
  ~9 minutes at 10 jobs on an M-series laptop.  The stride is the
  growth knob: shrink toward 1 as features land.
- **Expectations** — one `<status> <path>` line per expected-failing
  test, sorted; checking is by *membership* (a listed test may fail
  any way — status drift between failure kinds doesn't churn the
  file), `skip` entries mark environment-sensitive tests whose outcome
  is ignored either way, and `harness-error` (the runner itself broke)
  is never baselined.  Regenerating preserves `skip` lines.
- **Runner changes** (`run-test262.mjs`) — `--stride-language N`,
  `--expectations FILE`, `--update-expectations`; the run exits
  nonzero on any regression or stale entry.
- **CI** — a `test262 lane` step in the macOS bootstrap job, right
  after the stage ladder (the stage1 executable is already built; the
  step adds only the suite fetch + the run).  macOS only:
  expectations are generated on macos-arm64, the dev platform, so the
  checking platform matches the generating platform.  The suite is
  fetched shallow at the pinned SHA (`git fetch --depth 1 <sha>`).

## The baseline

Suite pinned at `b363f29d` (2026-07-31).  Lane totals at the pinning
run (stage1, macos-arm64):

| outcome | count |
|---|---|
| pass | 2,584 |
| fail-runtime | 1,981 |
| fail-compile | 1,056 (async-generator + BigInt + dynamic-import gates dominate) |
| fail-crash | 192 |
| fail-parse | 156 |
| fail-async | 6 |
| fail-negative-runtime-passed | 2 |
| skip-module / skip-agent | 142 / 2 |

2,584 of 5,977 runnable = 43% — the expectations file starts at 3,393
entries.  (The language-P1 probe's richer selection isn't directly
comparable; the lane's per-feature detail comes from `report` on the
lane results as usual.)

Verified end-to-end locally: the clean run exits 0; deleting an
expectations entry (making the still-failing test "expected to pass")
exits 1 with a `REGRESSION` line; listing a passing test exits 1 with
`STALE`; `skip` entries suppress both directions; `--update`
preserves `skip` lines and rewrites the rest from results.

## Policy

- **After feature work**: rerun `lane.sh --suite <checkout> --update`
  and commit the (shrinking) diff alongside the feature.
- **Bumping `suite.sha`**: requires an `--update` run in the same
  commit (new tests arrive expected-fail or passing; the selection
  shifts with the sorted walk).
- **Flakes**: a test that behaves differently on the CI runner than
  locally (timing, ICU) gets a `skip` line, not a looser check.
- The kangax-derived suite (`test/`) stays until test262 parity; the
  probe (`run-test262.mjs run` without a stride, `report`) remains the
  exploration tool — the lane is the gate.
