# echojs — the program of work

The single ordering document.  Milestones are `P#`, their phases
`P#.#`; each phase *references* a bucket plan's phase (`gc-P2`,
`shapes-P4`, ...) where the design, gates, and results live.  Bucket
plans: `compiler-plan.md`, `maam-plan.md`, `shapes-plan.md`,
`gc-plan.md`, `sinking-plan.md`, `runtime-plan.md`, `language-plan.md`,
`release-plan.md`.  Results docs (`*-results.md`) record gate numbers
per landed phase.

Conventions: a milestone is done when every phase is; phases within a
milestone are ordered; milestones are ordered but adjacent future
milestones can interleave when their buckets don't touch.  Bucket
phase ids are stable — commit messages and results docs written before
2026-07-25 use the pre-rename ids (maam's bare P0..P4, shapes' P4.1..
P4.6, sinking's S1..S3, gc's bare P0..P7); each bucket doc carries the
mapping.

## P1 — The EIR pipeline  [x]

One SSA middle-end, no legacy path.  Detail: compiler-plan.md
(history section) and `EIRProposal.md`.

- [x] **P1.1** close the per-function lowering gaps (424/424, zero
      fallbacks).
- [x] **P1.2** desugars run pre-EIR (classes, destructuring,
      generators, spread, hoisting).
- [x] **P1.3** toplevel-as-EIR: whole modules lower as one unit.
- [x] **P1.4** flip the default, delete the legacy middle-end (~9k
      lines); stage2/stage3 byte-identity under EIR self-compiles.

## P2 — Typed arithmetic: the maam oracle  [x]

An abstract-interpretation type oracle feeding guarded unboxed
arithmetic.  Detail: maam-plan.md; numbers in maam-p0-results.md.

- [x] **P2.1** oracle adapter + dialect shims (maam-P0).
- [x] **P2.2** ⊤-degradation + node-identity queries (maam-P1).
- [x] **P2.3** low-tier ops: has_tag/unbox/box/f64_* (maam-P2).
- [x] **P2.4** guarded arithmetic diamonds (maam-P3).
- [x] **P2.5** trust-free guard folding, region merging, raw f64
      joins (maam-P3.4).
- [x] **P2.6** differential harness: concreteEval vs node vs ejs
      (maam-P3.5).
- [x] **P2.7** typed calling convention / function specialization
      (maam-P3.6).  ~46× on the phase bench.

## P3 — Shapes  [x]

Type-aware hidden classes, slot storage, guarded property fast paths.
Detail: shapes-plan.md (designed as maam-P4).

- [x] **P3.1** runtime shape tracking, dual bookkeeping (shapes-P1).
- [x] **P3.2** slot storage + dictionary migration (shapes-P2).
- [x] **P3.3** shape-guarded fast paths under --types (shapes-P3).
- [x] **P3.4** born with their shape (shapes-P4).
- [x] **P3.5** typed slots + shape/numeric region fusion (shapes-P5).
- [x] **P3.6** measured extensions: 2-way polymorphic guard chains;
      accessor inlining/pretenuring/array-shapes declined on evidence
      (shapes-P6).

## P4 — Mover foundations  [x]

The generational moving collector, through precise JS roots.  Detail:
gc-plan.md; numbers in gc-p0/p2/p3-results.md.

- [x] **P4.1** measurement + generator-scan fixes + runtime -O2
      (gc-P0).  Verdict that shaped this milestone: pins are tiny, so
      the nursery ships on conservative roots.
- [x] **P4.2** 64-bit header + forwarding plumbing (gc-P1).
- [x] **P4.3** nursery + object-remembering barrier + evacuating
      minor + emitted inline env allocation, default ON (gc-P2).
- [x] **P4.4** emitter gc-frames: precise relocatable JS roots + env
      slot-address inlining + move-everything stress (gc-P3).

## P5 — Allocation elimination  [x]

Delete the allocations the mover made cheap.  Detail: sinking-plan.md.

- [x] **P5.1** shaped-literal sinking + own-key folding
      (sinking-P1).
