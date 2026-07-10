#!/bin/bash
# Invoked by //lib:generated.  Produces the equivalent of lib/generated/:
# the compiler (the //lib:tsjs tree — tsc-compiled + passed-through JS)
# run through babel (so stage0 can run under node), with the same import
# rewrites lib/Makefile applies:
#   "@llvm"        -> "llvm"          (resolved via NODE_PATH to node-llvm)
#   "@node-compat/"-> ""              (use node's own os/path/fs/...)
#
# babel and its presets come from the repo's node_modules, which buck2
# doesn't track as an input (mirrors the Makefile treating node_modules as
# an ambient dev dependency).  The repo root is recovered from $TMP, which
# buck2 always places under <repo>/buck-out/.
set -euo pipefail

TSJS="$1"  # //lib:tsjs — $TSJS/ejs-es6.js + $TSJS/lib/**.js

REPO="${TMP%%/buck-out/*}"
BABEL_JS="$REPO/node_modules/@babel/cli/bin/babel.js"
BABELRC="$REPO/.babelrc"

mkdir -p "$OUT"
OUTABS="$(cd "$OUT" && pwd)"

run_babel() {
    local src="$1" dst="$2"
    mkdir -p "$(dirname "$dst")"
    node "$BABEL_JS" --config-file "$BABELRC" "$src" \
        | sed -e 's,"@llvm","llvm",' -e "s,'@llvm','llvm'," -e 's,@node-compat/,,' \
        > "$dst"
}

(cd "$TSJS/lib" && find . -name "*.js" | sed 's,^\./,,') | while read -r f; do
    run_babel "$TSJS/lib/$f" "$OUTABS/lib/$f"
done

run_babel "$TSJS/ejs-es6.js" "$OUTABS/ejs-es6.js"

cd "$SRCDIR"

# host-config.js is generated (staged at $SRCDIR root by the genrule)
run_babel host-config.js "$OUTABS/lib/host-config.js"

for f in esprima/esprima-es6.js \
         escodegen/escodegen-es6.js \
         estraverse/estraverse-es6.js \
         esutils/esutils-es6.js \
         esutils/lib/code.js \
         esutils/lib/keyword.js \
         esutils/lib/ast.js; do
    run_babel "compiler-js/$f" "$OUTABS/external-deps/$f"
done
