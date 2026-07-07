#!/bin/bash
# Invoked by //:srcdir-tree.  Assembles a directory that looks enough like
# a source checkout for `ejs --srcdir` to compile ejs-es6.js in it:
#
#   ejs-es6.js                       compiler driver source
#   lib/*.js, lib/passes/*.js        compiler sources (incl. host-config.js)
#   external-deps/<esprima,...>      JS modules the compiler imports
#   external-deps/pcre-<os>/.libs/libpcre16.a
#   external-deps/double-conversion-<os>/double-conversion/libdouble-conversion.a
#   runtime/*.h                      passed via -I at the final link
#   runtime/out/<triple>/libecho.a   runtime + parson + invoke-closure-catch.o
#   node-compat/{node-compat.ejs,libejsnodecompat-module.a}
#   ejs-llvm/{ejs-llvm.ejs,libejsllvm-module.a}
set -euo pipefail

TRIPLE="$1"        # Triple.toString(), e.g. arm64-apple-macos
SHORT_TRIPLE="$2"  # Triple.toShortString(), e.g. arm64-macos
OSNAME="$3"        # macos | linux (also selects ar vs libtool merge below)
HDRS="$4"          # //runtime:headers
LIBECHO="$5"       # //runtime:echo[static]
ICC_O="$6"         # //runtime:platform-icc-o
PCRE_A="$7"        # //external-deps:pcre-build[lib]
DC_A="$8"          # //external-deps:double-conversion-build
EXT_JS="$9"        # //external-deps:compiler-js
LIB_JS="${10}"     # //lib:es6-srcs
HOST_CONFIG="${11}" # //lib:host-config.js
EJS_MAIN="${12}"   # //:ejs-es6.js
NC_EJS="${13}"     # //node-compat:node-compat.ejs
NC_A="${14}"       # //node-compat:node-compat[static]
LLVM_EJS="${15}"   # //ejs-llvm:ejs-llvm.ejs
LLVM_A="${16}"     # //ejs-llvm:ejs-llvm[static]
DTOA_A="${17}"     # //runtime:echo-dtoa[static]
OBJC_A="${18}"     # //runtime:echo-objc[static] on macos, "-" elsewhere

mkdir -p "$OUT"
ROOT="$(cd "$OUT" && pwd)"

# runtime headers + libecho.a: merge the runtime archives (C, C++, objc)
# and the llc'd trampoline object into the single archive the compiler
# links against, the way runtime/Makefile produces it.
mkdir -p "$ROOT/runtime/out/$TRIPLE"
cp -RL "$HDRS"/. "$ROOT/runtime/"
LIB="$ROOT/runtime/out/$TRIPLE/libecho.a"
cp "$ICC_O" "$TMP/ejs-invoke-closure-catch.o"
ARCHIVES=("$LIBECHO" "$DTOA_A")
if [ "$OBJC_A" != "-" ]; then
    ARCHIVES+=("$OBJC_A")
fi
if [ "$OSNAME" = "macos" ]; then
    libtool -static -o "$LIB" "${ARCHIVES[@]}" "$TMP/ejs-invoke-closure-catch.o" 2>/dev/null
else
    MERGE="$TMP/libecho-merge"
    rm -rf "$MERGE"
    mkdir -p "$MERGE"
    for a in "${ARCHIVES[@]}"; do
        (cd "$MERGE" && ar x "$(cd "$(dirname "$a")" && pwd)/$(basename "$a")")
    done
    ar rs "$LIB" "$MERGE"/*.o "$TMP/ejs-invoke-closure-catch.o"
fi
# some spots use the short triple for the runtime dir; provide both
mkdir -p "$ROOT/runtime/out/$SHORT_TRIPLE"
cp "$LIB" "$ROOT/runtime/out/$SHORT_TRIPLE/libecho.a"

# external-deps: static libs where --srcdir mode expects them + JS modules
mkdir -p "$ROOT/external-deps/pcre-$OSNAME/.libs"
cp "$PCRE_A" "$ROOT/external-deps/pcre-$OSNAME/.libs/libpcre16.a"
mkdir -p "$ROOT/external-deps/double-conversion-$OSNAME/double-conversion"
cp "$DC_A" "$ROOT/external-deps/double-conversion-$OSNAME/double-conversion/libdouble-conversion.a"
cp -RL "$EXT_JS"/. "$ROOT/external-deps/"

# compiler sources
mkdir -p "$ROOT/lib"
cp -RL "$LIB_JS"/. "$ROOT/lib/"
cp "$HOST_CONFIG" "$ROOT/lib/host-config.js"
cp "$EJS_MAIN" "$ROOT/ejs-es6.js"

# native modules
mkdir -p "$ROOT/node-compat"
cp "$NC_EJS" "$ROOT/node-compat/node-compat.ejs"
cp "$NC_A" "$ROOT/node-compat/libejsnodecompat-module.a"
mkdir -p "$ROOT/ejs-llvm"
cp "$LLVM_EJS" "$ROOT/ejs-llvm/ejs-llvm.ejs"
cp "$LLVM_A" "$ROOT/ejs-llvm/libejsllvm-module.a"
