#!/bin/bash
# Invoked by //lib:generated.  Produces the equivalent of lib/generated/:
# the compiler (the //lib:tsjs tree — ES-module JS) converted to
# CommonJS so stage0 can run under node, with the same import rewrites
# lib/Makefile applied:
#   "@llvm"        -> "llvm"          (resolved via NODE_PATH to node-llvm)
#   "@node-compat/"-> ""              (use node's own os/path/fs/...)
#
# The module conversion is tsc in --allowJs transpile mode.
# typescript comes from the repo's
# node_modules, which buck2 doesn't track as an input (same treatment as
# in buck-gen-tsjs.sh).  The repo root is recovered from $TMP, which
# buck2 always places under <repo>/buck-out/.
#
# The "$maam" import variable (lib/eir/oracle) rewrites to the maam CJS
# build, staged under external-deps/echojs-maam/dist/cjs so the require
# resolves inside this tree.
set -euo pipefail

TSJS="$1"      # //lib:tsjs — $TSJS/ejs-es6.js + $TSJS/lib/**.js
MAAM_CJS="$2"  # //external-deps:maam-cjs — the maam CommonJS build

REPO="${TMP%%/buck-out/*}"
TSC="$REPO/node_modules/typescript/bin/tsc"

mkdir -p "$OUT"
OUTABS="$(cd "$OUT" && pwd)"

# stage the ES-module tree, applying the import rewrites on the way in
# (pre-conversion: tsc then turns the rewritten imports into require()s)
STAGE="$TMP/genjs-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"

stage_one() {
    local src="$1" dst="$STAGE/$2"
    mkdir -p "$(dirname "$dst")"
    sed -e 's,"@llvm","llvm",' -e "s,'@llvm','llvm'," -e 's,@node-compat/,,' \
        -e 's,"$maam","../../external-deps/echojs-maam/dist/cjs/index",' \
        -e "s,'\$maam','../../external-deps/echojs-maam/dist/cjs/index'," \
        "$src" > "$dst"
}

(cd "$TSJS/lib" && find . -name "*.js" | sed 's,^\./,,') | while read -r f; do
    stage_one "$TSJS/lib/$f" "lib/$f"
done

stage_one "$TSJS/ejs-es6.js" "ejs-es6.js"

# the maam CJS build rides the same conversion pass (already CommonJS —
# tsc just passes it through, downleveling syntax to the shared target).
# staged before the cd: $MAAM_CJS is relative to the genrule cwd.
(cd "$MAAM_CJS" && find . -name "*.js" | sed 's,^\./,,') | while read -r f; do
    stage_one "$MAAM_CJS/$f" "external-deps/echojs-maam/dist/cjs/$f"
done

cd "$SRCDIR"

# host-config.js is generated (staged at $SRCDIR root by the genrule)
stage_one host-config.js "lib/host-config.js"

for f in acorn/acorn-es6.js \
         astring/astring-es6.js \
         esprima/esprima-es6.js \
         escodegen/escodegen-es6.js \
         estraverse/estraverse-es6.js \
         esutils/esutils-es6.js \
         esutils/lib/code.js \
         esutils/lib/keyword.js \
         esutils/lib/ast.js; do
    stage_one "compiler-js/$f" "external-deps/$f"
done

# one tsc transpile over the whole tree: ES modules -> CommonJS.
# --allowJs only, no checkJs — no type-checking, just the module
# conversion.  --esModuleInterop matches babel's
# default/namespace-import interop against CJS modules (llvm, glob, ...).
JS_FILES=$(cd "$STAGE" && find . -name "*.js" | sort)
(cd "$STAGE" && node "$TSC" \
    --ignoreConfig \
    --allowJs \
    --target es2016 \
    --module commonjs \
    --esModuleInterop \
    --rootDir . \
    --outDir "$OUTABS" \
    $JS_FILES)
