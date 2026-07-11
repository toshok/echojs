#!/bin/bash
# Invoked by //lib:tsjs.  Produces the compiler as plain ES-module JS:
# .ts sources compile through tsc (strict; flags mirror tsconfig.json),
# .js sources copy through unchanged (the port is incremental).  Output
# layout:
#   $OUT/ejs-es6.js
#   $OUT/lib/{*.js, passes/*.js, eir/*.js}
# Both //lib:generated (babel for the node-hosted stage0) and
# //:srcdir-tree (stage1+ self-compiles) consume this tree.
#
# typescript comes from the repo's node_modules, which buck2 doesn't
# track as an input (same treatment as babel in buck-gen-js.sh).
set -euo pipefail

REPO="${TMP%%/buck-out/*}"
TSC="$REPO/node_modules/typescript/bin/tsc"

mkdir -p "$OUT"
OUTABS="$(cd "$OUT" && pwd)"

cd "$SRCDIR"

# stage into the output layout; tsc emits over the same tree
STAGE="$TMP/tsjs-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/lib"

for f in *.js *.ts passes/*.js passes/*.ts eir/*.js eir/*.ts; do
    [ -e "$f" ] || continue
    case "$f" in
        ejs-es6.js|ejs-es6.ts) continue ;;
    esac
    mkdir -p "$STAGE/lib/$(dirname "$f")"
    cp "$f" "$STAGE/lib/$f"
done
for f in ejs-es6.js ejs-es6.ts; do
    if [ -e "$f" ]; then cp "$f" "$STAGE/$f"; fi
done

# hand-written surface declarations for the vendored external-deps JS,
# committed in the esprima/escodegen submodules next to their .js
# (relative imports like ../../external-deps/escodegen/escodegen-es6
# typecheck against these; the .js resolves at runtime)
if [ -d compiler-js ]; then
    (cd compiler-js && find . -name "*.d.ts" | while read -r f; do
        mkdir -p "$STAGE/external-deps/$(dirname "$f")"
        cp "$f" "$STAGE/external-deps/$f"
    done)
fi

# copy the .js files through
(cd "$STAGE" && find . -name "*.js" | while read -r f; do
    mkdir -p "$OUTABS/$(dirname "$f")"
    cp "$f" "$OUTABS/$f"
done)

# compile the .ts files (flags mirror tsconfig.json)
TS_FILES=$(cd "$STAGE" && find . -name "*.ts" | sort)
if [ -n "$TS_FILES" ]; then
    (cd "$STAGE" && node "$TSC" \
        --ignoreConfig \
        --strict \
        --noUncheckedIndexedAccess \
        --noImplicitOverride \
        --noEmitOnError \
        --target es2016 \
        --module esnext \
        --moduleResolution bundler \
        --types node \
        --typeRoots "$REPO/node_modules/@types" \
        --rootDir . \
        --outDir "$OUTABS" \
        $TS_FILES)
fi
