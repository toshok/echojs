# release-plan: packaging and distribution

Bucket plan; the ordering spine lives in `docs/plans.md` (milestone
references look like `release-P1`).  New bucket (2026-07-25): the
buck2 build is great for cross-platform development, but it is not the
answer for people who just want to download a package and go.

## Phases

- [x] **release-P1 — Relocatable binary artifact.**  DONE 2026-07-30 —
      docs/release-p1-results.md.  Define what an
      installed echojs IS: the `ejs` driver binary, the runtime static
      libraries (`libecho.a` + friends), the srcdir headers/manifests
      the driver needs, and a pinned LLVM toolchain policy (today the
      driver spawns `opt`/`llc` from a baked bindir — an installed
      package must either vendor the LLVM tools it needs or discover a
      compatible installation and fail loudly; the llvm@16-on-PATH
      miscompile taught us "fail loudly").  Deliverable: a `buck2
      build //:dist` (or script) that produces a self-contained,
      relocatable tarball per platform, exercised in CI.
- [x] **release-P2 — Platform packages.**  DONE 2026-07-30 —
      docs/release-p2-results.md.  Homebrew formula/cask for
      macOS (arm64 first), a deb/rpm or tarball+install.sh for Linux
      (arm64 + x86_64 — the CI bootstrap matrix already proves the
      targets).  An npm wrapper package is worth considering for the
      node-adjacent audience (postinstall fetches the platform
      tarball).  (Shipped: tarball+install.sh — deb/rpm deferred
      unless it proves insufficient — plus the formula generator and
      the npm wrapper; every package uses an absolute-path exec shim
      because the driver doesn't chase symlinks.  Hosted asset URLs,
      the real tap, and npm publish are release-P3's.)
- [x] **release-P3 — Versioning + release automation.**  DONE
      2026-07-30 — docs/release-p3-results.md.  Semver
      scheme, a changelog discipline, tagged releases built by CI from
      the bootstrap matrix (a release is a green matrix + packaged
      artifacts + smoke test of the installed package compiling a
      hello-world on a clean machine/container).  (Shipped:
      CHANGELOG.md + prepare-release.sh + reusable bootstrap.yml +
      release.yml drafting the release and running bare-container/
      fresh-runner install smokes; the tap push and npm publish legs
      are shell-gated on their secrets.  The first pushed tag is the
      end-to-end proof.)
- [ ] **release-P4 — Getting-started surface.**  A quickstart README
      path that assumes the package (not the repo): install, compile a
      file, link a multi-module program; document the supported
      language subset honestly (pointing at language-plan status)
      and the flag surface (`--types`, GC knobs) that users may touch.
