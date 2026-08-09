#!/bin/bash
# Invoked by //:test-eir-lowtier.  Compiles test/eir-lowtier1.js twice with
# the stage0 (node-hosted) compiler — once plain, once with -flowtier
# (which swaps the lowtier_* function bodies for hand-built low-tier EIR,
# see lib/eir/lowtier-probe.ts) — runs both executables, and fails unless:
#   - both outputs match the committed expected-out byte for byte;
#   - the injected build actually differs from the plain one; and
#   - the injected build's LLVM IR contains every low-tier float op
#     (fadd/fsub/fmul/fdiv/fcmp olt) — so a silent injection no-op or a
#     stale prebuilt llvm.node missing the FP bindings fails loudly.
set -euo pipefail

TREE="$1"       # //:srcdir-tree
GENERATED="$2"  # //lib:generated
TEST_FILES="$3" # //test:files
LLVM_BIN="$4"   # directory holding llc/opt

REPO="${TMP%%/buck-out/*}"
OUT_ABS="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"

WORK="$TMP/lowtier"
rm -rf "$WORK"
mkdir -p "$WORK"
cp -RL "$TREE"/. "$WORK/"
chmod -R u+w "$WORK"
mkdir -p "$WORK/lib/generated"
cp -RL "$GENERATED"/. "$WORK/lib/generated/"
mkdir -p "$WORK/test"
cp -RL "$TEST_FILES"/. "$WORK/test/"
chmod -R u+w "$WORK/test"

export PATH="$LLVM_BIN:$PATH"
export NODE_PATH="$REPO/node_modules:$REPO/node-llvm/build/Release"
if [ "$(uname -s)" = "Darwin" ]; then
    export SDKROOT="${SDKROOT:-$(/usr/bin/xcrun --show-sdk-path)}"
fi

cd "$WORK/test"
EXPECTED=expected/eir-lowtier1.js.expected-out
# -fverify-eir: the injected low-tier bodies must verify (opt-in for
# plain compiles, on in every test lane)
EJS_ARGS=(--srcdir --moduledir ../node-compat --moduledir ../ejs-llvm -fverify-eir)

# NOTE: `run` is invoked in an `if` condition, which disables `set -e`
# inside it (the classic bash trap — an early version of this script
# printed OK over an aborting executable).  Every step therefore checks
# its own status explicitly.
run() {
    echo "== plain build =="
    node ../lib/generated/ejs-es6.js "${EJS_ARGS[@]}" eir-lowtier1.js \
        || { echo "ERROR: plain compile failed"; return 1; }
    ./eir-lowtier1.js.exe > plain.out \
        || { echo "ERROR: plain executable failed"; return 1; }
    diff -u "$EXPECTED" plain.out \
        || { echo "ERROR: plain output does not match expected"; return 1; }
    cp eir-lowtier1.js.exe plain.exe

    echo "== injected build =="
    mkdir -p "$WORK/ltmp"
    TMPDIR="$WORK/ltmp" \
        node ../lib/generated/ejs-es6.js "${EJS_ARGS[@]}" --leave-temp -flowtier eir-lowtier1.js \
        || { echo "ERROR: injected compile failed"; return 1; }
    ./eir-lowtier1.js.exe > injected.out \
        || { echo "ERROR: injected executable failed"; return 1; }
    diff -u "$EXPECTED" injected.out \
        || { echo "ERROR: injected output does not match expected"; return 1; }

    if cmp -s plain.exe eir-lowtier1.js.exe; then
        echo "ERROR: injected binary is identical to the plain build (injection no-op?)"
        return 1
    fi

    LL=$(ls "$WORK"/ltmp/eir-lowtier1.js.*.ll 2>/dev/null | head -1)
    if [ -z "$LL" ]; then
        echo "ERROR: no --leave-temp .ll found under $WORK/ltmp"
        return 1
    fi
    for pat in "fadd double" "fsub double" "fmul double" "fdiv double" "fcmp olt double"; do
        if ! grep -q "$pat" "$LL"; then
            echo "ERROR: '$pat' missing from $LL — the low tier was not emitted"
            return 1
        fi
    done
    echo "lowtier e2e OK: outputs match expected, binaries differ, all f64 ops in the IR"
}

if run > "$OUT_ABS" 2>&1; then
    tail -1 "$OUT_ABS"
else
    cat "$OUT_ABS" >&2
    exit 1
fi
