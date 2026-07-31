# runtime-P4 results — collector structural refactor (plans P6.3)

Phase record for runtime-plan.md's runtime-P4: the cell-lifecycle
consolidation, explicit mark epochs, the root registry, the single
collection-policy function, and the ejs-gc.c file split.  Landed
2026-07-29 on `eir`.  Behavior-preserving by design; the differential
lanes gate.

## What landed

Three commits, each independently green:

1. **Cell lifecycle in one block; explicit mark epochs.**  The
   scattered `SET_*`/`IS_*` color macros and the mutable
   white/black mask pair became one section of inline functions
   (`cell_is_free/gray/white/black`, `cell_set_*`) with the bitmap
   encoding private to it.  White/black are now EPOCH-RELATIVE: the
   color bits hold GRAY or the parity of the mark epoch the cell was
   last colored in, and `mark_epoch_advance()` — one call site, the
   end of a full collection — ages every surviving black cell white in
   O(1).  The mask swap was the same aging as two coupled globals
   mutated in place; the epoch is that flip made explicit and
   single-owner.  The dead `CONCURRENT` CAS macro variants went with
   it.

2. **Root registry; one collection-policy function.**  The root set
   is a growable array (O(1) add, swap-with-last remove) with ONE
   iteration helper — full-GC mark, minor evacuation, compaction
   fixup, the debug walks, and the shutdown NULL-out all go through
   `root_registry_foreach`/`root_registry_shutdown` instead of five
   hand-rolled walks of a malloc'd linked list.  Every collection the
   runtime initiates for itself is decided in `gc_policy(event)`:
   the growth trigger on the old-allocation path, the post-minor
   promotion check, the EVERY_N_ALLOC stress cadences (minor in
   nursery mode, full in old mode), and the forced allocation-failure
   collections.  Each event preserves its historical baseline/counter
   resets exactly, so collection schedules are unchanged.

3. **File split.**  ejs-gc.c (~3.7k lines) became six files plus the
   internal contract header `ejs-gc-internal.h` (module map lives
   there):

   | file | contents |
   |---|---|
   | ejs-gc.c | lifecycle API, allocator entry, cell free path, root registry, collection policy, GC JS object |
   | ejs-gc-heap.c | arena reservation, arenas/pages, LOS + sorted-range lookup, find_page_and_cell |
   | ejs-gc-mark.c | worklist, precise + conservative scanners, gc-frame skip, generator stack bookkeeping |
   | ejs-gc-minor.c | the nursery and the mostly-copying minor |
   | ejs-gc-major.c | full collections: mark/sweep orchestration, major compaction, the epoch advance |
   | ejs-gc-debug.c | EJS_GC_PROFILE / WATCH / VERIFY / PARANOID |

   The split was verified mechanically: every function body extracted
   from the old file and diffed against the new tree — 98/98 identical
   modulo `static` (the two exceptions: `_ejs_gc_collect_inner`
   gained the `root_registry_shutdown()` call; dead-code
   `page_list_count` was dropped).  The duplicate tentative
   definition of `heap_size_at_last_gc` collapsed to one.

## The two bugs the split surfaced (both pre-existing)

The TU split shifts codegen — frame layouts, spill slots — and the
stress lanes promptly caught two hazards that ACCIDENTAL conservative
pins of stale stack copies had been masking.  Both fixed; both are
the same lesson as gc-P4's bistable pin-scan: anything that depends
on C-stack luck is a latent bug.

- **Orphaned old slot storage** (ejs-object.c).  When
  `shaped_ensure_capacity` grows a shaped object's out-of-line slot
  array (or `_ejs_object_to_dictionary` drops it), an OLD-gen env
  cell is disconnected while still holding its pre-copy slot values.
  It is garbage until the next full sweep — but the old-gen WALKERS
  (the minor's remset-overflow fallback, EJS_GC_VERIFY,
  EJS_GC_PARANOID) cannot tell garbage from live and visit those
  stale slots after the young referents move or die; the overflow
  fallback could even "evacuate" a poisoned cell.  Observed
  concretely: the promoted env of the still-young rooted Reflect
  object, orphaned by capacity growth during `_ejs_init`, whose slot
  7 aborted EJS_GC_VERIFY once the split removed the rescuing pin.
  Fix: `shaped_retire_slots` queues the retiree for one precise scan
  (remset entry) at retirement — the next minor rewrites its young
  refs while they are still live, after which the cell is inert until
  swept.

- **Paranoid checker self-scan** (ejs-gc-debug.c).  The
  dying-young-referrer report's raw C-stack sweep scanned from its
  own frame to stack bottom, which includes the COLLECTOR's frames —
  written after the conservative pin scan ran.  The sweep loop's own
  cell cursor spilled into the probed range and reported the dying
  cell as "still referenced."  Fix: the sweep floors at the minor's
  entry frame (`paranoid_stack_floor`, set in
  `_ejs_gc_minor_collect`), so only frames the conservative scan
  could have seen at pin time are probed.

## Notes

- The runtime-plan entry also listed "aligned LOS regions with
  O(log n) lookup."  The lookup half landed in gc-P4 (the sorted
  range array + the arena direct map); with the 256-byte size class
  (gc-P5) routing every cap-14 shape and >14-slot env to pages, the
  LOS population is small and cold, so the alignment half is dropped
  as moot.  Raising the shaped field cap past 14 is shapes-plan
  business (a behavior change), not this refactor's.
- The gc-P5 note about `old_alloc_cell_for_promotion` walking the
  free-page list (61 profile samples) stands as a recorded perf item;
  it was left alone here to keep the phase strictly
  behavior-preserving.

## Gates

- Matrix: test-eir-lowtier + stage0–3 (including the stage2/stage3
  byte-identity fixed point) + stage1-shapes-off green.
- **test-eir was found RED at phase entry** — 11 failing EIR unit
  tests, all pre-existing compiler-side test debt from gc-P5 part 2's
  flag-off born-shaped literals: stale `make_object keys=[...]`
  expectations, a stale "flag-off keeps make_object" test, and the
  sinking/sink-flow fold tests, which fail knob-independently (the
  flow-sensitive sinking does not drain `make_object_shaped`, and
  `assertNotContains("make_object")` substring-matches the shaped op
  besides).  Proven pre-existing: this phase touches no lib/ file
  (`git diff` empty against the base commit for lib/), and the
  failures reproduce from the base commit's sources alone.  Recorded
  in compiler-plan territory as follow-up; NOT masked by editing the
  tests here.
- The gc stress lane (the gc tests × EVERY_N_ALLOC 7/31/101 ×
  PARANOID / VERIFY / NURSERY=off / COMPACT=off) matches the
  phase-entry baseline failure set exactly — the pre-existing pinned
  generator-stress bugs (runtime-P1's burn-down list) and nothing
  else.  Verified identical at every intermediate commit.
