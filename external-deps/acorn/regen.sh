#!/bin/bash
# Regenerates acorn-es6.js: acorn's flat ESM bundle (dist/acorn.mjs),
# transpiled to ES5-level *syntax* while staying an ES module.
#
# Why transpile: the bundle is compiler input for stage1+ (the
# self-hosted compiler parses and compiles it), so it must stay inside
# the subset the echojs backend handles today.  As language-P3 lands
# features, the transpile step can shrink and eventually disappear
# (language-plan.md).
#
# The output is checked in; run this only to move to a newer acorn (bump
# ACORN_VERSION) and commit the result.  Versions are pinned so the
# regen is reproducible.
set -euo pipefail

ACORN_VERSION=8.18.0
BABEL_CORE_VERSION=8.0.1
PRESET_ENV_VERSION=8.0.2

cd "$(dirname "$0")"
OUT="$PWD/acorn-es6.js"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

npm init -y >/dev/null
npm install --no-audit --no-fund --silent \
    "acorn@$ACORN_VERSION" \
    "@babel/core@$BABEL_CORE_VERSION" \
    "@babel/preset-env@$PRESET_ENV_VERSION"

cat > transpile.mjs <<'EOF'
import { transformFileAsync } from "@babel/core";
import presetEnv from "@babel/preset-env";
import { writeFileSync } from "fs";
const res = await transformFileAsync("node_modules/acorn/dist/acorn.mjs", {
    configFile: false, babelrc: false, compact: false,
    // loose-mode equivalents (babel 8 spells them as assumptions)
    assumptions: {
        setPublicClassFields: true, constantSuper: true, noClassCalls: true,
        superIsCallableConstructor: true, ignoreFunctionLength: true,
        iterableIsArray: false, mutableTemplateObject: true,
        setSpreadProperties: true, ignoreToPrimitiveHint: true,
    },
    presets: [[presetEnv, {
        targets: { ie: "11" }, modules: false,
        // no generators in acorn's source; typeof-symbol helper is
        // unnecessary under echojs
        exclude: ["transform-regenerator", "transform-typeof-symbol"],
    }]],
});
writeFileSync(process.argv[2], res.code);
EOF
node transpile.mjs "$OUT"

echo "wrote $OUT (acorn $ACORN_VERSION)"
