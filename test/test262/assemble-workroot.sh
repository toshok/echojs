#!/bin/bash
# Assemble a test262 workroot into the given directory: the
# //:srcdir-tree layout, with //lib:generated at lib/generated/ and the
# stage1 compiler copied to ./ejs — the layout run-test262.mjs expects
# behind --ejs (the same one buck-test-stage.sh stages).  Extra buck2
# args (e.g. --config llvm.prefix=...) pass through after the dest.
#
#   assemble-workroot.sh <dest-dir> [buck2 args...]
set -euo pipefail

DEST="$1"; shift
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"

# buck2's progress spam goes to stderr, so it's captured rather than
# inherited — but on failure it's the only diagnostic there is, so it
# gets replayed instead of swallowed
ERRLOG="$(mktemp "${TMPDIR:-/tmp}/t262-buck2-XXXXXX.log")"
STATUS=0
OUTS="$(cd "$REPO" && buck2 build "$@" //:srcdir-tree //lib:generated //:ejs.exe.stage1 --show-full-output 2>"$ERRLOG")" || STATUS=$?
if [ "$STATUS" -ne 0 ]; then
    cat "$ERRLOG" >&2
    rm -f "$ERRLOG"
    echo "assemble-workroot: buck2 build failed (exit $STATUS)" >&2
    exit "$STATUS"
fi
rm -f "$ERRLOG"
TREE="$(echo "$OUTS" | awk '$1 == "root//:srcdir-tree" {print $2}')"
GENERATED="$(echo "$OUTS" | awk '$1 == "root//lib:generated" {print $2}')"
STAGE_EXE="$(echo "$OUTS" | awk '$1 == "root//:ejs.exe.stage1" {print $2}')"

mkdir -p "$DEST"
cp -RL "$TREE"/. "$DEST/"
chmod -R u+w "$DEST"
mkdir -p "$DEST/lib/generated"
cp -RL "$GENERATED"/. "$DEST/lib/generated/"
cp "$STAGE_EXE" "$DEST/ejs"
chmod +x "$DEST/ejs"
