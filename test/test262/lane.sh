#!/bin/bash
# The test262 CI lane: a fixed curated selection against
# the pinned suite SHA (suite.sha), checked against expectations.txt.
# Exits nonzero on any regression (expected-pass test failing) or stale
# expectation (expected-fail test passing).
#
#   lane.sh --suite <test262 checkout> [--ejs <workroot>] [--jobs N] \
#           [--expectations <file>] [--update]
#
# Without --ejs, assembles a workroot from buck2 outputs (srcdir-tree +
# lib/generated + the stage1 executable) — the same layout
# buck-test-stage.sh stages.  --update regenerates the expectations
# file instead of checking (run after feature work; commit the diff).
# --expectations selects the file — crash and timeout classes vary by
# platform, so each platform that runs the lane checks (and
# regenerates) its own.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"

# the lane's curated selection: every 18th language test (proportional
# across every directory), 1 test per built-ins leaf directory, all of
# harness — a per-test smoke sized to ride along in a platform build
# job.  The comprehensive number is the sharded full suite
# (test262-full.yml); the lane's job is exact per-test regressions on
# platforms the full suite doesn't cover.
STRIDE_LANGUAGE=18
CAP_BUILTINS=1

SUITE="" EJS_ROOT="" JOBS=6 UPDATE="" EXPECTATIONS="$HERE/expectations.txt"
while [ $# -gt 0 ]; do
    case "$1" in
        --suite) SUITE="$2"; shift 2 ;;
        --ejs) EJS_ROOT="$2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
        --expectations) EXPECTATIONS="$2"; shift 2 ;;
        --update) UPDATE=1; shift ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done
if [ -z "$SUITE" ]; then
    echo "usage: lane.sh --suite <test262 checkout> [--ejs <workroot>] [--jobs N] [--expectations <file>] [--update]" >&2
    exit 2
fi

WANT_SHA="$(cat "$HERE/suite.sha")"
GOT_SHA="$(git -C "$SUITE" rev-parse HEAD)"
if [ "$GOT_SHA" != "$WANT_SHA" ]; then
    echo "test262 checkout at $GOT_SHA, expectations pinned to $WANT_SHA" >&2
    echo "fetch it with:" >&2
    echo "  git -C '$SUITE' fetch --depth 1 origin $WANT_SHA && git -C '$SUITE' checkout $WANT_SHA" >&2
    exit 1
fi

CLEANUP=""
if [ -z "$EJS_ROOT" ]; then
    EJS_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/t262-lane-XXXXXX")"
    CLEANUP="$EJS_ROOT"
    "$HERE/assemble-workroot.sh" "$EJS_ROOT"
fi

RESULTS="${RESULTS:-$(mktemp "${TMPDIR:-/tmp}/t262-results-XXXXXX.jsonl")}"
STATUS=0
node "$HERE/run-test262.mjs" run \
    --suite "$SUITE" --ejs "$EJS_ROOT" --jobs "$JOBS" \
    --stride-language "$STRIDE_LANGUAGE" --cap-builtins "$CAP_BUILTINS" \
    --out "$RESULTS" \
    --expectations "$EXPECTATIONS" ${UPDATE:+--update-expectations} \
    || STATUS=$?

[ -n "$CLEANUP" ] && rm -rf "$CLEANUP"
exit $STATUS
