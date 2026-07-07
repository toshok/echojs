#!/bin/bash
# Builds the node-llvm addon (build/Release/llvm.node), which the stage0
# (node-hosted) compiler uses to drive llvm.  //node-llvm:llvm.node picks
# up the built addon; run this after changing node-llvm sources or
# switching llvm versions.
#
# usage: ./build-addon.sh [llvm-prefix]   (default: /opt/homebrew/opt/llvm)
set -euo pipefail
cd "$(dirname "$0")"

LLVM_PREFIX="${1:-/opt/homebrew/opt/llvm}"
LLVM_CONFIG="$LLVM_PREFIX/bin/llvm-config"

export PATH="$LLVM_PREFIX/bin:$PATH"

LLVM_CXXFLAGS="$($LLVM_CONFIG --cxxflags) -fno-rtti" \
LLVM_INCLUDEDIR="$($LLVM_CONFIG --includedir)" \
LLVM_DEFINES="" \
LLVM_LINKFLAGS="$($LLVM_CONFIG --ldflags --libs)" \
MIN_OSX_VERSION=11.0 \
    npx -y node-gyp@10 rebuild

node -e "require('./build/Release/llvm.node'); console.log('llvm.node loads ok')"
