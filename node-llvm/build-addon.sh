#!/bin/bash
# Builds the node-llvm addon (build/Release/llvm.node), which the stage0
# (node-hosted) compiler uses to drive llvm.  //node-llvm:llvm.node runs
# this from a genrule; it also still works by hand for addon development.
#
# usage: ./build-addon.sh [llvm-prefix]   (default: /opt/homebrew/opt/llvm)
set -euo pipefail
cd "$(dirname "$0")"

LLVM_PREFIX="${1:-/opt/homebrew/opt/llvm}"
LLVM_CONFIG="$LLVM_PREFIX/bin/llvm-config"

# binding.gyp resolves nan with `require('nan')`.  A checkout build finds
# it by walking up to the repo's node_modules; a buck sandbox copy has no
# parent node_modules, so point NODE_PATH back at the checkout (the
# sandbox lives under <repo>/buck-out/...).
if [[ "$PWD" == */buck-out/* ]]; then
    export NODE_PATH="${PWD%%/buck-out/*}/node_modules${NODE_PATH:+:$NODE_PATH}"
fi

export PATH="$LLVM_PREFIX/bin:$PATH"

LLVM_CXXFLAGS="$($LLVM_CONFIG --cxxflags) -fno-rtti" \
LLVM_INCLUDEDIR="$($LLVM_CONFIG --includedir)" \
LLVM_DEFINES="" \
LLVM_LINKFLAGS="$($LLVM_CONFIG --ldflags --libs)" \
MIN_OSX_VERSION=11.0 \
    npx -y node-gyp@10 rebuild

node -e "require('./build/Release/llvm.node'); console.log('llvm.node loads ok')"
