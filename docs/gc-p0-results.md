# gc-plan Phase 0 — correctness fixes + the measurement numbers

2026-07-24, M-series macOS (arm64), buck2 + LLVM 22.1.8.  All EJS-compiled
user code runs the normal `-O2` opt pipeline (the optimizer-on rule); the
runtime's own optimization level is the experiment's variable (`-O0` as
found → `-O2`, see below).

## Correctness fixes (all latent-mover-blockers, all real today)

1. **Collection while executing on a generator stack segfaulted.**
   `mark_thread_stack` scanned `[&local, main-stack-bottom)`; on a
   generator's malloc'd stack that range spans from the malloc heap
   across unmapped memory.  Reproduced with `EJS_GC_EVERY_N_ALLOC=7` on
   a generator that allocates (signal 11 in `mark_ejsvals_in_range`,
   backtrace even showed `_ejs_create_iter_result` — the alloc-after-
   `pop_generator` completion path, bug 3 below).  Fixed: when the
   active-generator chain is non-empty the current-stack scan stops at
   the running generator's stack end.
2. **The suspended main-stack segment was never scanned** while a
   generator ran.  Fixed: each swap-in site records the caller's stack
   position (`gen->caller_stack_top`); `mark_generator_stacks` (the
   2015 "XXX mark the actual stack" stub) now roots each ACTIVE
   generator object and scans each suspended caller segment up to its
   stack's end (main's `stack_bottom` for the outermost, the parent
   generator's stack end for nested resumes).
3. **`_ejs_generator_start` allocated the final iter-result AFTER
   popping the generator chain** — same bogus-range class as (1) while
   still on the generator stack.  Fixed by allocating before the pop.
4. **The suspended-generator stack scan was inverted**: it scanned
   `[stack_base, saved_SP)` — the DEAD region (stacks grow down) — so
   the live frames of every suspended generator were invisible: values
   referenced only by a suspended generator's frames could be collected
   and resumed-into (use-after-free).  Fixed to `[saved_SP, stack_end)`
   with out-of-range SPs degrading to a whole-stack scan.
5. **LOS lookups now honor interior pointers** (`find_page_and_cell`
   used an exact base match): a large object referenced only through a
   derived pointer — likelier once the runtime is `-O2` and base values
   die in registers — was collectable out from under the reference.
   Interior hits canonicalize to the base (the page-cell path always
   did this); the cost is a slightly larger conservative false-positive
   surface, which a conservative collector accepts by construction.
6. `_ejs_gc_push_generator` now aborts loudly at MAX_GENERATORS instead
   of silently corrupting the chain array.

Pinned by suite tests `generator23.js` (GC while running on the
generator stack), `generator24.js` (suspended-frame-only liveness across
forced collections), `generator25.js` (nested active chain) — all
node-identical, all green under `EJS_GC_EVERY_N_ALLOC=7`.

**Pinned, NOT fixed (pre-existing, outside gc scope):** an uncaught
exception thrown out of a generator body aborts the process (the
desugar's outer catch rethrows on the generator stack and the unwinder
walks off the makecontext frame; node prints the exception in the
caller).  Recorded here so the exceptions/coroutine interaction gets an
owner later.

## Instrumentation (EJS_GC_PROFILE=1)

Two bits from the header's gc-reserved range (57-63; every existing
consumer masks): YOUNG — set at allocation, cleared on first survival,
so "young" = allocated since the last collection, exactly a nursery's
population; PINNED — set once per cycle per object hit by a
conservative reference (recorded even when already marked: the white
check is a marking optimization, not a pin filter).  Per-cycle stderr
line: live set, young-allocated vs young-survived (count/bytes/%), pins
by source (cstack / regs / genstack) with env-interior, LOS, young/old
splits, pause.  Shutdown summary (atexit): totals, rates, kind and
size-class histograms.  The YOUNG-bit OR is folded into the header
store the allocator already does; everything else is behind the env
var — the measured path is unperturbed when profiling is off.

## The numbers, runtime `-O0` (as found)

**Self-compile** (stage1 `ejs.exe` compiling `ejs-es6.js`, the real
workload; 127.4s wall):

- **79.5M allocations, 3,419MB** — 26.8MB/s, 624K allocs/s.
- Kinds: **object 47.5M (60%), closureenv 31.0M (39%)**, primstr 0.94M,
  primsym 14.  The env-churn hypothesis is confirmed: 2 of every 5
  allocations are closure environments — gc-P2's inline `make_env`
  fast path targets the right thing.
