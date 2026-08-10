#!/bin/bash
# Regenerate ci/selfcompile.icprofile: build an instrumented
# (-fic-profile-dump) compiler node-hosted, run it on a full
# self-compile under EJS_IC_PROFILE=1, and capture the exit dump.
# Run from the repo root after `buck2 build //lib:generated
# //:srcdir-tree //node-llvm:llvm.node`.  The profile only pays when
# it matches current site numbering — regenerate after big compiler
# reshuffles; staleness decays to guard misses, never wrongness.
set -euo pipefail
cd "$(dirname "$0")/.."

ART_TREE="$(buck2 targets --show-output //:srcdir-tree 2>/dev/null | awk '{print $2}')"
ART_GEN="$(buck2 targets --show-output //lib:generated 2>/dev/null | awk '{print $2}')"
ART_NODE="$(buck2 targets --show-output //node-llvm:llvm.node 2>/dev/null | awk '{print $2}')"
[ -d "$ART_TREE" ] && [ -d "$ART_GEN" ] && [ -f "$ART_NODE" ] || {
    echo "buck artifacts missing — build //lib:generated //:srcdir-tree //node-llvm:llvm.node first" >&2
    exit 1
}

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp -RL "$ART_TREE"/. "$WORK/"
chmod -R u+w "$WORK"
mkdir -p "$WORK/lib/generated"
cp -RL "$ART_GEN"/. "$WORK/lib/generated/"

export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
export CXX="${CXX:-/usr/bin/clang++}"
if [ "$(uname -s)" = "Darwin" ]; then
    export SDKROOT="${SDKROOT:-$(/usr/bin/xcrun --show-sdk-path)}"
fi
export NODE_PATH="$(cd "$(dirname "$ART_NODE")" && pwd)"
export EJS_OBJ_CACHE=off

EJS_ARGS=(--srcdir --leave-temp --moduledir node-compat --moduledir ejs-llvm
          -I "maam=$WORK/external-deps/echojs-maam/dist/src/index")

echo "== building instrumented compiler (node-hosted, -fic-profile-dump)"
(cd "$WORK" && node lib/generated/ejs-es6.js "${EJS_ARGS[@]}" -fic-profile-dump ejs-es6.js)

echo "== training self-compile (EJS_IC_PROFILE=1)"
TRAIN_LOG="$WORK/train.log"
(cd "$WORK" && cp ejs-es6.js.exe ejs.trained && chmod +x ejs.trained &&
    EJS_IC_PROFILE=1 ./ejs.trained "${EJS_ARGS[@]}" ejs-es6.js > "$TRAIN_LOG" 2>&1)

OUT=ci/selfcompile.icprofile
{
    cat <<'HDR'
# IC training profile for the self-compile (see --ic-profile /
# -fic-profile-dump).  Regenerate with ci/regen-icprofile.sh after
# changes that shift property-site numbering; a stale profile only
# decays to guard misses (checked tier), never wrong answers.
HDR
    grep -E "ICPROFP? " "$TRAIN_LOG" | sed 's/^.*ICPROF/ICPROF/' | LC_ALL=C sort
} > "$OUT"
echo "wrote $OUT: $(grep -c ICPROF "$OUT") records"
