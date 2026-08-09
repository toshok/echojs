# Generator lowering at the EIR level

Direction set 2026-08-07: the compiler once had a regenerator-style
AST desugar for generators and dropped it (state-machine soup at the
AST level is unmaintainable).  Bring the idea back **one level down**:
lower generator bodies to EIR state machines.  Yield points become
explicit resumption states, live values live in slots of a heap
activation record (the gc-P3 slot-demotion machinery, retargeted), and
the runtime's machine-stack generators — ucontexts, per-generator LOS
stacks, conservative scans, the sticky-pin cache — get deleted
outright.

## Why (the cost class, measured)

Suspended generators are the runtime's last conservative-scan
stronghold, and the self-hosted oracle is generator-shaped: maam's
tree walks hold thousands of suspended generators (3,698 live in the
ejs-es6 analysis window when this was first measured).  Every piece of
machinery below exists only to keep machine-stack generators alive,
and every one of them has already cost a debugging campaign:

- **LOS machine stacks.**  Each suspended generator parks a
  large-object stack.  A generator storm races the old-gen growth
  trigger; the LOS-blind trigger produced the collect-per-alloc hang
  (fixed by counting `los_size` into the trigger, but the pressure
  itself remains).
- **Conservative stack scans.**  Every minor rescans suspended
  stacks; the sticky-pin cache (@fa38971) amortizes the rescan but
  the replay loop + `walk_gc_frames` over thousands of frozen chains
  is still real per-minor work, and pins block evacuation.
- **Lifecycle hacks.**  Complete-time resource release (@085753a)
  exists because finalize-time release left dead stacks scanned for
  whole compile phases.  The nursery-starvation fallback (@6bf19aa)
  was triggered by generator pin storms starving the nursery.
- **Analysis blind spot.**  maam treats yield conservatively; the
  queued "yield as full-precision point" task falls out for free once
  yields are explicit states over a precise record.

Post-lever-1..3a profile of the entry-module oracle window
(2026-08-09, top-of-stack shares): property access 48.9%, Map/SVZ
14.6%, **GC 11.8%**, malloc 5.6%, cache-guard 4.6%.  The GC share is
no longer the top line — the property storm is — so this lever's value
is the *deletion of the whole cost class* (scan work inside that 11.8%,
allocation churn, LOS pressure, pin-blocked compaction) plus the
precision bonus, not a headline wall win on its own.  Sequencing
respects that: receiver-coverage work can overtake it if the property
storm gets a direct lever first.

## Shape of the lowering

- **Activation record = heap object.**  A precise-scanned cell:
  `{ state: i32, done, sent/thrown slot, slots[N] }` where N = the
  maximum live-across-yield set (liveness.ts already computes
  live-across-safepoint sets; yields become just another safepoint
  kind).  The generator object holds the record; no stack, no
  ucontext, nothing conservative.  Records are ordinary young objects
  — the storm allocates small cells instead of 64K+ LOS blocks.
- **Resume dispatch.**  The generator body compiles to one EIR
  function taking `(record, resume_mode, resume_value)`.  Entry
  dispatches on `record.state` — an integer compare chain over dense
  small-int states that LLVM folds to a jump table (the atom-switch
  lever's compare-chain discipline, minus the atom table).
- **Yield = store live set, set state, return.**  Slot demotion
  mirrors gc-frames: values live across a yield store to record slots
  at definition and reload at use (the val()-intercept pattern), so
  resume re-derives everything from the record.  `resume_mode`
  distinguishes next/throw/return; throw re-raises at the yield point
  so existing EIR handler blocks apply.
- **try/finally across yield** is the hard part: a resume must
  re-enter the body with the right handler context.  The lowering
  already duplicates finalizers per abrupt-exit site (runFinalizers);
  resume dispatch lands INSIDE the try region's blocks, so the
  handler stack must be re-established at dispatch — either
  per-state handler prologues or dispatch-through-nested-switches
  (regenerator's approach).  Decide in G2 with the test battery in
  hand, not before.
- **Runtime surface unchanged.**  The generator object keeps its
  species and iteration protocol; only its innards switch from
  (stack, context) to (record, body-fn).  `generator_new` for a
  lowered body allocates a record; resume calls the body function
  instead of swapping stacks.

## Phasing

- **G0 — net + representation.**  Inventory the generator surface
  (test/generator*.js, gc-gen*, async*, eir-generator1, yield*
  corners, test262 generator lanes) into a fast battery.  Pick the
  record layout and the dispatch representation; write the EIR-level
  design note.  No behavior change.
- **G1 — plain generators.**  Lower bodies with no try/finally
  crossing a yield (the overwhelmingly common shape — every maam
  tree-walk generator qualifies).  Unlowerable bodies fall back to
  the machine-stack path behind a flag (-fgen-eir / -fno-gen-eir for
  bisection).  Gates: battery, test-stage1, types-diff parity,
  stress, stage2 wall + oracle window re-profile.
- **G2 — abrupt completions.**  try/catch/finally across yield,
  throw()/return() at every state, yield* delegation.  Same gates.
- **G3 — async functions** on the same machinery (they are generators
  driven by the promise loop today; if the current async path already
  shares the generator runtime, this is mostly plumbing).
- **G4 — delete the old world.**  Remove ucontexts, generator machine
  stacks, the stack push/pop gc-frame-chain hooks, the sticky-pin
  cache, complete-time release, generator LOS accounting; simplify
  the minor's scan paths.  Full stress discipline
  (EJS_GC_EVERY_N_ALLOC=101, sanity build, gc-genstress) — this is
  the payoff commit and the riskiest one.
- **G5 — precision.**  Surface yield as a full-precision point to
  maam (subsumes the queued task); revisit C6 default-on and the
  oracle-speed milestone with the new cost structure.

## Baselines to re-measure against (2026-08-09)

stage2 exe self-compile 71.9s (post-3a; 67.6s pre-3a), exe oracle
pass1 sum 23.5s / entry 9.8s, oracle-window shares as above, stress
RSS ~1.0GB.  Queued interim levers superseded by this plan: survivor-
hole reuse (task 19) and yield-precision (task 20).
