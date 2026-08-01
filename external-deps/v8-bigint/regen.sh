#!/bin/bash
# Re-extract V8's standalone bigint library (the BigInt core the runtime
# uses — see docs/bigint-plan.md).  The library is deliberately
# self-contained: its DEPS file forbids including anything outside
# src/bigint, so extraction is a plain file copy.
set -euo pipefail

V8_COMMIT=2ad1b92f41e8e86bc106b9b6095d575e16b99a48

HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

git clone --depth 1 --filter=blob:none --sparse https://github.com/v8/v8.git "$TMP/v8"
git -C "$TMP/v8" fetch --depth 1 origin "$V8_COMMIT"
git -C "$TMP/v8" checkout "$V8_COMMIT"
git -C "$TMP/v8" sparse-checkout set src/bigint

rm -rf "$HERE/src"
mkdir -p "$HERE/src/bigint"
cp "$TMP/v8/src/bigint/"*.h "$TMP/v8/src/bigint/"*.cc "$HERE/src/bigint/"
cp "$TMP/v8/LICENSE.v8" "$HERE/LICENSE.v8"
