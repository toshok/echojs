#!/bin/bash
# Invoked by //:test-stage{1,2,3}.  Assembles a repo-shaped tree (the
# --srcdir tree + test/ + the stage executable) and runs test/tester.js
# against it.  The genrule fails if any test fails; the test log is the
# output artifact.
set -euo pipefail

TREE="$1"       # //:srcdir-tree
GENERATED="$2"  # //lib:generated (tester requires ../lib/generated/.../host-config.js)
STAGE_EXE="$3"  # //:ejs.exe.stageN, or "-" for stage 0 (node-hosted)
STAGE_NUM="$4"  # N
TEST_FILES="$5" # //test:files
LLVM_BIN="$6"   # directory holding llc/opt
EXTRA_FLAGS="${7:-}"  # extra compiler flags, e.g. --ir

# node_modules (glob/colors/temp for the tester) come from the repo, same
# as the babel step in //lib:generated.
REPO="${TMP%%/buck-out/*}"

OUT_ABS="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"

WORK="$TMP/testroot"
rm -rf "$WORK"
mkdir -p "$WORK"
cp -RL "$TREE"/. "$WORK/"
chmod -R u+w "$WORK"
mkdir -p "$WORK/lib/generated"
cp -RL "$GENERATED"/. "$WORK/lib/generated/"
if [ "$STAGE_NUM" = "0" ]; then
    # stage 0 runs the babel'd compiler under node via the ../ejs driver
    printf '#!/bin/sh\ndir=$(cd `dirname $0`; pwd)\nexec node $dir/lib/generated/ejs-es6.js "$@"\n' > "$WORK/ejs"
    chmod +x "$WORK/ejs"
else
    cp "$STAGE_EXE" "$WORK/ejs.exe.stage$STAGE_NUM"
    chmod +x "$WORK/ejs.exe.stage$STAGE_NUM"
fi
mkdir -p "$WORK/test"
cp -RL "$TEST_FILES"/. "$WORK/test/"
chmod -R u+w "$WORK/test"

# the tester regenerates an expected-out (using node) when the test file
# is newer than it; the copies above have fresh mtimes, so re-stamp the
# expected outputs afterwards to keep them newer.
find "$WORK/test" -name '*.js' -exec touch {} +
find "$WORK/test/expected" -type f -exec touch {} +

export PATH="$LLVM_BIN:$PATH"
export NODE_PATH="$REPO/node_modules:$REPO/node-llvm/build/Release"
# the tester regenerates missing expected-outs by RUNNING node: keep that
# color-free even when the buck daemon inherited a colored dev shell
# (FORCE_COLOR writes ANSI into the expected files and poisons the diffs)
export NO_COLOR=1
unset FORCE_COLOR
if [ -n "$EXTRA_FLAGS" ]; then
    export EJS_EXTRA_FLAGS="$EXTRA_FLAGS"
fi
if [ "$(uname -s)" = "Darwin" ]; then
    export SDKROOT="${SDKROOT:-$(/usr/bin/xcrun --show-sdk-path)}"
fi

cd "$WORK/test"
if node tester.js -s "$STAGE_NUM" > "$OUT_ABS" 2>&1; then
    tail -5 "$OUT_ABS"
else
    echo "stage$STAGE_NUM tests FAILED:" >&2
    tail -40 "$OUT_ABS" >&2
    exit 1
fi
