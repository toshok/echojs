#!/bin/bash
# Invoked by //:ejs.exe.stage{1,2,3}.  Copies the --srcdir tree into a
# writable work dir and self-compiles ejs-es6.js in it, either with the
# node-hosted stage0 compiler or with the previous stage's executable.
set -euo pipefail

TREE="$1"       # //:srcdir-tree
MODE="$2"       # "node" (stage0 compiler) or "exe" (previous stage binary)
COMPILER="$3"   # node: //lib:generated dir; exe: previous ejs.exe.stageN
LLVM_NODE="$4"  # node: //node-llvm:llvm.node; exe: "-"
LLVM_BIN="$5"   # directory holding llc/opt (and llvm-config)
EXTRA_FLAGS="${6:-}"  # extra compiler flags for the self-compile, e.g. --ir

abspath() {
    if [ -d "$1" ]; then
        (cd "$1" && pwd)
    else
        echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
    fi
}

OUT_ABS="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"
COMPILER_ABS="$(abspath "$COMPILER")"
if [ "$LLVM_NODE" != "-" ]; then
    LLVM_NODE_ABS="$(abspath "$LLVM_NODE")"
fi

WORK="$TMP/work"
rm -rf "$WORK"
mkdir -p "$WORK"
cp -RL "$TREE"/. "$WORK/"
chmod -R u+w "$WORK"

cd "$WORK"

# llc/opt for codegen; Apple clang++ for the final link on macOS so SDK
# discovery works.
export PATH="$LLVM_BIN:$PATH"
if [ "$(uname -s)" = "Darwin" ]; then
    export CXX="${CXX:-/usr/bin/clang++}"
    export SDKROOT="${SDKROOT:-$(/usr/bin/xcrun --show-sdk-path)}"
fi

EJS_ARGS=(--srcdir --leave-temp --moduledir node-compat --moduledir ejs-llvm)
if [ -n "$EXTRA_FLAGS" ]; then
    EJS_ARGS+=($EXTRA_FLAGS)
fi
EJS_ARGS+=(ejs-es6.js)

if [ "$MODE" = "node" ]; then
    mkdir -p lib/generated
    cp -RL "$COMPILER_ABS"/. lib/generated/
    NODE_PATH="$(dirname "$LLVM_NODE_ABS")" \
        node lib/generated/ejs-es6.js "${EJS_ARGS[@]}"
else
    cp "$COMPILER_ABS" ./ejs.exe.prev
    chmod +x ./ejs.exe.prev
    ./ejs.exe.prev "${EJS_ARGS[@]}"
fi

test -f ejs-es6.js.exe
cp ejs-es6.js.exe "$OUT_ABS"
chmod +x "$OUT_ABS"
