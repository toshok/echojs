# echojs (npm wrapper)

An ahead-of-time compiler for JavaScript.  This package downloads the
platform's prebuilt echojs toolchain at install time (macOS arm64,
Linux arm64/x86_64) and exposes its `ejs` driver on your PATH.

    npm install -g @pirouette/echojs
    ejs -o hello hello.js && ./hello

Compiling needs an LLVM toolchain with the major version the release
was built against (`ejs` discovers it and fails loudly otherwise —
macOS: `brew install llvm`; Linux: https://apt.llvm.org, plus libuv and
libunwind development packages for linking).

`EJS_NPM_TARBALL=/path/to/echojs-<version>-<triple>.tar.gz` makes the
install use a local dist tarball instead of downloading.