- Sizes: ≤32B: 31.4M / 957MB; ≤64B: 42.9M / 2,022MB; ≤128B: 5.2M /
  440MB; **LOS: 2,489 / 0.86MB** — the heap is uniformly tiny-object.
- Survival: warmup cycles 33%/35%/12.5%, then **steady-state 2.5-3.5%
  of young bytes survive** each ~60-110MB cycle — a nursery reclaims
  ~97% of its space per minor GC on the compiler workload.
- **Pins: 100-650 objects (5-36KB) per cycle** out of ~1M-object live
  sets — C-stack source dominates, registers contribute 1-5,
  generator-stack 0 (none active), **env-interior 0**, LOS 0.  The pin
  population is 4-5 orders of magnitude smaller than the live set.
- Pauses: 240-770ms per cycle; total 9.2s = 7.2% of wall.

**Kernels** (types-bench2 2.00s / types-bench3 0.31s under `--types`,
matching their P4.x records): 1.57M allocs per 60MB cycle, **young
survival 0.0-0.1%**, pins 13 objects, pauses ~21ms.

**Generator kernel** (gens1small): allocations on the generator stack,
chain pins visible under stress; profile attributes genstack pins once
generators are suspended with live frames.

## What the numbers decide (the plan's open orderings)

- **gc-P2 (nursery + inline alloc) is GO, and P3 need NOT move ahead of
  it**: the pin rate under pure conservative roots is trivially small
  (≤650 objects/cycle, KBs), so premature-promotion erosion from pinned
  young objects is negligible.  Bartlett cell-pinning at this rate is
  free; precise JS frames (gc-P3) remain a throughput/paranoia
  improvement, not a prerequisite.
- **Inline allocation should cover `make_env` AND plain objects**
  early: objects+envs are 99% of allocations.
- The LOS is irrelevant to the mover's economics today (2,489 allocs,
  <1MB) — the P4.2 slot-cap workaround (field cap 14) stays until the
  planned size-class/lookup work, with no added urgency from these
  numbers.
- Marking cost (not sweep) dominates the pause at `-O0`; the `-O2`
  runtime move (below) and later concurrent marking (gc-P6) both attack
  it.

## The `-O2` runtime experiment — LANDED

Runtime moved `-O0` → `-O2` (`defs.bzl`), scanner assumptions
re-verified: `MARK_REGISTERS` spills callee-saved registers explicitly
(volatile asm), live-across-call values sit in callee-saved registers
or caller frames per the ABI (both scanned), interior pointers
canonicalize in page and (now) LOS lookups, and the generator stress
tests exercise collection from generator stacks under optimization
(generator23-25 + EJS_GC_EVERY_N_ALLOC=7 all green on the -O2 build).

**Results:**

- **Self-compile: 127.4s → 41.6s wall (3.06×).**  The allocation totals
  are bit-identical between the runs (79,529,117 allocs / 3,419.54MB —
  the workload is deterministic, which doubles as an instrumentation
  sanity check).  Total pause 9.2s → 6.05s (240-770ms → 140-608ms per
  cycle); survival and pin profiles unchanged (steady-state 2.4-3.4%
  young-byte survival; pins 380-530 objects/cycle, cstack-sourced,
  `regs` drops to 0 — the optimized runtime holds fewer stray ejsvals
  in callee-saved registers at the collection point; env-interior
  still 0).
- **types-bench2 (--types): 2.00s → 0.68s (2.9×)** — most of what P4.5
  recorded as the "1.71s allocation-loop residual" was runtime `-O0`
  overhead, not intrinsic allocation cost.  types-bench3: 0.31s →
  0.18s.  (Flag-off bench2: 6.7s → measured on the -O2 runtime at the
  gate as well.)
- The `-O2` flip is kept (defs.bzl comment records the P0 verification);
  gc-P2's inline-allocation gate ("strictly better than the free-list
  path") must be measured against THIS baseline.

Measurement gotchas recorded for future phases: the EIR optimizer sinks
non-escaping allocations, so a churn kernel can profile as ~zero allocs
(size the probe's escapes deliberately — the optimizer-on rule cuts
both ways); survived-bytes are cell-size-accounted while allocated-bytes
are request-accounted, so tiny survivor sets can read as >100% on
sub-KB cycles (harmless at real scales).

## Phase checklist impact

- gc-plan P0: DONE (this doc).  P1's remaining scope: forwarding
  helpers only (the 64-bit header + reserved bits + lib/types.ts
  lockstep landed with shapes P4.1).
- gc-P2 proceeds with conservative roots; P3 stays sequenced after
  (pin-rate evidence above).
