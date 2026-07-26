# gc-P4 results — mostly-copying major compaction + the pin-scan cliff fix

Phase P6.1 (plans.md) / gc-P4 (gc-plan.md).  Three deliverables, one
commit: the conservative pin-scan cliff fix (the plan's "first order of
business"), the mostly-copying major compaction, and the auto-tuned
growth target.

## 1. The pin-scan cliff fix (arena reservation + O(1) lookup)

The mechanism (evidence recorded in gc-plan.md §gc-P4 while gating
sinking-P3): each arena was its own mmap, so once a late arena landed
beyond the C/LLVM heap the conservative prefilter span
`[conservative_lo, conservative_hi)` swallowed the malloc heap, and
every stack word pointing into LLVM's own allocations passed the
prefilter into a per-word arena bsearch (and formerly a locked linear
LOS walk).  Self-compile wall time was bistable per run (6s-vs-60s,
mmap-layout luck) and quasi-deterministic per binary.

The fix, in `runtime/ejs-gc.c`:

- **One arena reservation at init**: `MAX_HEAP_SIZE` (2GB) of address
  space, `ARENA_SIZE`-aligned, mapped `PROT_NONE` and committed one
  32MB arena at a time (`arena_space_reserve` / `arena_new` via
  `mprotect`).  The arena span is fixed and disjoint from the C heap
  for the life of the process — nothing foreign can ever be mapped
  inside it — and the linux boxability hint (sub-2^47) is applied once
  at reservation time.
- **O(1) arena lookup**: `(ptr - arena_space) >> ARENA_SHIFT` into a
  direct map (`arena_lookup`) replaces the per-word bsearch.
  `heap_arenas[]` stays for iteration and is address-sorted by
  construction (sequential carving).
- **LOS sorted-range array**: `los_ranges` (binary search, grow/remove
  on alloc/free) replaces the locked linear walk of `los_list` for
  conservative candidates; the `[los_lo, los_hi)` stopgap bounds from
  sinking-P3 remain as the quick reject.
- Drive-by: `release_to_los` now unmaps the whole mapping (header +
  bitmap slop), not just `alloc_size` — the old code leaked the tail
  page of every freed large object.

## 2. Mostly-copying major compaction

After a full collection's sweep, `compact_old_gen` evacuates the live
UNPINNED cells of the sparsest pages of each size class into the free
space of denser pages, rewrites every reference through the gc-P1
forwarding records, and returns the emptied pages to their arenas.

- **Pinning**: the conservative mark helpers now set the PINNED header
  bit (bit 58) on every hit during a full collection — under
  `EJS_GC_PROFILE` this rides the existing `profile_note_pin` dedup.
  Every registered generator also pins (the registry is an intrusive
  list of raw pointers, and the generator's own address is baked into
  its `makecontext` args).  Pinned cells sweep in place; pins clear in
  the fixup walk.
- **Selection**: per size class, pages sorted live-count-ascending; the
  COMPLETE source set is chosen before any evacuation (a source must
  fit in the pool that excludes it and all prior sources).  The
  one-pass version had a real bug the frag benchmark caught: an early
  DESTINATION could later be selected as a source via its stale live
  count, evacuating more cells than the accounting reserved
  ("compaction ran out of destination space" abort).
- **Fixup surface**: root-set slots (includes every shape's rooted
  `name`), module Scan, gc-frame chains (no-ops today — frame-held
  referents are conservatively pinned — walked for future-proofing),
  remset entries (raw owner pointers), every live heap cell via
  `old_gen_walk` + young survivor pages (Scan slots, primstr raw
  children, self-interior pointers via `minor_fixup_evacuated` at copy
  time).  Sources skip via the FORWARDED header bit; freed afterwards
  with no finalizers (the objects live on).
- **Safety facts established** (why moving OLD objects is sound):
  property maps content-hash names; symbol hashcodes are cached
  in-object; WeakMap/WeakSet ride hidden properties on the key; Map/Set
  are linear SameValue lists; shapes transition tables key on shape
  indices + content hashes; the only raw-pointer webs into the heap are
  the generator registry (pinned) and rope/dependent string children
  (fixed up).
- `EJS_GC_COMPACT=off` restores plain mark-sweep for A/B; the young
  survivor-page-emptied-by-full-sweep path got a latent list-corruption
  fix on the way (`young_page_freed`: the page lives on
  `heap_priv.young_pages`, but `_ejs_finalize_obj` detached it from the
  `heap_pages` bucket list, silently unlinking neighbors and leaving a
  stale young-list head).

### Shrink gate (frag benchmark, `GC.heapSize()` added for the gate)

400k 3-slot objects, keep every 16th, clobber the stack, collect twice:

| | heap after collect |
|---|---|
| compact **on** | **8.28 MB** (moved 38,177 objs, freed 7,159 pages) |
| compact off | 37.61 MB |

4.5× shrink, identical checksums, second collect moves 0 (idempotent).
Nursery-off variant: 2.44 MB vs 38.44 MB.

Test-writing lesson (cost an hour): garbage "dropped" at module
toplevel is conservatively retained by stale stack slots of the
toplevel frame — 7 pins held 800k objects transitively.  Allocate in a
callee and clobber the stack before measuring.

### Stress gates

- `//:test-eir` + `//:test-stage1` green with compaction default-on.
- gc-genstress1 / generator23-25 / frag under `EJS_GC_NURSERY=off
  EJS_GC_EVERY_N_ALLOC=997` (a compacting full GC every 997 allocs):
  byte-identical output compact-on vs compact-off.
- Full self-compile under `EJS_GC_NURSERY=off` (compaction exercised on
  every trigger for the entire compile): completes, produced compiler
  runs.

## 3. Auto-tuned growth target (knob census = 1)

`full_gc_trigger()` replaces the duplicated `60MB` constant at both
trigger sites: a full collection fires when old-gen growth since the
last one exceeds `EJS_GC_GROWTH` percent (default 50) of the post-sweep
footprint, floored at two arenas (64MB — the old constant's cadence for
small heaps).  With compaction shrinking the footprint, the trigger now
adapts in BOTH directions.  `EJS_GC_GROWTH` is the census's one knob.

## Timing (self-compile A/B, arm64, same tree, same probe)

- **Baseline (HEAD runtime, per-arena mmaps)**: this binary sat in the
  cliff's DEEP slow mode — it never completed one self-compile inside a
  10-minute timeout (attempt 1), and attempt 2's first run took ≈10
  minutes (inferred from process start times; the kill ate the buffered
  probe output).  `sample` during the run: ~95% of stacks inside
  `_ejs_gc_minor_collect → mark_ejsvals_in_range → find_page_and_cell`.
  This is the sinking-P3-era evidence reproduced at full strength — the
  slow mode is a property of the BINARY's allocation layout, and this
  binary drew the short straw.
- **Fixed (arena reservation + direct map + LOS bsearch)**: 62.6s /
  63.4s / 64.6s / 64.7s across 4 runs — the bistability is gone.  A
  profiled run: 56s wall, 106M allocs/4.65GB, GC total ≈ 5.5s (~10% of
  wall: 4.45s across 5,723 minors, max minor pause 10.9ms — down from
  the 500-860ms cliff pauses; 1.06s across 3 fulls).  Compaction on the
  real workload: the three full GCs freed 759 / 5,375 / 5,666 pages
  (~2.9 / 21 / 22 MB returned per collection).

So the fix is worth ~10× on unlucky binaries and removes the layout
lottery entirely; the residual GC share of a healthy self-compile is
~10%, of which pin scans are no longer the dominant term.

## Follow-ups / deferred

- Full-GC remset rooting retains dead dirty owners (`mark_object_root`
  on every remset entry) and the post-sweep rebuild keeps them — a
  self-sustaining garbage-retention cycle observed at 65536-entry
  overflow in the frag test's first draft.  Scanning dirty owners'
  young edges without marking the owner live (or filtering dead owners
  first) would fix it; not this phase's scope.
- Arena decommit: emptied arenas stay committed (page-level reuse only);
  `mprotect(PROT_NONE)`/`madvise` on fully-free arenas is a cheap
  follow-up now that the reservation exists.
- LOS is never compacted (by design) and `calc_heap_size` still counts
  only page bytes, not LOS.
