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

## P5 — Allocation elimination  [~]

Delete the allocations the mover made cheap.  Detail: sinking-plan.md.

- [x] **P5.1** shaped-literal sinking + own-key folding
      (sinking-P1).
- [x] **P5.2** epoch-guarded constructor-result sinking — the
      types-bench2 alloc loop (sinking-P2).
- [x] **P5.3** flow-sensitive field writes, partial escapes,
      rest_args/args_obj (sinking-P3).
- [ ] **P5.4** optimizer residue: SSA cleanups, type lattice,
      slot-load CSE for toplevel receivers (compiler-P1).

## P6 — Compacting, shape-fused GC

The heap shrinks; the collector consumes the object model.  Detail:
gc-plan.md, shapes-plan.md (Step B).

- [x] **P6.1** mostly-copying major compaction + auto-tuned growth
      target (gc-P4).  DONE 2026-07-26 — docs/gc-p4-results.md.
- [ ] **P6.2** shapes intersection: per-shape trace bitmaps, inline
      slots, object-literal inline allocation, typed-slot barrier
      elision (gc-P5; consumes shapes-plan's deferred Step B).
- [ ] **P6.3** collector structural refactor: cell-lifecycle module,
      LOS lookup, file split (runtime-P4; can land any time after
      P6.1, behavior-preserving).

## P7 — Robustness

The correctness debts, paid down.  Detail: runtime-plan.md,
compiler-plan.md.

- [ ] **P7.1** pinned runtime-bug burn-down (runtime-P1).
- [ ] **P7.2** export-boundary wrapper: specialization across escaping
      entry points (runtime-P2).
- [ ] **P7.3** value-based test harness, un-pinning node's inspect
      format (runtime-P3).
- [ ] **P7.4** finish the TypeScript port of the compiler; babel step
      becomes tsc (compiler-P2).

## P8 — Language modernization

Catch up with the language; adopt test262.  Detail: language-plan.md.

- [ ] **P8.1** gap inventory + test262 subset probe (language-P1).
- [ ] **P8.2** parser replacement behind the ESTree seam
      (language-P2; coordinates with compiler-P3 if TS input
      happens).
- [ ] **P8.3** features in payoff order (language-P3).
- [ ] **P8.4** test262 CI lane (language-P4).
- [ ] **P8.5** un-fork the JS external-deps (language-P5).

## P9 — Distribution

From repo to product.  Detail: release-plan.md, compiler-plan.md.

- [ ] **P9.1** relocatable dist artifact + LLVM toolchain policy
      (release-P1).
- [ ] **P9.2** platform packages: homebrew, linux, npm wrapper
      (release-P2).
- [ ] **P9.3** versioning + release automation off the bootstrap
      matrix (release-P3).
- [ ] **P9.4** getting-started surface (release-P4).
- [ ] **P9.5** reusable native modules + IR-in-manifest cross-module
      linking (compiler-P4).

## P10 — Concurrent GC

Pause bounds independent of live-set size.  Detail: gc-plan.md.

- [ ] **P10.1** collector thread: concurrent mark (SATB) + STW
      survivor evacuation (gc-P6).
- [ ] **P10.2** fully concurrent evacuation — only on P10.1's pause
      evidence (gc-P7).
