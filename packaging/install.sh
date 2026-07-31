#!/bin/sh
# echojs prefix installer (release-P2).  Ships at the root of the dist
# tarball; run it from the unpacked directory:
#
#     tar xzf echojs-<version>-<short-triple>.tar.gz
#     cd echojs-<version>-<short-triple>
#     sudo ./install.sh                  # into /usr/local
#     ./install.sh --prefix ~/.local     # anywhere writable
#
# The tree is copied whole to $PREFIX/lib/echojs/<name> (the driver
# resolves include/ and lib/ relative to its own binary, so bin/,
# include/ and lib/ must stay together) and $PREFIX/bin/ejs becomes a
# tiny exec shim holding the absolute path — the driver does not chase
# symlinks, so a symlink would break that resolution.
#
#     ./install.sh --uninstall [--prefix P]   removes both again
set -eu

PREFIX=/usr/local
UNINSTALL=no

usage() {
    echo "usage: $0 [--prefix PREFIX] [--uninstall]" >&2
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)   [ $# -ge 2 ] || usage; PREFIX="$2"; shift 2 ;;
        --prefix=*) PREFIX="${1#--prefix=}"; shift ;;
        --uninstall) UNINSTALL=yes; shift ;;
        -h|--help)  usage ;;
        *)          usage ;;
    esac
done

HERE="$(cd "$(dirname "$0")" && pwd)"
[ -f "$HERE/dist-info" ] || {
    echo "error: $HERE/dist-info not found — run this from an unpacked echojs dist directory" >&2
    exit 1
}
# EJS_VERSION, EJS_TRIPLE, EJS_SHORT_TRIPLE, EJS_OS, EJS_LLVM_MAJOR
. "$HERE/dist-info"

NAME="echojs-$EJS_VERSION-$EJS_SHORT_TRIPLE"
DEST="$PREFIX/lib/echojs/$NAME"
SHIM="$PREFIX/bin/ejs"

if [ "$UNINSTALL" = yes ]; then
    rm -rf "$DEST"
    rmdir "$PREFIX/lib/echojs" 2>/dev/null || true
    # only remove the shim if it is ours (points into $DEST)
    if [ -f "$SHIM" ] && grep -q "lib/echojs/$NAME/bin/ejs" "$SHIM" 2>/dev/null; then
        rm -f "$SHIM"
    fi
    echo "uninstalled $NAME from $PREFIX"
    exit 0
fi

mkdir -p "$PREFIX/bin" "$PREFIX/lib/echojs"
rm -rf "$DEST"
cp -R "$HERE" "$DEST"
rm -f "$DEST/install.sh"

cat > "$SHIM" <<EOF
#!/bin/sh
exec "$DEST/bin/ejs" "\$@"
EOF
chmod +x "$SHIM"

echo "installed $NAME"
echo "  compiler:  $SHIM"
echo "  layout:    $DEST"

# Best-effort environment advice; the driver itself verifies the LLVM
# major on every compile and fails loudly, so these are warnings only.
find_opt() {
    for d in \
        "/opt/homebrew/opt/llvm@$EJS_LLVM_MAJOR/bin" \
        /opt/homebrew/opt/llvm/bin \
        "/usr/local/opt/llvm@$EJS_LLVM_MAJOR/bin" \
        /usr/local/opt/llvm/bin \
        "/usr/lib/llvm-$EJS_LLVM_MAJOR/bin"; do
        [ -x "$d/opt" ] || continue
        if "$d/opt" --version 2>/dev/null | grep -q "LLVM version $EJS_LLVM_MAJOR\."; then
            echo "$d"
            return 0
        fi
    done
    if opt --version 2>/dev/null | grep -q "LLVM version $EJS_LLVM_MAJOR\."; then
        echo "(PATH)"
        return 0
    fi
    return 1
}

if BINDIR="$(find_opt)"; then
    echo "  llvm $EJS_LLVM_MAJOR:   $BINDIR"
else
    echo "warning: no LLVM $EJS_LLVM_MAJOR opt/llc found in the conventional locations." >&2
    if [ "$EJS_OS" = macos ]; then
        echo "  install it with: brew install llvm" >&2
    else
        echo "  install it from https://apt.llvm.org (or your distribution's llvm-$EJS_LLVM_MAJOR packages)" >&2
    fi
    echo "  or point LLVM_BINDIR at a bindir containing a matching opt/llc." >&2
fi

if [ "$EJS_OS" = linux ] && command -v ldconfig >/dev/null 2>&1; then
    # the trailing space matters: "libuv.so " is the -dev symlink the
    # final link needs; "libuv.so.1" is just the runtime library
    for spec in libuv:libuv1-dev libunwind:libunwind-dev; do
        lib="${spec%%:*}"; pkg="${spec#*:}"
        if ! ldconfig -p 2>/dev/null | grep -q "$lib\.so "; then
            echo "warning: $lib development package not found (compiled programs link against it)" >&2
            echo "  e.g. apt install $pkg" >&2
        fi
    done
fi
