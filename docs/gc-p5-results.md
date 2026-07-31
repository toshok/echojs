# gc-P5 results: shapes intersection — single-cell shaped objects

Completed 2026-07-28.  The Step B design addendum lives in
gc-plan.md (the gc-P5 bullet); this doc records what shipped and the
gate numbers.

## What shipped

- **Single-cell shaped objects (embedded slots).**  Born-with-shape
  allocation places the slot storage inside the object's own cell:
  32B object + 16B embedded closureenv header + slots, one GC cell.
  The governing design choice: `obj->slots` remains a closureenv-boxed
  ejsval that merely points at `obj+32`, so the compiled slot
  addressing seam (`slotRef`), has_shape guards, the verifier
  contract, and the C storage engine's accessors are all UNTOUCHED —
  embedded-ness is pointer identity (`env == obj+32`), no new header
  bit, and growth past birth capacity silently degrades to the old
  out-of-line array.  Entry points:
  - `_ejs_object_new_shaped` derives the true shape FIRST (pure,
    transition-memo'd, ~one compare per field), then births object +
    storage as one cell; anything off-script falls back byte-for-byte.
  - Constructor results: a **birth-capacity hint on EJSFunction**
    (one-shot feedback — the first construct's field count sizes every
    later `this`).  Ordinary `Construct` allocates `this` with
    embedded capacity = hint.  Works flag-off; zero compiler plumbing.
- **Barrier owner flip.**  Shaped-slot stores now remember the wrapper
  OBJECT (all five C sites + emitted `slot_store`); the ordinary Scan
  walks slot values directly in both storage modes and scans the env
  edge only when out-of-line.  Remset owners are therefore always cell
  heads — the interior-pointer entry class never exists.  Scan order
  (values, then edge) is load-bearing: a dirty rescan must rewrite
  value slots before a young out-of-line env is evacuated.
- **Evacuation.**  Whole-cell memcpy (the existing routine); the
  embedded slots ejsval joins `minor_fixup_evacuated`'s
  self-interior-pointer cases (flat strings, EJSArguments) and is
  never presented to the precise slot callbacks (they assume
  object-base payloads).
- **Per-shape trace masks (typed-slot trace elision).**  `EJSShape`
  grows `f64_mask`, built incrementally at intern time (parent mask |
  edge bit).  The shaped Scan skips f64 slots — precise trace elision
  for raw doubles — on every collector walk (mark, minor, compaction
  fixup, paranoid/verify) since they all route through the specop.
  Barrier elision for typed stores was already true and is now
  documented: emitted f64 `slot_store` skips the barrier
  (emit.ts), and the runtime filter exits on non-traceable values.
- **Born-shaped literals go flag-off (lower.ts).**  The
  `make_object_shaped` literal lowering drops its oracle gate: key
  order/count are the site's static truth; without the oracle the
  static reprs are all-boxed and the runtime's birth derivation
  supplies true ones.  Flag-off literals now allocate single-cell and
  are eligible for the shaped-literal sinking.  Flag-off semantics are
  identical by construction (`make_object` was `object_create` +
  per-key setprop — exactly `new_shaped`'s screens and fallback).
- **The 256-byte size class is enabled.**  `ffs(256)=9 >
  HIGH_LIMIT_BITS` had routed 256B cells to the LOS since the
  beginning — the nursery seam, bump arrays, and emitter mapping were
  already built for 5 classes.  With gc-P4's LOS bsearch + direct
  arena map in, the class is on: `HEAP_PAGELISTS_COUNT` +1, three ffs
  threshold comparisons +1, emitter inline-env cap 128→256.  Every
  cap-14 shape now fits a single cell (`EJS_SHAPE_EMBED_FIELD_MAX =
  EJS_SHAPE_FIELD_CAP_MAX`), and 15..30-slot envs take pages, not the
  LOS.

## Numbers (arm64 M-series, medians of 3)

| workload | before (d48cf69) | after |
|---|---|---|
| types-bench2, flag-off | 2.47–2.53 s | 2.37 s |
| types-bench2, --types | 0.21 s | 0.21 s |
| litbench1 (escaping-literal loop, flag-off) | 1.56 s | **0.85 s (1.84×)** |
| self-compile (stage2 action wall) | 62–64 s recorded (gc-P4) | 60–67 s across runs — parity |

Allocation shape, types-bench2 flag-off: object+env cells **8.0M →
4.0M** (the 4M separate slot arrays are gone), requested bytes 305 →
244 MB, closureenv count 4,000,061 → 42.

Allocation shape, compiling lib/desugar.js (the compiler compiling a
real module, flag-off): closureenv 5.47M → 5.22M; **LOS allocations
85,304 → 26,251 (−69%)** with 83,549 now in 256-byte page cells.
Shape-table transitions are UNCHANGED (~11.3M) — the born-shaped
derivation still walks one memo edge per field; what changed is cells,
bytes, and the per-add call path.

**The headline correction this phase records**: shapes-P5's
"types-bench2 residual 1.71s = the allocation loop, gc-P5's half" is
obsolete.  The ctor-sinking phases (sinking-P2/P3) virtualized both
bench2 construct sites (`ctorSunk=2`), and --types bench2 is now
0.21 s on the phase-entry baseline already.  Profiling shows the old
"alloc loop" time was predominantly guard-miss generic property
traffic plus pre-sink allocation — gc-P5's real payoff is flag-off
code, literal-allocating loops, heap footprint, and LOS pressure.

## Measured and deferred (the shapes-P6 discipline)

- **Emitted bump allocation for `make_object_shaped`**: sampling the
  1.84×-improved litbench puts `_ejs_object_new_shaped` + `gc_alloc`
  at ~6% of in-process samples; generic property reads (strict_eq,
  getprop) dominate the flag-off residual.  The emitted-inline variant
  is deferred on that evidence.  Design note for whoever picks it up:
  inline stamping of the STATIC shape diverges from the runtime's
  true-repr derivation (flag-off claims are all-boxed; a number stored
  later would repr-flip the shape per object) — either derive
  number-ness inline per boxed-claimed field or revisit
  `classify_repr`'s number→f64 policy first.
- **Emitted inline construct-result allocation**: unnecessary — the
  EJSFunction hint gets constructor results single-cell with no
  compiler involvement, and the epoch-guarded ctor sink already
  deletes the allocation entirely where it matters under --types.

## Gates

- Matrix ×7 green at every step (test-eir, lowtier, stage0–3 including
  the stage2/stage3 byte-identity fixed point, stage1-shapes-off).
- Embedded-slot stress probe (growth past capacity, ctor hints with
  under-sized hints, dictionary migration out of embedded storage,
  repr flips, old→young stores through existing slots, enumeration
  order, `in`): node-identical under
  EJS_GC_EVERY_N_ALLOC=7/31/101, EJS_GC_PARANOID=1,
  EJS_GC_NURSERY=off, EJS_SHAPES=off, EJS_GC_COMPACT=off.
- types-typedslots1 / types-bornshape1 / types-poly1 (--types builds)
  green under the same stress envs (poly1 A/B'd bit-identical against
  the phase-entry baseline binary).
- --types diff lane at phase close: **475 files, 474 identical, 0
  divergent, 1 N/A** (tester.js, the standing esprima parse gap) —
  LANE PASS.

## Notes for later phases

- The old collector's promotion allocator
  (`old_alloc_cell_for_promotion`) showed 61 samples walking its
  free-page list in the compile profile — a P6.3-refactor-adjacent
  perf item.
- Self-compile in-process time is dominated by `_ejs_op_strict_eq`
  (196 samples — property-name compares in generic get paths and Map
  lookups) and string flatten/compare churn, not allocation: the next
  self-compile win lives in flag-off property access (compiler-P1
  lattice territory), not the collector.
- The pre-existing gc-P4 note about remset-rooted dead dirty owners
  self-sustaining across full GCs applies unchanged to the new
  object-owner entries.
