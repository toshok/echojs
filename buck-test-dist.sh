#!/bin/bash
# Invoked by //:test-dist.  Exercises the dist tarball the way an
# installer would: unpack it somewhere unrelated to the repo, compile
# and run programs with the installed layout (no --srcdir), and check
# that the LLVM version policy fails loudly when pointed at nothing.
set -euo pipefail

DIST="$1"     # //:dist (directory holding the tarball)
LLVM_BIN="$2" # build-config LLVM bindir; kept OFF PATH — discovery must
              # find a toolchain on its own (baked path or conventional
              # locations), which is exactly what an end user relies on

# $OUT is genrule-relative; absolutize before cd'ing around
OUT="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"

log() { echo "$@" | tee -a "$OUT"; }
: > "$OUT"

WORK="$TMP/dist-test"
rm -rf "$WORK"
mkdir -p "$WORK"

TARBALL="$(echo "$DIST"/echojs-*.tar.gz)"
test -f "$TARBALL"
tar -C "$WORK" -xzf "$TARBALL"
ROOT="$(echo "$WORK"/echojs-*)"
test -x "$ROOT/bin/ejs"
log "unpacked $(basename "$TARBALL")"

# the final link wants a C++ driver; on macos use Apple clang++ so SDK
# discovery works (same as buck-stage.sh)
if [ "$(uname -s)" = "Darwin" ]; then
    export CXX="${CXX:-/usr/bin/clang++}"
    export SDKROOT="${SDKROOT:-$(/usr/bin/xcrun --show-sdk-path)}"
fi

cd "$WORK"

# 1: a plain program, no imports
cat > hello.js <<'EOF'
class Greeter {
    constructor(who) { this.who = who; }
    greet() { return `hello, ${this.who}`; }
}
let parts = ["from", "the", "installed", "echojs"].map((w) => w);
console.log(new Greeter(parts.join(" ")).greet());
EOF
"$ROOT/bin/ejs" -q -o hello.exe hello.js >> "$OUT" 2>&1
actual="$(./hello.exe)"
expected="hello, from the installed echojs"
if [ "$actual" != "$expected" ]; then
    log "FAIL hello: got '$actual', want '$expected'"
    exit 1
fi
log "PASS hello"

# 2: a program importing the node-compat native module (exercises the
# lib/ manifest scan + lib/<short-triple>/ module archive)
cat > pathtest.js <<'EOF'
import * as path from "@node-compat/path";
console.log(path.basename(path.join("/a/b", "c.js")));
EOF
"$ROOT/bin/ejs" -q -o pathtest.exe pathtest.js >> "$OUT" 2>&1
actual="$(./pathtest.exe)"
if [ "$actual" != "c.js" ]; then
    log "FAIL pathtest: got '$actual', want 'c.js'"
    exit 1
fi
log "PASS pathtest"

# 3: the fail-loudly policy: pointed at a bindir with no LLVM, the
# driver must refuse (mentioning the required major), not miscompile
rm -f nollvm.exe
set +e
LLVM_BINDIR=/nonexistent "$ROOT/bin/ejs" -q -o nollvm.exe hello.js > nollvm.log 2>&1
status=$?
set -e
cat nollvm.log >> "$OUT"
if [ "$status" -eq 0 ] || [ -f nollvm.exe ]; then
    log "FAIL nollvm: expected a loud failure, got exit $status"
    exit 1
fi
if ! grep -q "requires LLVM" nollvm.log; then
    log "FAIL nollvm: no version-policy message in the failure output"
    exit 1
fi
log "PASS nollvm (exit $status)"

log "test-dist OK"
