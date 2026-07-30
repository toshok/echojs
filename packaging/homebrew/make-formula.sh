#!/bin/sh
# Generate the echojs Homebrew formula from a //:dist tarball
# (release-P2).  Until release-P3 hosts tagged release artifacts the
# default URL is the local tarball itself (file://…), which is enough
# for `brew install --formula echojs.rb` and CI smoke tests; pass the
# hosted URL once one exists:
#
#     make-formula.sh --tarball dist-out/echojs-*.tar.gz \
#         [--url https://github.com/toshok/echojs/releases/download/vX/…] \
#         [--out echojs.rb]
set -eu

TARBALL= URL= OUT=echojs.rb

usage() {
    echo "usage: $0 --tarball PATH [--url URL] [--out PATH]" >&2
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) [ $# -ge 2 ] || usage; TARBALL="$2"; shift 2 ;;
        --url)     [ $# -ge 2 ] || usage; URL="$2"; shift 2 ;;
        --out)     [ $# -ge 2 ] || usage; OUT="$2"; shift 2 ;;
        *)         usage ;;
    esac
done
[ -n "$TARBALL" ] && [ -f "$TARBALL" ] || usage

HERE="$(cd "$(dirname "$0")" && pwd)"
ABS_TARBALL="$(cd "$(dirname "$TARBALL")" && pwd)/$(basename "$TARBALL")"
[ -n "$URL" ] || URL="file://$ABS_TARBALL"

# dist-info lives at <name>/dist-info inside the tarball; the exact
# member path avoids needing GNU tar's --wildcards
NAME="$(basename "$TARBALL" .tar.gz)"
INFO="$(tar -xzOf "$ABS_TARBALL" "$NAME/dist-info")"
eval "$INFO" # EJS_VERSION, EJS_TRIPLE, EJS_SHORT_TRIPLE, EJS_OS, EJS_LLVM_MAJOR

if [ "$EJS_OS" != macos ]; then
    echo "error: the homebrew formula wants a macos dist tarball (got EJS_OS=$EJS_OS)" >&2
    exit 1
fi

if command -v shasum >/dev/null 2>&1; then
    SHA256="$(shasum -a 256 "$ABS_TARBALL" | cut -d' ' -f1)"
else
    SHA256="$(sha256sum "$ABS_TARBALL" | cut -d' ' -f1)"
fi

sed -e "s|@URL@|$URL|" \
    -e "s|@SHA256@|$SHA256|" \
    -e "s|@VERSION@|$EJS_VERSION|" \
    -e "s|@LLVM_MAJOR@|$EJS_LLVM_MAJOR|" \
    "$HERE/echojs.rb.in" > "$OUT"

echo "wrote $OUT (version $EJS_VERSION, llvm@$EJS_LLVM_MAJOR, $URL)"
