# gc-P2 results: generational nursery + emitted allocation/barrier seam

Completed 2026-07-25.  Nursery is ON by default; `EJS_GC_NURSERY=off`
selects the old collector (the A/B knob the differential lane uses).

## What shipped

- **P2a — slot-based Scan protocol.** `EJSValueFunc` takes `ejsval*`; every
  precise scan (roots, modules, remset, transitive) can rewrite slots.
  42 call sites converted; property maps walked directly
  (`scan_property_entries`).
- **P2b — nursery + evacuating minor GC.** One dedicated 32 MB arena
  (`is_young` = range check); size-class bump pages (young=1, allocated-ness
  = below-bump rule) and survivor pages (young=2, bitmap rule); conservative
  cell pinning (C stacks + registers + every live generator stack);
  evacuation via first-word forwarding (gc-P1 bits); promotion into old
  free-list pages; 1 MB default minor budget (`EJS_GC_NURSERY_BUDGET`).
- **Object-remembering write barrier** (second design; the slot-address
  remset was abandoned after dangling recorded slots in freed/realloc'd
  malloc storage proved unfixable by enumeration): `_ejs_gc_remember(owner,
  value)` — inline filter (traceable, value-young, owner-not-young,
  DIRTY-bit dedup) then owner append; minors re-Scan dirty owners against
  whatever storage they own *at scan time*; pinned-young referents re-dirty
  the owner (edge carry); LOS objects are born dirty; full GC prunes the
  buffer.
- **P2c — emitted seam.** `EJSHeapContext _ejs_heap` exported ([12 × i64]:
  bump[5], limit[5], nursery_base, nursery_end — append-only layout
  contract); emitted inline `make_env` allocation (bump/compare/init/box,
  slow call = safepoint; `EJS_NO_INLINE_ALLOC` bisect); emitted store
  barriers at env/slot stores (inline young-check reading the seam words,
  out-of-line `_ejs_gc_remember_val`); shaped-object stores remember the
  slot-array env (the storage owner), not the wrapper object.
- **Conservative-lookup bounds prefilter** (`conservative_lo/hi`, widened
  at `arena_new` and `alloc_from_los`, checked in the stack scanners and at
  the top of `find_page_and_cell`).  Not nursery-specific — it fixed two
  pathologies (below) and speeds the old collector's full marks as well.

## The bug that nearly killed the phase

The compiler self-compile under `EJS_GC_NURSERY=1 EJS_GC_EVERY_N_ALLOC=101`
crashed in module init with a 0xa7-poisoned receiver while *every* checker
(barrier-coverage verify, whole-heap paranoid walk, module-slot death
detector, sweep-time reverse-referrer lookup over old gen + LOS + roots +
modules + the raw C stack) stayed green.

Root cause: **unrooted `ejsval` C statics in the ejs-llvm native bindings**
— chiefly `_ejs_StructType_prototype` (never `_ejs_gc_add_root`ed).  The
prototype was live and correctly *evacuated* (reachable via
`ctor.prototype`; every scanned copy rewritten), but the C static kept the
stale nursery address, and `StructType_impl` births every subsequent
wrapper object with a dead proto.  The non-moving collector never noticed:
liveness was sufficient and addresses were eternal.

The mover lesson, stated once: **roots exist to rewrite locations, not just
to keep referents alive.**  Any C-side `ejsval` that outlives a collection
and is later read must be registered as a root (or re-derived from a
scanned location on every use).  Fix: all unrooted statics across the 17
ejs-llvm binding files rooted (prototype statics *and* constructor statics
— the latter had an init-time window between `_ejs_function_new` and the
exports `setprop` read).

Debug tooling built for the hunt (permanent, all in `runtime/ejs-gc.c`):

- reentrant-minor / young-alloc-during-minor / page-install-during-minor
  aborts; minor-end seam-desync check; sweeping-an-active-page check;
- `EJS_GC_PARANOID` sweep-time reverse-referrer lookup (names every
  location still referencing a dying young cell);
- `EJS_GC_WATCH=<hex addr>` cell-lifecycle tracer (alloc / pin / evacuate /
  sweep-poison, each with a C backtrace) — the tool that named the killer;