- [x] **P5.2** epoch-guarded constructor-result sinking — the
      types-bench2 alloc loop (sinking-P2).
- [x] **P5.3** flow-sensitive field writes, partial escapes,
      rest_args/args_obj (sinking-P3).
- [x] **P5.4** optimizer residue: SSA cleanups, type lattice,
      slot-load CSE for toplevel receivers (compiler-P1).  DONE
      2026-07-28 — docs/compiler-p1-results.md.

## P6 — Compacting, shape-fused GC

The heap shrinks; the collector consumes the object model.  Detail:
gc-plan.md, shapes-plan.md (Step B).

- [x] **P6.1** mostly-copying major compaction + auto-tuned growth
      target (gc-P4).  DONE 2026-07-26 — docs/gc-p4-results.md.
- [x] **P6.2** shapes intersection: per-shape trace bitmaps, inline
      slots, object-literal inline allocation, typed-slot barrier
      elision (gc-P5; consumes shapes-plan's deferred Step B).  DONE
      2026-07-28 — docs/gc-p5-results.md.
- [x] **P6.3** collector structural refactor: cell-lifecycle module,
      LOS lookup, file split (runtime-P4; can land any time after
      P6.1, behavior-preserving).  DONE 2026-07-29 —
      docs/runtime-p4-results.md.

## P7 — Robustness

The correctness and ergonomics debts, paid down.  Detail:
runtime-plan.md, compiler-plan.md.

- [x] **P7.1** pinned runtime-bug burn-down (runtime-P1).  DONE
      2026-07-29 — docs/runtime-p1-results.md.
- [x] **P7.2** export-boundary wrapper: specialization across escaping
      entry points (runtime-P2).  DONE 2026-07-29 —
      docs/runtime-p2-results.md (wrapper dispatches to an UNTRUSTED
      guarded clone — maam's constant-domain claims can't cross the
      boundary — plus the escape-taint fence, closing a pre-existing
      cross-module trusted-rewrite miscompile).
- [x] **P7.3** value-based test harness, un-pinning node's inspect
      format (runtime-P3).  DONE 2026-07-29 —
      docs/runtime-p3-results.md (harness-owned serializer on both
      sides; baselines byte-identical from node 22.4.0 and 22.23.2, CI
      floats on 22.x; the un-masking flushed 3 runtime bugs fixed +
      3 pinned, plus a tester scheduler bug that had silently skipped
      weakmap2 forever).
- [x] **P7.4** finish the TypeScript port of the compiler; babel step
      becomes tsc (compiler-P2).  DONE 2026-07-29 —
      docs/compiler-p2-results.md (//lib:generated converts modules
      with one tsc --allowJs pass; tester.ts and gen-atoms.ts ported;
      `generator: esm` baselines byte-identical vs babel-node; babel
      removed from the repo).
- [x] **P7.5** clang-style pass configuration: -O suites define the
      optimizer tiers, -f/-fno- per-pass flags replace the EJS_* env
      vars, which revert to debugging-only (compiler-P5; independent,
      can land any time).  DONE 2026-07-29 —
      docs/compiler-p5-results.md (pass registry in lib/pass-config.ts;
      -O2 byte-identical to the pre-P5 default; env spellings deleted
      after a 35-pair env≡flag A/B; EJS_FLAGS is the one env escape).

## P8 — Language modernization

Catch up with the language; adopt test262.  Detail: language-plan.md.

- [x] **P8.1** gap inventory + test262 subset probe (language-P1).
      DONE 2026-07-31 — docs/language-p1-results.md (26,820-test
      probe, runner in test/test262/; 35% pass, parser confirmed as
      the long pole, prioritized language-P3 feature list).
- [x] **P8.2** parser replacement behind the ESTree seam
      (language-P2; coordinates with compiler-P3 if TS input
      happens).  DONE 2026-07-31 — docs/language-p2-results.md
      (acorn 8.18.0 behind lib/parser.ts, chosen over @babel/parser by
      a self-host probe; unsupported-syntax gates replace silent
      miscompiles; five pre-existing runtime bugs fixed; matrix
      426/20/0, corpus AST-identity vs node).
- [x] **P8.3** features in payoff order (language-P3).  DONE 2026-07-31
      — docs/language-p3-results.md (the payoff list landed: `**`/`**=`,
      `??`, logical assignment, optional chaining, object spread/rest,
      bare catch, class fields + private members + static blocks,
      async/await + `for await`; eight pre-existing bugs fixed incl.
      super.other() mis-dispatch and Promise.all-never-resolves; still
      gated: async generators, BigInt, dynamic import; matrix 438/20/0
      ×5 lanes).
- [x] **P8.4** test262 CI lane (language-P4).  DONE 2026-07-31 —
      docs/language-p4-results.md (`test/test262/lane.sh`: curated
      selection vs pinned suite SHA, checked-in expectations file as a
      conformance ratchet; runs in the macOS bootstrap job).  First
      ratchet turn same day (language-P4.1,
      docs/language-p4.1-results.md): async generators landed, `yield*`
      value-position/forwarding fixed, function `.length`/`.name`,
      for-in symbol-key crash, globalThis; class/elements and
      for-await-of both 71%/31% → 90%.
- [ ] **P8.5** un-fork the JS external-deps (language-P5).

## P9 — Distribution

From repo to product.  Detail: release-plan.md, compiler-plan.md.

- [x] **P9.1** relocatable dist artifact + LLVM toolchain policy
      (release-P1).  DONE 2026-07-30 — docs/release-p1-results.md
      (`//:dist` tarball of the installed layout, `//:test-dist` smoke
      test, CI uploads per-platform artifacts; the driver discovers a
      matching-major opt/llc and fails loudly otherwise, LLVM_MAJOR
      baked into host-config).
- [x] **P9.2** platform packages: homebrew, linux, npm wrapper
      (release-P2).  DONE 2026-07-30 — docs/release-p2-results.md
      (packaging/: prefix installer shipped in the tarball +
      `//:test-dist` step, homebrew formula generator + libexec/exec-
      shim layout, npm wrapper with EJS_NPM_TARBALL override; CI
      smokes all three; hosted URLs await release-P3).
- [x] **P9.3** versioning + release automation off the bootstrap
      matrix (release-P3).  DONE 2026-07-30 —
      docs/release-p3-results.md (CHANGELOG discipline +
      prepare-release.sh stamping/tagging; ci matrix refactored into
      reusable bootstrap.yml; release.yml on v-tags: version-check →
      same matrix → draft release with tarballs/formula/npm tgz →
      clean-machine container+runner smokes; npm wrapper is
      @pirouette/echojs, the bare name was taken).
- [x] **P9.4** getting-started surface (release-P4).  DONE 2026-07-31
      — docs/release-p4-results.md (README rewritten around the
      released package: npm/tarball install, verified quickstart,
      honest language-subset status from language-plan's census, the
      real flag/env surface; examples proven against a dist tarball).
- [ ] **P9.5** reusable native modules + IR-in-manifest cross-module
      linking (compiler-P4).

## P10 — Concurrent GC

Pause bounds independent of live-set size.  Detail: gc-plan.md.

- [ ] **P10.1** collector thread: concurrent mark (SATB) + STW
      survivor evacuation (gc-P6).
- [ ] **P10.2** fully concurrent evacuation — only on P10.1's pause
      evidence (gc-P7).

## P11 — Self-hosted type oracle

`--types` in the shipped compiler.  Ordered after P8 (the language
milestone makes maam's ES2022 output compile as-is — decided
2026-07-31, see maam-plan.md's self-hosting strategy addendum);
interleaves freely with P9.5/P10.  Detail: maam-plan.md.

- [ ] **P11.1** compile maam into the bootstrap: ESM build flavor,
      static-import seam in the oracle, srcdir/BUCK wiring; gate =
      the --types differential lane run stage0-vs-stage1 (identical
      typed output), and the README caveat deleted (maam-P5).
      Consider pairing with the maam repo merge.
