#!/bin/bash
# Invoked by //lib:generated.  Produces the equivalent of lib/generated/:
# the compiler sources run through babel (so stage0 can run under node),
# with the same import rewrites lib/Makefile applies:
#   "@llvm"        -> "llvm"          (resolved via NODE_PATH to node-llvm)
#   "@node-compat/"-> ""              (use node's own os/path/fs/...)
#
# babel and its presets come from the repo's node_modules, which buck2
# doesn't track as an input (mirrors the Makefile treating node_modules as
# an ambient dev dependency).  The repo root is recovered from $TMP, which
# buck2 always places under <repo>/buck-out/.
set -euo pipefail

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

cd "$SRCDIR"

for f in *.js passes/*.js; do
    case "$f" in
        ejs-es6.js) continue ;;
    esac
    run_babel "$f" "$OUTABS/lib/$f"
done

run_babel ejs-es6.js "$OUTABS/ejs-es6.js"

for f in esprima/esprima-es6.js \
         escodegen/escodegen-es6.js \
         estraverse/estraverse-es6.js \
         esutils/esutils-es6.js \
         esutils/lib/code.js \
         esutils/lib/ast.js \
         esutils/lib/keyword.js; do
    run_babel "compiler-js/$f" "$OUTABS/external-deps/$f"
done
