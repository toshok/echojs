#!/bin/bash
# The test262 CI lane: a fixed curated selection against
# the pinned suite SHA (suite.sha), checked against expectations.txt.
# Exits nonzero on any regression (expected-pass test failing) or stale
# expectation (expected-fail test passing).
#
#   lane.sh --suite <test262 checkout> [--ejs <workroot>] [--jobs N] [--update]
#
# Without --ejs, assembles a workroot from buck2 outputs (srcdir-tree +
# lib/generated + the stage1 executable) — the same layout
# buck-test-stage.sh stages.  --update regenerates expectations.txt
# instead of checking (run after feature work; commit the diff).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"

# the lane's curated selection: every 6th language test (proportional
# across every directory), 2 tests per built-ins leaf directory, all of
# harness — sized to fit a CI runner; shrink the stride toward 1 as
# features land
STRIDE_LANGUAGE=6
CAP_BUILTINS=2

SUITE="" EJS_ROOT="" JOBS=6 UPDATE=""
while [ $# -gt 0 ]; do
    case "$1" in
        --suite) SUITE="$2"; shift 2 ;;
        --ejs) EJS_ROOT="$2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
        --update) UPDATE=1; shift ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done
if [ -z "$SUITE" ]; then
    echo "usage: lane.sh --suite <test262 checkout> [--ejs <workroot>] [--jobs N] [--update]" >&2
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
    --expectations "$HERE/expectations.txt" ${UPDATE:+--update-expectations} \
    || STATUS=$?

[ -n "$CLEANUP" ] && rm -rf "$CLEANUP"
exit $STATUS
