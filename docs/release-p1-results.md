# release-P1 results: relocatable dist artifact + LLVM toolchain policy

Status: DONE 2026-07-30 (P9.1 in plans.md).

## What an installed echojs IS

The driver always had a latent installed layout (every non-`--srcdir`
path in ejs-es6.ts); release-P1 makes it a real, shippable artifact:

    echojs-<version>-<short-triple>/
      bin/ejs                          the self-hosted compiler (stage2)
      include/*.h                      runtime headers (-I at final link)
      lib/<triple>/libecho.a           merged runtime archive
      lib/<triple>/libpcre16.a
      lib/<triple>/libdouble-conversion.a
      lib/node-compat.ejs              native-module manifest (lib/ is
      lib/<short-triple>/libejsnodecompat-module.a   the moduledir)
      LICENSE.txt, README.md

`buck2 build //:dist` produces the tarball (in an out *directory*,
since the version isn't knowable at analysis time; version comes from
package.json until release-P3 owns versioning).  `buck-dist.sh` repacks
`//:srcdir-tree` — the same bits the bootstrap matrix proves — plus the
stage2 executable, which the stage3 fixed point vouches for.

Deliberately excluded: the ejs-llvm module.  Its manifest bakes the
build machine's `llvm-config --ldflags --libs`, and only the bootstrap
imports `@llvm`; reusable native modules are compiler-P4 (P9.5).

## LLVM toolchain policy: discover, verify, fail loudly

The dist can't vendor `opt`/`llc` (~100MB+ per platform) and can't
trust the baked build-machine bindir (on a user's machine that path may
hold a different major — and the llvm@16-on-PATH incident showed a
mismatched `opt` miscompiles *silently*: llvm-22 module-init stores
became `unreachable` traps with exit code 0).  So:

- `//lib:host-config.js` now bakes `LLVM_MAJOR` (from `llvm-config
  --version` at build time) alongside `LLVM_BINDIR`.
- The driver resolves the tool bindir lazily (first compile, so
  `--help` never probes), in order:
  1. `LLVM_BINDIR` env — explicit override, `""` = plain PATH; still
     version-checked, `EJS_LLVM_NO_VERSION_CHECK=1` forces past it;
  2. the baked build-machine bindir;
  3. conventional locations (`/opt/homebrew/opt/llvm{@N,}/bin`,
     `/usr/local/opt/llvm{@N,}/bin` on macos; `/usr/lib/llvm-N/bin` on
     linux), then bare PATH.
- Every candidate is verified by parsing `LLVM version (\d+)` out of
  `opt --version`; the first matching-major candidate wins; if none
  match the driver exits with an actionable message (what it tried,
  what each had, how to install/point at LLVM N).
- The probe captures output via `sh -c '... > tmpfile'` +
  `readFileSync` — the one capture mechanism the node-hosted and
  self-hosted drivers share (self-hosted `spawn` returns only the exit
  status).
- The never-spawned `llvm-as` entry in the tool table is gone.

Verified by hand: baked-path success; `LLVM_BINDIR=/nonexistent` fails
loudly (exit 255, names the required major); `LLVM_BINDIR=""` with no
opt on PATH fails loudly; with llvm on PATH succeeds; the
no-version-check escape reaches the tool spawn and fails there via
spawnSyncChecked (pre-existing behavior).

## Smoke test

`buck2 build //:test-dist` unpacks the tarball into scratch and, with
the build LLVM *off* PATH (discovery must work as a user's machine
would):

1. compiles + runs a no-import program (classes, template strings,
   arrow lambdas) — output checked;
2. compiles + runs a `@node-compat/path` import — exercises the lib/
   manifest scan and the `lib/<short-triple>/` module archive;
3. asserts the fail-loudly path: `LLVM_BINDIR=/nonexistent` must exit
   nonzero with the version-policy message and produce no executable.

## CI

Both jobs build `//:test-dist` then `//:dist --out` (the stage builds
are shared, so this adds only the repack + smoke compile) and upload
the tarball: `echojs-dist-macos-arm64`, `echojs-dist-linux-{arm64,x86_64}`.

## Gates

- tsc typechecks clean (tsconfig.json + test --noEmit)
- `//:test-eir` green
- full matrix `//:test-stage{0,1,2,3}` + shapes-off + lowtier green
- `//:dist` + `//:test-dist` green

## Follow-ons

- The runtime's EXCEPTIONS spew (ejs-exception) is noisy on stderr
  during every native-module import resolution — cosmetic, pre-existing,
  but every dist user compiling an `@node-compat` import sees it.
  Worth silencing before release-P2.
- `bin/ejs` ships unstripped (~debug-sized); strip at dist time once a
  symbol-preservation story exists.
- Linux compiled programs need libuv/libunwind dev packages at link
  time; the README documents it, release-P2 packaging should depend on
  them properly.
- macOS ld warns that dist archives (built for the SDK, 15.7) are newer
  than the default `-mmacosx-version-min` (osx_min 11.0 → linked 15.0
  objects); pre-existing in srcdir mode too, harmless but noisy.
