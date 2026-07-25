# release-plan: packaging and distribution

Bucket plan; the ordering spine lives in `docs/plans.md` (milestone
references look like `release-P1`).  New bucket (2026-07-25): the
buck2 build is great for cross-platform development, but it is not the
answer for people who just want to download a package and go.

## Phases

- [ ] **release-P1 — Relocatable binary artifact.**  Define what an
      installed echojs IS: the `ejs` driver binary, the runtime static
      libraries (`libecho.a` + friends), the srcdir headers/manifests
      the driver needs, and a pinned LLVM toolchain policy (today the
      driver spawns `opt`/`llc` from a baked bindir — an installed
      package must either vendor the LLVM tools it needs or discover a
      compatible installation and fail loudly; the llvm@16-on-PATH
      miscompile taught us "fail loudly").  Deliverable: a `buck2
      build //:dist` (or script) that produces a self-contained,
      relocatable tarball per platform, exercised in CI.
- [ ] **release-P2 — Platform packages.**  Homebrew formula/cask for
      macOS (arm64 first), a deb/rpm or tarball+install.sh for Linux
      (arm64 + x86_64 — the CI bootstrap matrix already proves the
      targets).  An npm wrapper package is worth considering for the
      node-adjacent audience (postinstall fetches the platform
      tarball).
- [ ] **release-P3 — Versioning + release automation.**  Semver
      scheme, a changelog discipline, tagged releases built by CI from
      the bootstrap matrix (a release is a green matrix + packaged
      artifacts + smoke test of the installed package compiling a
      hello-world on a clean machine/container).
- [ ] **release-P4 — Getting-started surface.**  A quickstart README
      path that assumes the package (not the repo): install, compile a
      file, link a multi-module program; document the supported
      language subset honestly (pointing at language-plan status)
      and the flag surface (`--types`, GC knobs) that users may touch.
