#!/bin/bash
# Invoked by //:dist.  Repacks the --srcdir tree (whose libraries the
# bootstrap matrix already proved) plus the stage2 executable into the
# relocatable installed layout the driver's non---srcdir mode expects:
#
#   bin/ejs                          the self-hosted compiler (stage2)
#   include/*.h                      runtime headers (-I at the final link)
#   lib/<triple>/libecho.a           runtime + pcre + double-conversion
#   lib/<triple>/libpcre16.a
#   lib/<triple>/libdouble-conversion.a
#   lib/node-compat.ejs              native-module manifest
#   lib/<short-triple>/libejsnodecompat-module.a
#
# The LLVM tools are NOT vendored: the driver discovers a matching-major
# opt/llc at runtime and fails loudly otherwise (the release-P1 policy;
# see the llvm_bindir() resolution in ejs-es6.ts).
#
# $OUT is a directory holding echojs-<version>-<short-triple>.tar.gz
# (version isn't knowable at buck analysis time, so the tarball name
# can't be the genrule out itself).
set -euo pipefail

TREE="$1"         # //:srcdir-tree
EXE="$2"          # //:ejs.exe.stage2
TRIPLE="$3"       # Triple.toString(), e.g. arm64-apple-macos
SHORT_TRIPLE="$4" # Triple.toShortString(), e.g. arm64-macos
OSNAME="$5"       # macos | linux
PKG_JSON="$6"     # //:package.json (version source until release-P3)
LICENSE="$7"      # LICENSE.txt

VERSION="$(sed -n 's/.*"version": *"\([^"]*\)".*/\1/p' "$PKG_JSON" | head -1)"
test -n "$VERSION"

NAME="echojs-$VERSION-$SHORT_TRIPLE"
mkdir -p "$OUT"
ROOT="$TMP/$NAME"
rm -rf "$ROOT"
mkdir -p "$ROOT/bin" "$ROOT/include" "$ROOT/lib/$TRIPLE" "$ROOT/lib/$SHORT_TRIPLE"

cp "$EXE" "$ROOT/bin/ejs"
chmod +x "$ROOT/bin/ejs"

# headers: the srcdir tree keeps them at runtime/*.h
cp "$TREE"/runtime/*.h "$ROOT/include/"

# link-time libraries, in the exact paths target_libecho()/
# target_extra_libs() resolve relative to bin/ejs
cp "$TREE/runtime/out/$TRIPLE/libecho.a" "$ROOT/lib/$TRIPLE/libecho.a"
cp "$TREE/external-deps/pcre-$OSNAME/.libs/libpcre16.a" "$ROOT/lib/$TRIPLE/libpcre16.a"
cp "$TREE/external-deps/double-conversion-$OSNAME/double-conversion/libdouble-conversion.a" \
   "$ROOT/lib/$TRIPLE/libdouble-conversion.a"

# the node-compat native module: manifest scanned from lib/, archive
# from lib/<arch>-<os>/ (do_final_link's non---srcdir module path).
# ejs-llvm is deliberately left out: its manifest bakes the build
# machine's `llvm-config --ldflags --libs`, and only the bootstrap
# imports @llvm (reusable native modules are compiler-P4 / P9.5).
cp "$TREE/node-compat/node-compat.ejs" "$ROOT/lib/node-compat.ejs"
cp "$TREE/node-compat/libejsnodecompat-module.a" "$ROOT/lib/$SHORT_TRIPLE/libejsnodecompat-module.a"

cp "$LICENSE" "$ROOT/LICENSE.txt"

cat > "$ROOT/README.md" <<EOF
# echojs $VERSION ($SHORT_TRIPLE)

An ahead-of-time compiler for JavaScript.

## Requirements

- LLVM $(sed -n "s/.*LLVM_MAJOR = '\([0-9]*\)'.*/\1/p" "$TREE/lib/host-config.js") (\`opt\`/\`llc\`) — macos: \`brew install llvm\`;
  linux: https://apt.llvm.org.  \`ejs\` discovers a matching installation
  and refuses to run with a different major; \`LLVM_BINDIR\` in the
  environment points it somewhere specific.
- a C++ linker driver (\`clang++\` on PATH, or set \`CXX\`)$([ "$OSNAME" = linux ] && printf '\n- libuv and libunwind development packages (linked into compiled programs)')

## Use

    $NAME/bin/ejs -o hello hello.js && ./hello

The directory is relocatable; keep bin/, include/ and lib/ together.
EOF

tar -C "$TMP" -czf "$OUT/$NAME.tar.gz" "$NAME"
