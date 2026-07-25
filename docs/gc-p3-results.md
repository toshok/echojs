# gc-P3 results: precise JS-frame roots (emitter gc-frames)

Completed 2026-07-25.  The chained-frame variant of gc-plan P3: emitted
functions own their precise root records; frame-held values relocate.

## What shipped

- **The chain.**  `EJSHeapContext.gc_frame_head` (seam word 17; the
  emitted view widened `[12 x i64]` → `[18 x i64]`).  An emitted
  function whose values are live across a safepoint allocas an
  `EJSGCFrame { prev, count, slots[count] }`, links it in its prologue,
  unlinks at every return.  Catch handlers re-link their own frame (the
  unwind discarded every callee record).  Chains are **per machine
  stack**: the generator push/pop hooks swap the head exactly like
  `current_stack_end` (caller segment parks on the generator), and the
  minor walks the live chain, every suspended generator's saved chain,
  and every active generator's parked caller segment.
- **Slot demotion, not spill/reload.**  A value live across a safepoint
  is demoted: stored to its frame slot at its definition, and **loaded
  at every use** (`val()` intercepts).  Every load is dominated by the
  def's store, so there is no dominance hazard from rewriting SSA uses
  across branches; LLVM CSEs redundant loads between safepoints and
  cannot forward across one — the frame address escapes through the
  chain link, which is exactly the store-to-load-forwarding discipline
  the plan demanded.  Slots are undefined-initialized (a stale slot
  must parse as an ejsval).
- **Liveness** (`lib/eir/liveness.ts`): standard backward analysis over
  EIR; safepoints = target-less ops with GC|CALL effects (`box_f64`
  excluded — it never allocates).  Deliberately partial, and sound
  because of one load-bearing ABI fact: **a live-across-call SSA value
  is always in a callee-saved register or a stack slot, so the
  conservative scan sees it and pins its referent.**  Under-coverage
  costs pins, never correctness.  v1 skips invoke-form safepoints (try
  regions) and values defined by them — those stay pinned.
- **Env slot-address inlining.**  `env_load`/`env_store` compute the
  slot address inline (payload mask + `+16 + 8*slot`), recomputed per
  use from the boxed env value — a relocated env re-derives through its
  own slot load.  Deletes a runtime call from every env access.
  Bisects: `EJS_NO_GC_FRAMES`, `EJS_NO_INLINE_ENV_SLOTS`.
- **Pin-first ordering.**  The chain walk runs AFTER the conservative
  pin pass, on purpose: an object visible to both a gc-frame slot and a
  C frame (an ejsval argument into the very call that triggered the
  minor) must not move — `minor_process_slot` leaves pinned targets in
  place, so the pin wins and the C copy stays valid.  Precise-first
  would have been a use-after-move factory.

## The bug measurement caught

First profile: `gcframe_moves = 0` across 4256 minors, pins UP 6×.
The gc-frame is a stack alloca — **the conservative stack scan saw
every frame slot and pinned every frame-held value through its own
slot.**  Precision existed but could never move anything.

Fix: during a minor, `mark_ejsvals_in_range` skips the frame records of
the stack it is scanning (`set_frame_skip_chain` — a sorted range list
with a merge cursor, O(1) per word).  Each conservative range scan gets
its matching chain: the live head for the current stack, the saved head
for a suspended generator stack, the parked caller segment for each
active generator.  Full collections never skip — the old collector
still relies on conservative slot visibility (it doesn't move, so it
doesn't need to rewrite).

## Numbers (self-compile, arm64, nursery default-on, 1 MB budget)

- **Movement**: `gcframe_moves` = 76,174 relocations per self-compile
  (p50 13/minor, max 101) — the "move-everything" property runs
  continuously; under `EJS_GC_EVERY_N_ALLOC=101` stress every
  frame-held young value relocates constantly, which is the
  store-forwarding trap the gate demanded (green across the ladder).
- **Pins**: p50 373 → **101** per minor (mean 452 → 136) after the
  skip fix; ~45% fewer pin events per compile than the P2 baseline.
  Residual pins = C-frame-referenced values + stale dead spills — the
  set precision cannot touch, as predicted.
- **Spill cost** (variant exes, one self-compile each): P2 baseline
  42.3 s; env-inlining alone 41.9 s; frames alone 43.3 s; both 42.7 s.
  Net ≈ **+1% wall** — frames cost ~1 s, env inlining gives back
  ~0.4 s.  (The plan's "expected small: safepoints are call sites;
  calls spill anyway.")

## Validation

- tiny ×3 / gennest compile + runs / paranoid / verify, all under
  `EJS_GC_EVERY_N_ALLOC=101`; self-compile under stress-997 — green.
- probes (ropes, envwb, gens, gennest, genstress): 8/8 byte-identical
  across off / on / stress / stress+verify.
- nursery differential lane (full recompile — gc-frames are in ALL
  emitted code): 475 pass / 0 fail / 1 n-a.
- --types differential lane: 485 identical / 0 divergent / 1 n-a.
- matrix ×7 green.

## Deferred (recorded)

- Invoke-form safepoints (try regions) and their results stay
  conservatively pinned; covering them needs reload placement on the
  normal edge (single-pred case is easy; shared continuations need
  edge splitting).
- Slot liveness is per-value, not interval-packed; frame sizes are
  small in practice.
- Return-address-keyed stackmaps (the zero-entry-cost upgrade) remain
  the measured-later variant; chain maintenance cost is within noise.
- Dead-slot floating garbage: a slot keeps its last value alive until
  the frame pops (bounded by frame size; undefined-init bounds it at
  function entry).
