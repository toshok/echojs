# release-P2 results: platform packages

Status: DONE 2026-07-30 (P9.2 in plans.md).  Everything layers on the
release-P1 dist tarball; nothing here touches the compiler or runtime.

## The one structural fact all three packages obey

The driver resolves `include/` and `lib/` relative to its own binary
via `argv[0]` and **does not chase symlinks** (ejs_exe_dirname in
ejs-es6.ts).  So no package may put a symlink on PATH pointing into the
layout; each uses an absolute-path exec shim instead, and the layout
stays whole in one directory.

## Tarball additions (buck-dist.sh)

- `dist-info` at the tarball root: sh-sourceable metadata
  (`EJS_VERSION`, `EJS_TRIPLE`, `EJS_SHORT_TRIPLE`, `EJS_OS`,
  `EJS_LLVM_MAJOR`).  Every packaging layer reads it from the artifact
  instead of re-deriving facts about the build.
- `install.sh` (from `packaging/install.sh`) ships at the root.

## The packages

- **Prefix installer** (`packaging/install.sh`, in every tarball):
  `./install.sh [--prefix /usr/local]` copies the tree to
  `$PREFIX/lib/echojs/<name>` and writes the `$PREFIX/bin/ejs` exec
  shim; `--uninstall` reverses it (and only removes a shim that points
  into its own tree).  Best-effort post-install advice: probes the
  driver's conventional LLVM locations for a matching major, and on
  linux checks `ldconfig -p` for the libuv/libunwind **dev** symlinks
  (`libuv.so ` with the trailing space — `.so.1` alone is just the
  runtime lib).  Warnings only; the driver stays the authority and
  fails loudly.
- **Homebrew** (`packaging/homebrew/`): `echojs.rb.in` +
  `make-formula.sh --tarball … [--url …] [--out …]` which fills url,
  sha256, version, and llvm major from the tarball's dist-info
  (refuses non-macos tarballs).  The formula installs the whole layout
  under `libexec` and `bin.write_exec_script`s the shim — a brew link
  farm is exactly the symlink shape the driver can't follow.  (Learned
  the hard way: `(bin/"ejs").write_exec_script …` creates a *directory*
  `bin/ejs/`; the receiver is the dir, the argument the target.)
  `depends_on "llvm@N"`: homebrew-core keeps a versioned alias for the
  current major (llvm@22 → llvm 22.1.8 today) and a real versioned
  formula after it's superseded, so the pin survives brew's llvm
  moving on.  Homebrew rejects loose formula files now — install goes
  through a tap (`brew tap-new`; release-P3 should push generated
  formulas to a real `toshok/homebrew-echojs`).
- **npm wrapper** (`packaging/npm/`, name `echojs`): postinstall
  downloads `echojs-<version>-<short-triple>.tar.gz` from the
  `v<version>` GitHub release (darwin-arm64, linux-arm64, linux-x64 →
  short triples) and unpacks it as `dist/`; `bin/ejs.js` spawnSync's
  `dist/bin/ejs` (node realpaths the main module, so the `.bin`
  symlink is harmless — `__dirname` is the package's true location).
  `EJS_NPM_TARBALL=/path/to/tarball` overrides the download: the CI
  path, the offline escape, and the only way to test before release-P3
  hosts assets.  Wrapper version == dist version == release tag; the
  publish flow is release-P3's.

## Verification

- `//:test-dist` grew step 4: install into a scratch prefix, compile
  through the shim, uninstall, assert nothing is left.  Runs on all
  three CI platforms.
- CI macos job: builds the formula from the just-built tarball via a
  throwaway `--no-git` tap, `brew install` + shim compile + `brew
  test` + uninstall; then the npm wrapper via `npm pack` + install
  with `EJS_NPM_TARBALL` + shim compile.  Linux jobs: the npm smoke.
- Locally verified on macos arm64: full brew tap/install/compile/
  `brew test`/uninstall cycle green; npm pack/install/compile green;
  `//:dist` + `//:test-dist` green.

## Removed

2016-era bitrot from the make/llvm-3.4 build: `debian/`,
`release/` (trusty64 vagrant), `packaging/npm/package.json.in`,
`packaging/.gitignore`.

## Follow-ons

- release-P3 owns: hosted release assets (which make the formula's
  `--url` mode and the npm download path real), a
  `toshok/homebrew-echojs` tap, npm publish, version stamping (root
  package.json is still 0.0.0), and deb/rpm if tarball+install.sh
  proves insufficient.
- ~~The npm package name `echojs` may be taken on the registry —
  check at first publish (scoped fallback).~~
  RESOLVED in release-P3: it was taken (an unrelated 0.1.4); the
  wrapper is scoped — `@pirouette/echojs` (the @pirouette npm org).
- macos ld's version-min warnings (release-P1 follow-on) now also
  surface through every package's compile smoke; still harmless,
  still noisy.