- per-phase minor timing (`phases[pins/roots/dirty/wl/sweep]`) and a
  full-GC phase line, both under `EJS_GC_PROFILE`.

## The two performance pathologies

First honest interleaved timing said nursery-ON self-compile was **2.4×
slower** (43.4 s → 103 s).  Phase profiling attributed 60.5 s of the 65 s
minor total to the conservative pin phase, and 13.1 s of a single full
collection (54 MB live!) to `process_worklist`.  One cause, two faces:

1. **Stack scan**: every C-stack word paid an arena bsearch and, on miss,
   a locked linear LOS-list walk.  Deep compiler recursion × a growing
   heap made minors cost 25→230 ms.  With the bounds prefilter in the
   scanners: pins 60.5 s → 0.3 s (p50 25 ms → 2 ms).
2. **Full-mark edges**: references to *static atoms* (which live outside
   every arena) fell through to the same locked LOS walk on every edge —
   and nursery mode had never culled the LOS list (no full GCs), so it was
   thousands of entries long: ~13 µs per marked object.  With the
   prefilter at the top of `find_page_and_cell`: worklist 13.07 s → 45 ms,
   the full collection 13.2 s → **59.6 ms**.  The old collector's full
   marks chase the same atom edges — it got faster too (self-compile 43.4 s
   → 39.3 s with the nursery *off*).

## Numbers (final build, interleaved ×3, arm64 M-series)

| workload | old collector | nursery (default 1 MB) |
|---|---|---|
| self-compile (full pipeline) | 38.7–40.0 s | 39.0–39.2 s (4 MB budget: 39.1 s) |
| types-bench2 --types | 0.69–0.70 s | 0.64 s |
| envbench1 | 1.81–1.96 s | 1.34–1.48 s |

GC work on the self-compile: old = 34 stop-the-world collections,
9.3 s total pause (worst ~1.3 s); nursery = ~1060 minors totaling ~4.6 s
(p50 2.1 ms at 4 MB budget) + one 60 ms full collection.

Minor pause distribution (envbench corpus) by budget:

| budget | minors | p50 | p99 | envbench wall | self-compile wall |
|---|---|---|---|---|---|
| 512 KB | 1221 | 0.48 ms | **0.68 ms** | 1.35 s | 43.4 s |
| 1 MB (default) | 611 | 0.93 ms | 1.27 ms | **1.34 s** | 40.4 s |
| 2 MB | 306 | 1.99 ms | 2.57 ms | 1.42 s | — |
| 4 MB | 153 | 4.08 ms | 5.27 ms | 1.48 s | **39.1 s** |

The <1 ms p99 gate is met at the 512 KB setting; the 1 MB default trades
p99 1.27 ms for ~7 % better self-compile throughput.  Self-compile minors
run heavier than the bench corpus (p99 ~54 ms at 4 MB budget — deep stacks
and promotion bursts).

Pin report (self-compile): conservative pins ~330 objects/minor (KBs);
full-GC census: cstack 167 objects / 8 KB, registers 0, generator stacks 0.
Pinning remains 4–5 orders of magnitude below the live set — the P0
conclusion stands.

## Validation

- probes (ropes1/2, envwb1, gens1small/2small, gennest, genstress1/2):
  8/8 byte-identical across off / on / stress-101 / stress-101+verify.
- nursery-diff lane: every suite test compiled once, run off vs on vs
  stress-997, byte-compared — 475 pass / 0 fail / 1 n-a (tester.js).
- self-compile: nursery, nursery+stress-997, nursery+stress-101 (tiny),
  paranoid and verify lanes on the gennest ladder — all green.
- matrix ×7 green (final build).

## Measurement caveats

- emitted inline allocations are invisible to `EJS_GC_PROFILE` alloc
  counters (they never enter `_ejs_gc_alloc`);
- heap addresses are only stable across runs under lldb (no ASLR) —
  `EJS_GC_WATCH` targets must come from the same-process run;
- differential-lane exes statically link the runtime: after any runtime
  change, `rm test/*.exe` or the lane silently tests the old collector code.
