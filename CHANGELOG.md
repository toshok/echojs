# Changelog

All notable changes to echojs are recorded here, newest first.  The
format follows [Keep a Changelog](https://keepachangelog.com); versions
follow [semver](https://semver.org) with the pre-1.0 reading: while the
major is 0, a minor bump may break things and a patch bump may not.

Releases are cut by `packaging/prepare-release.sh <version>`, which
rolls the Unreleased section into a dated version section, stamps the
version everywhere it lives, and tags `v<version>`; pushing the tag
runs the release pipeline (a release is a green bootstrap matrix +
packaged artifacts + a clean-machine install smoke — see
`.github/workflows/release.yml`).

## [Unreleased]

### Added

- `--types` works in the self-hosted compiler: the MAAM abstract
  interpreter (external-deps/echojs-maam) is compiled into the
  bootstrap, so the shipped binary runs the same type analysis the
  node-hosted compiler does, with byte-identical output.
- Module system: `export * from` and `export * as ns from`, and
  `.js`-suffixed relative import specifiers (NodeNext output style).
- Standard library: `Object.entries`/`values`/`fromEntries`/`hasOwn`,
  `Array.prototype.includes`/`at`/`flat`/`flatMap`/`findLast`, and
  `String.prototype.at`/`padStart`/`padEnd`/`trimStart`/`trimEnd`/
  `replaceAll`.

### Fixed

- Strings containing an embedded `U+0000` are handled correctly
  end-to-end: source files no longer truncate at a raw NUL byte,
  distinct string literals differing only past a NUL no longer fuse
  into one constant, and `===`/`Object.is`/relational
  comparisons/`Map`/`Set` see the full code-unit sequence.
- Namespace objects (`import * as ns` / `export * as ns`) carry the
  correct object tag; runtime property reads on module namespace
  objects resolve through the export accessors.
- `String.prototype.replace` no longer drops the tail of the string
  when the match ends at the second-to-last character, and `` $` ``
  (before-match) substitutions are supported.
- `Array.prototype.every` applies ToBoolean to the callback result:
  falsy non-boolean results (null, 0, "") now fail the predicate.

## [0.2.0] - 2026-07-30

### Added

- The EIR compilation pipeline: an SSA IR between the AST and LLVM,
  with a clang-style pass configuration (`-O0`..`-O3` suites,
  `-f`/`-fno-` per-pass flags, `--print-passes`).
- Type-feedback optimization: shape tracking, guarded fast paths,
  born-with-shape allocation, typed slots, allocation sinking,
  devirtualization, and an export-boundary specialization wrapper.
- A generational, mostly-copying garbage collector with compaction,
  precise young-generation roots from compiler-emitted gc-frames, and
  `EJS_GC_*` debugging knobs.
- Relocatable per-platform dist tarballs (macOS arm64, Linux
  arm64/x86_64) with a bundled prefix installer, a Homebrew formula
  generator, and an npm wrapper package (`@pirouette/echojs`).
- An LLVM toolchain policy: the driver discovers a matching-major
  `opt`/`llc` (env `LLVM_BINDIR` override → build-baked path →
  conventional locations → PATH) and refuses to run against a
  different major.
- A value-based test harness whose baselines are independent of the
  node version, and a fully self-hosted bootstrap proven by a
  four-stage CI matrix on all three platforms.

### Changed

- The compiler sources are TypeScript throughout; babel is gone from
  the toolchain (one `tsc` pass converts modules for the build).

### Fixed

- Too many runtime-correctness fixes to enumerate here (typeof null,
  -0 semantics, Math.round ties, string-to-number edge cases, sparse
  arrays, generator exception propagation, error prototype chains,
  DataView indexing, and more) — see docs/runtime-p1-results.md and
  docs/runtime-p3-results.md.
