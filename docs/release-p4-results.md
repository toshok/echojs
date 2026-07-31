# release-P4 results: getting-started surface

Status: DONE 2026-07-31 (P9.4 in plans.md).  Documentation only, plus
a one-line quickstart pointer in the dist tarball's README.

## What changed

README.md rewritten around the released package instead of the repo
build:

- **Install** leads with what is actually live for v0.2.0: npm
  (`@pirouette/echojs` — 0.2.0 is `latest` on the registry, the OIDC
  publish worked) and the release tarball + `install.sh`.  The
  Homebrew formula is noted as shipping with each release, tap
  pending.  Prerequisites (LLVM 22 + the fail-loudly policy, clang++,
  linux libuv/libunwind) stated once, up front.
- **Quickstart**: hello-world, then a multi-module program (relative
  extensionless imports + `@node-compat/path`).  Both examples were
  compiled and run against an actual dist tarball before being
  written down; the macOS ld version-min warning is acknowledged as
  harmless.
- **Language support, honestly**: the solid ES2015 core (backed by
  the suite and self-compilation), then the named missing-features
  list straight from language-plan's census (async/await, `?.`,
  `??`, `**`, class fields, object spread, BigInt, newer stdlib) and
  the deliberate node-divergence pins.  Links to
  docs/language-plan.md as the tracker.
- **Flags and knobs**: the user-relevant surface from the real
  `--help` (-o, -O suites + -f pass flags + --print-passes,
  --moduledir, debugging flags) and environment (LLVM_BINDIR, CXX,
  the EJS_GC_* knob set).  `--types`/`--record-types` documented
  honestly: source-checkout-under-node only — the shipped self-hosted
  binary declines them.
- **Building from source** condensed and corrected: the "only builds
  on OSX" claim replaced (linux arm64/x86_64 bootstrap in CI on
  every push), stale CircleCI badge replaced with the GitHub Actions
  badge + a release badge.
- The dist tarball README gained a pointer to the repo quickstart.

## Verified

- Both quickstart examples compiled and ran against the dist layout
  (`hello, world!` / `ECHOJS`).
- Flag descriptions transcribed from the current `ejs --help`, not
  memory.
- npm/asset/tap availability checked against the live registry and
  release before being claimed.
- `//:test-dist` green after the buck-dist.sh README addition.

## Follow-ons

- The tarball curl example pins v0.2.0 URLs; prepare-release.sh (or
  release automation) could stamp the README's version at release
  time — recorded for release-P3's automation bucket.
- When the tap lands, promote Homebrew into the Install section
  proper.
- The macOS ld version-min warning keeps earning its "being fixed"
  parenthetical (release-P1 follow-on).
