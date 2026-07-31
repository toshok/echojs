# GC plan: an industrial generational moving collector, co-designed with the compiler

Phase ids here are `gc-P0`..`gc-P7` (formerly bare P0..P7 in this
doc).  The ordering spine lives in `docs/plans.md`.


A plan for replacing echojs's stop-the-world conservative mark-and-sweep
collector (`runtime/ejs-gc.c`) with a generational, moving, eventually-
concurrent collector — in independently-landable phases, each of which leaves
the tree green and shippable.

**The stance, up front.** An earlier revision of this document took the current
repo state — conservative stack scanning, runtime-call allocation, the hash-map
object model — as fixed, and designed a collector around *tolerating* it
(Bartlett mostly-copying with pervasive pinning). That got the substrate right
and the ambition wrong. The compiler is ours and is being actively rebuilt
(EIR, the optimizer, the maam type oracle); every allocation site, every store,
and every safepoint in compiled code is an EIR op we control, declared in the
effect table (`lib/eir/ops.ts`: `E.GC`, `E.WRITE`). A modern collector for this
engine is a **compiler/runtime co-design**: precise, relocatable roots in JS
frames because we emit them; inline allocation because we emit that too;
barriers the optimizer can elide; object layout designed once, jointly with the
maam shapes work. Conservatism survives only where it is genuinely stuck — the
hand-written C runtime — and the mostly-copying substrate exists to absorb
exactly that remainder, not to excuse imprecision everywhere.

**Why now.** The compiler work is landing: EIR optimization (literal sinking,
IIFE inlining, env scalar replacement, DCE), typed guarded arithmetic (10.3× on
a numeric kernel, maam-plan P3), with specialization (P3.6) and shapes (P4)
queued. As mutator time falls, allocation and collection become the floor. The
goal of this plan is that **GC is never the reason echojs loses a benchmark**:
allocation as cheap as a bump-and-compare, minor pauses sub-millisecond, major
pauses bounded, and a design that scales with the object-model improvements
rather than fighting them.

## What we have today, as found

- **Collector** (`runtime/ejs-gc.c`, ~1700 lines): stop-the-world,
  single-threaded, tri-color mark-and-sweep, non-moving. Trigger is 60 MB of
  allocation since the last cycle (`ejs-gc.c:1408`), plus `GC.collect()`,
  allocation-failure fallbacks, and shutdown.
- **Allocator**: segregated free-lists in size classes 16–256 bytes over 32 MB
  arenas, a per-page bump pointer for fresh pages, and a large-object store
  (LOS) for anything larger. Every allocation — including from compiled JS —
  is a call to `_ejs_gc_alloc(size, scan_type)` (`ejs-gc.h:33`).
- **The heap is already precisely traceable.** Every object carries a per-class
  `Scan` spec-op (`ejs-object.h`) plus typed scanners for strings, symbols, and
  closure environments. The collector knows the exact outgoing edges of every
  heap object. This is the single most important asset we have.
- **Roots are not precise.** An explicit root list (~142 registrations, almost
  all static singletons), module exotics, and **a conservative scan of the C
  stack and spilled registers** (`mark_thread_stack`, `MARK_REGISTERS`) that
  treats anything pointer-shaped — including interior pointers — as a root.
- **Generator stacks are not scanned at all** — `mark_generator_stacks` is a
  stub (`ejs-gc.c:1087`). A latent correctness bug today; a hard blocker for
  any mover.
- **Value representation**: SpiderMonkey-style NaN-boxing (`runtime/ejsval.h`);
  GC pointers live in the low 47 bits of an 8-byte value, heap addresses forced
  below 2⁴⁷.
- **Compiler emits no GC support.** The EIR backend (`lib/eir/emit.ts`) keeps
  locals as pure SSA values — "locals never touch memory"; no statepoints,
  stackmaps, or safepoint metadata. Notably, `env_load`/`env_store` each make a
  runtime call to `_ejs_closureenv_get_slot_ref` and then load/store through
  the returned raw `ejsval*` (`emit.ts:619-643`) — an interior pointer the
  conservative scanner must honor, and a per-access call the mutator pays.
- **No write barriers anywhere.** No card table, no remembered set, no handle
  abstraction in the C runtime.
- **Single-threaded.** One event loop, no workers, GC lock macros are no-ops.
  Collections only happen inside `_ejs_gc_alloc`, i.e. under a runtime call.
- **Build**: Buck2, Homebrew LLVM **22.1.8**, runtime compiled **`-O0`**
  (`defs.bzl:83`, inherited from the old config.mk), user JS compiled `-O2`.
  New collector code goes in `runtime/BUCK` `shared_sources` and must survive
  compilation as Objective-C on macOS.

## Assets, liabilities, and the resulting shape

Three assets determine the design:

1. **Precise heap tracing already exists** (the `Scan` ops). Evacuation is
   "copy + rewrite the slot" wherever marking today is "gray the target."
2. **The mutator is single-threaded — today.** Barriers need no atomics,
   safepointing is one handshake, and a collector thread (later) coordinates
   with exactly one partner. We exploit this deliberately — but **concurrent
   JS is a stated goal** (Workers; the tc39 shared-memory work), so every
   single-mutator shortcut is taken behind a seam with a documented exit path
   (§"Concurrent JS"). The structural insight that keeps this cheap: the
   platform's first concurrency step is *isolates* — N heaps, each with one
   mutator — which preserves per-heap single-mutator simplicity; only
   shared-memory objects ever put two mutators in one space.
3. **We own every emitted allocation, store, and safepoint.** The effect table
   already classifies them (`E.GC`, `E.WRITE`). Anything the design needs from
   compiled code — spill slots, barriers, inline allocation, liveness metadata
   — is an emitter feature, not a research project.

One liability: **root precision**, and it splits cleanly in two:

- **JS frames** — ours to fix. The emitter will make them precise *and
  relocatable* (see "Roots" below). This is the plan of record, not a fallback.
- **C runtime frames** — hundreds of `EJS_NATIVE_FUNC`s holding bare `ejsval`
  locals across allocation points. Making these precise is the SpiderMonkey
  exact-rooting migration (years). We don't do it: C frames stay conservatively
  scanned, and objects they reference get **pinned** for the cycle. JSC ships
  this way permanently; pinning C-frame referents is industrially respectable,
  and every collection necessarily has some C frames live (the alloc slow path
  is C), so the pin population never reaches zero anyway. What matters is that
  it becomes *small and bounded* once JS frames are precise.

The **mostly-copying substrate** (Bartlett) is what lets both root regimes
coexist in a moving collector: precise references are evacuated and rewritten;
ambiguous references pin their targets in place for the cycle. In the earlier
revision pinning had to absorb *all* stack roots; here it absorbs only the
C-runtime remainder — but the machinery is identical, and it means precision
work is an incremental improvement, never a flag-day prerequisite.

NaN-boxing stays. It forecloses LLVM's native statepoint relocation (which
needs reference-typed values), but not precision — see below. The oracle-driven
hybrid representation (typed values as real `ptr addrspace(1)`, maam-plan
P3.6's typed calling convention taken further) remains the far-future path to
native statepoints for the typed fraction of the program; nothing in this plan
blocks it and nothing waits for it.

## Target architecture

```
             ┌────────── nursery ──────────┐    ┌─────────── old gen ───────────┐
 inline  ──→ │ bump pointer, block chain;  │    │ block-structured; evacuated/  │
 bump alloc  │ evacuating minor GC;        │ ─→ │ compacted per-block; pinned   │
 in JS code  │ pins at cell granularity    │    │ cells swept in place          │
             └─────────────────────────────┘    └───────────────────────────────┘
                              ┌──── LOS ────┐
        size > threshold ──→  │ mmap'd, never moved │
                              └─────────────────────┘

 roots:    JS frames — precise, relocatable (emitter-owned gc-frame slots)
           C frames  — conservative scan → cell-granularity pinning
 barrier:  card marks + SATB old-value log, one barrier, two consumers;
           compiler elides barriers on initializing stores
 later:    concurrent marking on a collector thread; brief STW evacuation;
           optionally fully concurrent evacuation (Brooks forwarding)
 further:  Workers = N isolates (one heap+mutator each, this design ×N);
           tc39 shared structs = a contained shared space, atomic
           protocols scoped to it alone
```

## Roots: precise, relocatable JS frames

### What LLVM offers (condensed; the conclusions matter)

- **`llvm.gcroot`** wants pointer-typed slots; an ejsval is an `i64` that is
  only sometimes a pointer. Unusable — the abandoned experiment in
  `lib/abi.ts:34-46` hit exactly this.
- **`gc.statepoint` + `RewriteStatepointsForGC`** relocates GC values LLVM can
  *type* as GC pointers (`ptr addrspace(1)`). Incompatible with polymorphic
  NaN-boxed i64s; reachable only after a value-representation split (the P3.6
  hybrid). Far future, not load-bearing.
- **`llvm.experimental.stackmap`** records the *locations* (register, stack
  slot, or Direct alloca) of arbitrary-typed live values — including i64 — at
  a given call site, into an `__LLVM_StackMaps` section keyed by return
  address. It records; it does not relocate.

### Plan of record: emitter-owned gc-frame slots

The emitter gives each function a **gc-frame**: a contiguous alloca array of
ejsval slots. At every safepoint (every `E.GC`-effect op — all of which lower
to calls, including the inline-allocation slow path):

1. every live GC-typed value is **stored** into a gc-frame slot before the
   call, and
2. every use after the call reads the **reloaded** value — the emitter rewrites
   the SSA uses, so no pre-safepoint copy survives the call.

Because the roots now live in memory we own, the collector can *rewrite* them:
precise **and relocatable**, NaN-boxing intact, no LLVM fork. This is the
"shadow stack" idea, but built where it belongs — in our own emitter, on the
liveness information EIR already has.

How the collector *finds* the frames, two variants, decided by measurement:

- **Chained frames (start here).** Function prologue links its gc-frame record
  (base, slot count) onto a thread-global chain; epilogue and unwind edges
  unlink it. Simple, portable, no binary-format work. Costs a couple of stores
  per function entry/exit — measurable, possibly ignorable, and functions the
  optimizer proves allocation-free (no `E.GC` ops transitively) skip the frame
  entirely.
- **Return-address-keyed maps (the zero-entry-cost upgrade).** Emit
  `llvm.experimental.stackmap` at each safepoint listing the gc-frame slots;
  the collector walks frame pointers and looks up return addresses in the
  `__LLVM_StackMaps` section. No per-call chain maintenance; the stackmap
  intrinsic serves purely as a *metadata emitter* while the slots themselves
  make relocation sound. Requires a stack walker + section parsing (Mach-O and
  ELF) and `-fno-omit-frame-pointer` discipline.

Two sharp details, named now because they are the kind that silently corrupt:

- **Store-to-load forwarding.** If LLVM can prove the safepoint call doesn't
  touch the gc-frame, it will forward the pre-call store to the post-call load
  and the collector's rewrite is lost. The gc-frame base must **escape** (the
  chain registration does this naturally; under the stackmap variant, escape it
  explicitly once per function). Verify with a stress test that moves *every*
  object *every* collection.
- **Interior pointers must not be live across safepoints.** `env_load`/
  `env_store` currently materialize raw `ejsval*` slot refs via a runtime call.
  The emitter should instead compute slot addresses inline (a GEP off the env
  base) and **recompute per use** rather than caching across a safepoint — the
  env base is then the only rooted value, and it relocates like any other.
  Bonus, independent of GC: this deletes a runtime call from every env access,
  a straight mutator win available today.

The C runtime keeps the existing conservative scanner verbatim — stack ranges,
`MARK_REGISTERS`, interior-pointer canonicalization via `find_page_and_cell` —
but its hits **pin** rather than mark. No handle API, no rewrite of hundreds of
native functions.

**Sequencing note.** The collector does not *wait* for precise JS frames: the
mostly-copying substrate runs with fully conservative roots on day one (that's
the earlier revision's design, still sound), and precision lands as a
pin-rate reduction. Phase 0 measures where the pins actually come from; if the
young-gen pin rate under conservative roots is already low, precision can slide
later in the sequence with no design change.

## Allocation: inline the fast path

Every allocation today is a full call into `-O0` runtime code. Industrial
engines allocate in ~4 inline instructions; so will we:

```
bump = *bump_ptr; new = bump + size;
if (new > *limit) goto slow;         // slow: call _ejs_gc_alloc_slow → safepoint
*bump_ptr = new;                     // object header init follows inline
```

Single-threaded means the bump pointer is a plain global — no TLS. The
emitter stages this by allocation kind, payoff-ordered:

1. **Closure environments** (`make_env`) — the most frequent allocation in
   closure-heavy code, trivial to initialize inline (header + length), size
   known at compile time.
2. **Object/array literals** (`make_object`/`make_array`) — worth inlining
   once shapes land and initialization is "store shape id + slots" rather
   than "build a hash map".
3. Strings/others stay runtime-side.

The slow path is the safepoint; the fast path never GCs, which is what makes
"spill live values at safepoints only" cheap — straight-line allocating code
pays nothing.

**Pretenuring**: the maam oracle (or cheap runtime feedback) tags allocation
sites whose objects reliably survive; those sites' inline sequence bumps an
old-gen block instead. Small change once the generational split exists.

**Interplay with allocation sinking.** The optimizer is already removing
allocations (env scalar replacement landed; object/array sinking planned in
`docs/plans.md`). These compose — sinking removes allocations, the nursery
makes the survivors cheap — but they must be *measured together*: Phase 0's
allocation profiling runs with the optimizer on, so both efforts see the same
numbers and neither claims the other's wins.

## Write barrier: one barrier, two consumers

Generational collection needs old→young stores caught; concurrent marking
(later) needs overwritten values logged. Build one barrier that does both from
day one:

- **Card marking** for location: old gen divided into ~512 B cards; a store
  into old gen dirties the card (shift + byte store, unconditional, no
  branches). Minor GC scans dirty cards only.
- **SATB old-value log** for the future concurrent marker: the barrier records
  the overwritten ejsval into a sequential-store buffer. Dormant until Phase 6,
  but designing it in now is what makes concurrency an *addition* rather than a
  barrier rewrite.
- **Non-atomic everything** — single mutator. A plain store to the card byte,
  a plain SSB append. Revisit only if Workers ever land.

Where it goes — the store surface is small and enumerable:

- **Runtime**: `_ejs_object_setprop` and the property-map insert path; one
  barrier covers most object writes.
- **Emitted code**: `env_store` and `module_slot_store` are the only inline
  ejsval stores (`emit.ts`); the emitter adds the card-dirty sequence there.

And where it *doesn't* go — the compiler elides barriers it can prove dead:

- **Initializing stores.** Stores that fill in a just-allocated object
  (literal construction, `make_env` slot init) target an object that is
  necessarily nursery-resident: no barrier. This is the majority of stores in
  allocation-heavy code and the elision is purely local.
- **Provably-young targets.** The optimizer/oracle can extend "just allocated"
  to "allocated in this function and not yet escaped/collected-across".
- **Non-reference stores.** Once typed slots exist (f64 slots in specialized
  envs/shapes, post-P3.6/P4), stores of unboxed doubles need no barrier and no
  trace entry at all.

## Object header, forwarding, and shapes (joint design with maam P4)

`GCObjectHeader` is a bare `uint32_t` (`ejs-types.h:30`). **Widen it to 64
bits** with room for: forwarded bit + forwarding address (or the classic
first-word overwrite — objects are 8-aligned, low bits free), age, pin, mark,
card/log bits, and — the important part — a **shape/trace-map index**.

Layout mechanics: for `EJSObject` the widening is free (4 B of padding already
follows the header before the `ops` pointer — compiled code's view via
`lib/types.ts` doesn't shift). `EJSClosureEnv` and `EJSPrimString` shift their
second word; runtime structs and `lib/types.ts` must move in **one atomic
change**, verified with the old collector still active.

**The shapes tie-in is the single highest-leverage item in this document.**
maam-plan P4 designs shape-guarded property access from the oracle's
`layouts()` (per-allocation-site field names, offsets, type sigs). That design
and the GC's object layout are **one design, written once**:

- objects become **shape id + contiguous inline slots** — fixed-size, trivially
  copyable, no out-of-line malloc'd `_EJSPropertyMap` (which today never
  compacts and is traced through a virtual call);
- tracing becomes a **per-shape pointer-offset bitmap** — branch-free, no
  indirect `Scan` call, and exactly what a fast evacuation loop wants;
- objects are **born with their shape** at oracle-known allocation sites — no
  dynamic hash-map buildup;
- property storage lives **in the GC heap** and compacts with everything else;
- and it is the doorstep to inline caches, which is where "competitive with
  V8" actually gets decided.

This plan's P1 header change reserves the bits; the shapes design doc (maam
P4) fills them in. The GC must not ship a header layout that shapes then has
to break.  [Update 2026-07-23: that design now exists — **docs/shapes-plan.md**
— written against this section's layout; its Step A claims 24 bits + a mode
bit of the widened header for the shape index, and its P4.1 lands jointly
with this plan's P1 as the one atomic layout change, whichever starts first.]

Per-kind moving notes: `EJSObject` copies as a struct (the property map, while
it still exists, is malloc'd and stays put); envs copy header+slots with each
slot rewritten; flat strings copy, out-of-line buffers stay put, ropes'
children are ordinary edges; LOS never moves; suspended generator stacks are
conservative root ranges (pin) once the Phase-0 bug fix lands.

## Concurrency I: the collector on its own thread

The mutator stays single-threaded (per isolate — see the next section); the
collector eventually gets its own thread, in two tiers:

1. **Concurrent marking + brief STW evacuation — the sweet spot; target
   first.** Marking (the long phase, proportional to live set) runs on the
   collector thread, fed by the SATB log; a single-mutator handshake takes the
   root snapshot; a short STW window evacuates unpinned survivors (small,
   because generational). Pause becomes proportional to survivors, not live
   set. SpiderMonkey lived here productively for years.
2. **Fully concurrent evacuation.** Objects move while the mutator runs; needs
   a per-object forwarding word (Brooks — the widened header has room) and a
   load barrier. Real cost per load; adopt only if tier 1's numbers demand it.
   An *incremental* (time-sliced, same-thread) marker is the cheaper fallback
   if the collector thread proves troublesome — a rung, not a requirement.

Conservative/pinned roots compose fine with both tiers: pinned objects simply
don't move, and precise JS frames (relocatable) are what allow
stack-referenced objects to participate in evacuation at all.

## Concurrency II: concurrent JS — Workers, shared memory, multiple mutators

Concurrent JS is a goal, not a hazard: the web platform has Workers, and tc39
is converging on safe shared-memory primitives (SharedArrayBuffer today;
shared structs / `Atomics` extensions in progress). The design must
accommodate multiple concurrent mutators *eventually* without paying for them
*now*. The platform's own staging makes that tractable, because each step
isolates a different cost:

1. **Workers as isolates — N heaps, one mutator each.** The web's Worker model
   shares nothing traced: `postMessage` copies, and a `SharedArrayBuffer` is
   untraced off-heap memory (the GC only keeps the per-isolate wrapper object
   alive — SAB support is *easy* and can come early). Under isolates,
   everything in this plan holds per-heap unchanged: non-atomic barriers,
   one-handshake safepoints, a lock-free bump nursery — each isolate has its
   own. What isolates require is that collector state be **instantiable**:
   `ejs-gc.c` today is a pile of file-static globals. New collector code puts
   all state in a heap-context struct from day one, so "spin up a second
   isolate" is plumbing, not a rewrite.

2. **Compiled-code contact points go through a seam.** Emitted code touches
   heap state at a handful of named points: bump/limit pointers, card-table
   base, gc-frame chain head, (later) a safepoint-poll flag. The emitter
   treats these as **context accessors** — today they resolve to plain
   globals; under isolates they become TLS loads or a pinned context register.
   Because echojs is AOT and statically linked, flipping the accessor
   implementation is one emitter change plus a world recompile — there is no
   deployed-binary ABI to preserve. The discipline that matters now is *not
   scattering* heap-state contact through emitted code, so the flip never
   grows a long tail.

3. **Shared-memory objects are the real multi-mutator step — contained in a
   shared space.** When shared structs (or an engine-level shared heap) land,
   shared objects live in a distinct **shared space** with the expensive
   protocols scoped to it alone: atomic card/SATB barriers on stores into
   shared objects, CAS-installed forwarding if it ever moves (more likely:
   non-moving initially), collection under a global rendezvous of all
   isolates. The tc39 proposal's own containment rule — shared objects
   reference only other shared data — is exactly what keeps this tractable:
   isolate→shared edges are roots into the shared space; shared→isolate edges
   don't exist by construction. Per-isolate nurseries and old gens keep their
   cheap single-mutator protocols forever.

4. **Safepoint reachability.** Allocation-slow-path safepoints suffice for one
   mutator. A multi-isolate rendezvous needs every thread to reach a safepoint
   promptly, including one spinning in a non-allocating loop — that means
   emitter-inserted **back-edge polls** (a flag check; EIR knows its loop
   back-edges). Not emitted today; reserved as a known emitter feature, and
   the gc-frame design already gives polls a place to stand.

What we do **now** (cheap, structural):
- no new file-static collector state — everything in the heap-context struct;
- heap-state access from emitted code only via the context-accessor seam;
- metadata designed atomics-friendly: side mark bitmaps that can be set with
  an atomic OR, a forwarding word that can be CAS-installed, card/SATB buffers
  that shard per-thread;
- no protocol that is correct *only* for one mutator by construction — the
  single-mutator fast paths must be the degenerate case of a design that
  admits N, not a different design.

What we do **not** do now: no locks or atomics on any hot path, no shared
space, no rendezvous protocol. Those are paid when the platform work arrives,
and the seams above are what make the bill small.

## Knobs

One primary knob: a **heap-growth target** — collect when live × (1 + g) is
reached, `g` auto-tuned from recent survival rates. Nursery size, block size,
card size, promotion age: derived, not exposed. Existing `EJS_GC_*` env vars
survive as debug overrides only. If a knob can be derived from a measurement,
derive it.

## Adjacent runtime work (same bottleneck, not this collector)

Named here because "the runtime is about to be the bottleneck" is bigger than
GC, and these are cheap:

- **The runtime is compiled `-O0`** (`defs.bzl:83`). Moving to `-O2` is likely
  the single cheapest runtime speedup available and directly speeds the
  collector itself. The conservative scanner's assumptions (register spills,
  no hidden pointer representations) must be re-verified under `-O2` — do it
  in Phase 0 while instrumentation is fresh. LTO across runtime/user-code is a
  further step with the same caveat.  *(DONE at P0: `-O2` landed after
  verification — self-compile 3.06× faster, types-bench2 2.9×; see
  gc-p0-results.md.  LTO remains open.)*
- **`env_load`/`env_store` runtime-call round-trip** — inline the slot address
  computation (also required for precise roots; see above). Can land early and
  alone.
- **Property access cost** (hash map, no ICs) — owned by shapes (maam P4), not
  this plan; noted so nobody aims the GC at a mutator problem.

## Phased plan

Bias, as with the eir/maam plans: small phases, matrix green after each
(`//:test-eir`, `//:test-stage0..3`, the `--types` diff lane once relevant),
each independently revertable. The old collector stays behind a build flag
through Phase 3 for A/B and differential testing.

- **gc-P0 — Correctness prerequisites + measurement.** Fix generator stack
  scanning (`ejs-gc.c:1087` stub) — a real bug today, a corruption source under
  any mover. Add instrumentation: allocation rate and size/kind profile (with
  the optimizer on), survival rates, and a **pin-rate estimator** — walk
  conservative roots and report pinned bytes, retained-block counts, and pin
  *sources* (C stack vs. register spill vs. env interior pointers), separately
  for what would be young vs. old. Run the `-O2`-runtime experiment and
  re-verify scanner assumptions. **Gate: the numbers.** They size the payoff of
  every later phase and decide how early precise JS frames need to land.
  **DONE 2026-07-24 — docs/gc-p0-results.md.**  The generator work found
  FOUR bugs (crash on collect-during-generator-execution; unscanned
  suspended main segment; alloc-after-pop on completion; and the
  suspended-stack scan bounds INVERTED — it scanned the dead region and
  missed every live frame), pinned by generator23-25 under gc-stress;
  LOS lookups made interior-tolerant.  The numbers: 2.4-3.4% steady
  young survival, 39% closureenv allocation share, pins in the hundreds
  of objects/KBs per cycle (⇒ P2 ships on conservative roots; P3 stays
  behind it), and the `-O2` runtime landed at 3.06× on the self-compile.

- **gc-P1 — Header widening + forwarding plumbing.** 64-bit header, bits
  reserved per the shapes tie-in; coordinated `runtime/` + `lib/types.ts`
  layout change, landed atomically with the old collector active; forwarding
  read/write helpers. No behavior change. **Gate: matrix green on all three
  bootstrap targets.**

- **gc-P2 — Generational nursery: the payoff phase.** Block-structured
  spaces; all new collector state in an instantiable heap-context struct and
  all emitted heap-state access through the context-accessor seam
  (§"Concurrency II" — this is when the discipline starts, because this is
  when the new code is written); bump-pointer nursery with the **inline
  allocation fast path** for
  `make_env` (objects follow later); card-table + SATB-logging store barrier
  (runtime sites + the two emitted sites, with initializing-store elision);
  **evacuating minor GC** on the mostly-copying substrate — precise heap edges
  and root-list entries evacuate, conservative hits pin at cell granularity
  (`find_page_and_cell` already canonicalizes interior pointers). Old gen
  stays mark-sweep. **Gate: allocation throughput strictly better than the
  free-list path; minor-pause p99 sub-millisecond on the benchmark corpus;
  differential vs. old collector across the whole suite plus a
  collect-every-N-allocations stress mode; pin-rate report from real runs.**

- **gc-P3 — Precise JS-frame roots.** Emitter-owned gc-frame slots at `E.GC`
  safepoints with SSA-use rewriting; chained-frame variant first; env slot
  address inlining (interior pointers die); allocation-free functions carry no
  frame. Nursery pins drop to C-frame-referenced objects only. **Gate:
  move-everything stress mode green (catches store-forwarding bugs); pin rate
  vs. Phase 2 recorded; mutator regression from spills measured and
  acceptable; matrix green.**

- **gc-P4 — Mostly-copying major collection.** Evacuate/compact unpinned
  old-gen blocks; pinned cells swept in place; heap actually shrinks. This is
  where fragmentation dies. **Gate: identical output vs. Phase 3 under stress;
  demonstrated heap shrink on a fragmenting benchmark; auto-tuned growth
  target replaces the 60 MB constant, knob census = 1.**

  **FIRST ORDER OF BUSINESS (measured 2026-07-25, while gating
  sinking-P3): the conservative pin scan has a scaling cliff that
  dominates self-compile wall time.**  Evidence, so it isn't
  re-derived: on the desugar.js-closure compile (20 modules), minor-GC
  pin scans total 41–126 s of a 46–132 s wall — pauses grow from
  <1 ms early to 500–860 ms during deep-recursion parse/lower phases.
  Mechanism: `mark_ejsvals_in_range` treats every stack word as a raw
  pointer candidate; the only rejection before the per-word arena
  bsearch (and, before the sinking-P3-era fix, a LOCKED LINEAR walk of
  the whole LOS list) is the `[conservative_lo, conservative_hi)` span
  — and once a late arena or LOS mmap lands beyond the C/LLVM heap,
  that span swallows it, so during codegen MILLIONS of stack words
  pointing into LLVM's own allocations pass the prefilter.  The cost
  is therefore bistable per RUN (mmap layout luck: the same binary
  compiles the same input in 6 s or 60 s) and quasi-deterministic per
  BINARY (any allocation-pattern change — sinking-P3's was +1.4%
  allocs — shifts when arenas are minted and can lock a binary into
  the slow mode; its stage1 sat at ~1.5–2× baseline wall).  A
  bounds prefilter for the LOS walk (`los_lo/los_hi`,
  ejs-gc.c) landed with sinking-P3; the real fixes belong here:
  reserve arena address space once at init (span stays tight and
  disjoint from the C heap forever, and arena lookup becomes two
  compares + an index instead of a bsearch), and give the LOS a real
  lookup structure (the P6.3 refactor).  Self-compile wall time should
  then sit at the fast mode (~6 s for the desugar closure)
  deterministically — a bigger win than most optimizer phases.

- **gc-P5 — Shapes intersection (floats with maam P4).** When the shapes
  design lands, the collector consumes it: per-shape trace bitmaps replace
  `scan_type` + virtual `Scan`; inline-slot objects copy as memcpy + bitmap
  walk; property storage moves into the GC heap; inline allocation extends to
  object literals; typed slots get barrier/trace elision. Sequenced by
  maam-plan; the GC-side work is deliberately small because P1 reserved the
  header bits.

  **Settled design (2026-07-28, the Step B addendum).**  The governing
  choice: shaped slot storage stays a *closureenv-shaped* region reached
  through the `obj->slots` ejsval — but born-with-shape allocation places
  it **inside the object's own cell** (object header, ops, proto, slots
  ejsval pointing at `obj+32`, then an embedded env header + the slot
  values).  Embedded-ness is pointer identity (`env == (char*)obj +
  sizeof(EJSObject)`), no new header bit.  Because the compiled
  slot-addressing seam (`slotRef`, emit.ts) already loads the slots
  ejsval and indexes the env, **compiled slot access, has_shape guards,
  and the verifier's contract change not at all**; only allocation sites
  and the collector know.  Consequences, each independently gated:
  - **Single-cell shaped allocation**: `_ejs_object_new_shaped` grows a
    shape-index-passing form (trusting the module's interned shape,
    values verified against the f64 mask, fallback = today's
    re-derivation); one cell of `32 + 16 + 8n` bytes replaces the
    object-cell + env-cell pair.  Constructor allocations get there via
    a **birth-capacity hint on EJSFunction** (set from the result's
    field count after the first construct; ordinary Construct allocates
    `this` with embedded capacity = hint) — no compiler plumbing, works
    flag-off.  Growth past embedded capacity falls back to an
    out-of-line closureenv (today's doubling path); the object stays
    shaped, the embedded region goes dead.
  - **Barrier owner flip**: shaped-slot stores remember the *wrapper
    object* (C sites and emitted slot_store both; today they remember
    the env), and the ordinary object's Scan walks the slot *values*
    directly in both modes (plus the env edge only when out-of-line).
    This makes owner pointers always cell heads — no interior-pointer
    remset entries — and dirty-object rescans see embedded slots.
  - **Evacuation**: whole-cell memcpy (the existing routine) + a shaped
    case in `minor_fixup_evacuated`'s self-interior-pointer fixup (the
    flat-string/EJSArguments precedent): rebase the slots ejsval when
    it points into the moved cell.  The embedded slots edge is never
    presented to the precise slot callbacks (they assume object-base
    payloads); Scan's mode switch owns that.
  - **Per-shape trace masks**: the shape record gains an f64 bitmap
    (u16, built incrementally at intern time from parent | repr); the
    ordinary-object walk skips f64 slots — precise trace elision — and
    the three hot collector sites (mark, minor trace, compact fixup)
    may short-circuit `ops->Scan` for `_ejs_Object_specops` objects
    into the same inline walk.  Out-of-line arrays keep the closureenv
    range scan (raw doubles are NaN-box-valid numbers; unchanged).
  - **The 256-byte size class is enabled**: `ffs(256)=9 >
    OBJECT_SIZE_HIGH_LIMIT_BITS` routes 256B cells to the LOS today —
    an off-by-one that predates gc-P4's LOS lookup fix and the direct
    arena map.  Enabling the already-plumbed class (pagelist, seam
    words, emitter cap all exist) makes every cap-14 shaped object
    single-cell (`32+16+112 = 160 ≤ 256`) and takes >14-slot envs off
    the LOS; A/B-measured at the gate (frag bench + self-compile).
  - **Emitter inline allocation for `make_object_shaped`** (literals):
    the make_env bump-sequence precedent, one guard (module shape
    global != NOMATCH — literal installs are CreateDataProperty, so no
    epoch/proto check is needed), header stamped with the shape index,
    initializing stores, no barriers.  Inline `fill_object_shaped` is
    measured-later work (the ctor hint already single-cells it).
  - **Typed-slot barrier elision is already true** (emitted f64
    slot_store skips the barrier; the runtime filter exits on
    non-traceable values) — the phase audits and documents it; the new
    elision is the trace mask above.

- **gc-P6 — Concurrent marking + STW survivor evacuation.** Collector
  thread, single-mutator handshake, SATB log becomes live. **Gate: marking off
  the mutator; STW time independent of live-set size; stress-differential
  green.**

- **gc-P7 — Fully concurrent evacuation (optional).** Brooks forwarding +
  load barrier, only if Phase 6's pause numbers say so.

Phases 0–4 deliver the generational mover with no threads and no value-rep
change; 5 fuses the collector with the object-model future; 6–7 buy pause
bounds as needed.

## Risks, named

- **Pin rate before precision lands.** Phases 2 runs with conservative roots;
  if the young-gen pin rate is high (plausible: env interior pointers, `-O2`
  register pressure), premature promotion erodes the win until Phase 3. Phase
  0 measures this *first*; if it's bad, Phase 3 moves ahead of Phase 2's gate
  being declared, or ships together with it. Cell-granularity pinning bounds
  the damage to pinned bytes either way.
- **Store-to-load forwarding across safepoints** (Phase 3) — the silent-
  corruption class. Mitigated by the escape discipline and killed dead by the
  move-everything stress mode, which must exist before the first relocating
  root does.
- **The header widening is a coordinated cross-language change.** Runtime
  structs and `lib/types.ts` move together or compiled code reads garbage.
  Atomic land, old collector active, all three bootstrap targets.
- **Spill overhead at safepoints** (Phase 3). Live-across-call values get
  stores/reloads LLVM might otherwise have kept in callee-saved registers.
  Expected small (safepoints are call sites; calls spill anyway); measured at
  the Phase 3 gate, and allocation-free functions opt out entirely.
- **`-O2` runtime and scanner assumptions.** The conservative scanner was
  hardened against `-O0`-runtime/`-O2`-mutator asymmetry; flipping the runtime
  to `-O2` re-opens those assumptions. Phase 0 owns re-verifying them.
- **Objective-C compilation on macOS** — collector sources compile as ObjC in
  `runtime/BUCK`; keep them clean C.
- **Single-mutator shortcuts leaking past their seams.** Non-atomic barriers,
  the global bump pointer, and one-handshake safepoints are deliberate
  exploitations of today's engine — but concurrent JS is a goal, so each must
  stay behind the §"Concurrency II" seams (heap-context struct, context
  accessors, atomics-friendly metadata). The cheap discipline is refusing new
  file-static collector state; the expensive mistake would be a barrier or
  forwarding protocol that is single-mutator-only *by construction*.

## Alternatives considered

- **Keep mark-sweep, add a non-moving generational layer.** Cheaper, gets
  minor-pause wins, no compaction ever — fragmentation and cache locality stay
  bad, and the shapes future wants copyable objects. The Phase 2 substrate
  costs only modestly more; not worth the dead end.
- **Full exact rooting including the C runtime (handles everywhere).**
  The SpiderMonkey migration; years of churn across hundreds of native
  functions for a pin population that pinning already bounds. No.
- **Native LLVM statepoints now.** Requires un-NaN-boxing or the P3.6 hybrid
  value representation. The gc-frame design gets relocatable precision without
  it; statepoints remain the far-future upgrade for typed values, and nothing
  here blocks that.
- **MMTk (or another off-the-shelf collector).** The serious outside option;
  its binding model fits AOT runtimes. But it wants exactly the root precision
  and barrier plumbing this plan builds anyway, and adopting it forfeits reuse
  of the existing precise `Scan` ops and battle-tested conservative scanner.
  Reconsider if the hand-rolled collector stalls at Phase 4+; the compiler-side
  work (roots, barriers, inline alloc) transfers either way.

## Coordination with maam-plan / plans.md

- **maam P3.6 (specialization)**: raw f64s in registers/typed signatures are
  invisible to GC (not listed in gc-frames) — correct by construction. Typed
  slots later enable barrier/trace elision.
- **maam P4 (shapes)**: joint design of header bits, trace bitmaps, inline
  slots, born-with-shape allocation (this plan's Phase 5). The P4 design doc
  should be written against the Phase 1 header layout.
- **Oracle pretenuring**: allocation-site lifetimes → old-gen birth; consumes
  Phase 2 infrastructure.
- **plans.md escape analysis / allocation sinking**: measured jointly with
  Phase 0's allocation profile; sinking shrinks nursery pressure, the nursery
  cheapens what remains.
- **IR-in-manifest (cross-module)**: whole-program oracle facts strengthen
  pretenuring and barrier elision; no GC dependency.

## Phase checklist (for /goal sessions)

- [x] **gc-P0** generator-stack fix; alloc/survival/pin instrumentation (optimizer
      on); `-O2`-runtime experiment + scanner re-verification.
      *Gate:* matrix green; numbers recorded in this doc or a results doc.
      DONE 2026-07-24 — docs/gc-p0-results.md has the numbers.  Headlines:
      four latent generator-scan bugs fixed (collection-on-generator-stack
      segfaulted; the suspended-stack scan was INVERTED — dead region
      scanned, live frames missed) + LOS interior-pointer tolerance;
      profile: self-compile = 79.5M allocs/3.4GB, 39% closureenv,
      steady-state young survival 2.4-3.4% of bytes, pins 380-650
      objects/cycle (KBs — conservative pinning is a non-issue, so P2
      proceeds WITHOUT P3); runtime `-O2` landed: self-compile 127s→42s
      (3.06×), types-bench2 2.00s→0.68s.
- [x] **gc-P1** 64-bit header (+ reserved shape/trace bits) + `lib/types.ts`
      lockstep; forwarding helpers.
      *Gate:* matrix green, all three bootstrap targets.
      DONE 2026-07-24.  The header half landed 2026-07-23 as the joint
      shapes-P4.1 atomic change (u64 header, shape bits 32-55, mode bit
      56, lib/types.ts as two i32 halves); this phase added the
      remainder: bit 59 = FORWARDED + first-word-overwrite forwarding
      record (target address in bits 0-46 — the sub-2^47 NaN-box rule
      makes the discriminator unambiguous), read/write helpers in
      ejs-gc.h (`_ejs_gc_is_forwarded` / `_ejs_gc_forwarding_addr` /
      `_ejs_gc_forward`), inert until gc-P2 and exercised by
      EJS_GC_SELFTEST=1 at init; ejs-types.h now documents the complete
      bit inventory (57 YOUNG / 58 PINNED from P0 profiling, 60-63
      still free for mark/card).  Local matrix ×7 green; linux targets
      ride the standing CI bootstrap matrix on push.
- [x] **gc-P2** nursery + inline `make_env` allocation + write barrier +
      evacuating minor GC w/ cell pinning; old collector behind a flag
      (`EJS_GC_NURSERY=off`), differential + stress lanes; heap-context
      struct + context-accessor seam from the first line of new code.
      *Gate:* alloc throughput ↑; minor p99 < 1 ms; differential green; pin
      report; zero new file-static collector state.
      DONE 2026-07-25 — docs/gc-p2-results.md has the numbers.  Headlines:
      object-remembering barrier (slot-address remset abandoned — dangling
      recorded slots in freed malloc storage); nursery ON by default;
      bench2 0.69→0.64s, envbench 1.82→1.48s, self-compile parity at 39s;
      minor p99 0.68ms @512KB budget (1MB default = 1.27ms); the
      conservative-lookup bounds prefilter that fixed two lookup
      pathologies also sped the OLD collector's full marks (43.4→39.3s
      self-compile).  The war story: unrooted ejsval C statics in
      ejs-llvm bindings — under a mover, roots exist to REWRITE
      locations, not just keep referents alive.  Deviation from the plan
      line: no card table and no initializing-store elision — the
      object-remembering DIRTY bit dedups repeat stores and modules/LOS
      are handled by unconditional scan / born-dirty instead.
- [x] **gc-P3** gc-frame precise JS roots (chained variant) + env slot-address
      inlining; move-everything stress mode.
      *Gate:* stress green; pin-rate delta + spill-cost numbers recorded.
      DONE 2026-07-25 — docs/gc-p3-results.md has the numbers.  Headlines:
      slot DEMOTION (store at def, load per use) rather than
      spill/reload — dominance-safe by construction, forwarding-safe
      because the frame escapes through the chain; per-stack chains
      swapped by the generator hooks; pin-first ordering (a C-visible
      object must not move).  The bug measurement caught: the
      conservative scan pinned every frame-held value through its own
      stack-resident slot — minors now skip the scanned stack's frame
      records.  76k relocations/self-compile, pins p50 373→101, net
      wall cost ~+1% (env slot inlining pays back half the frame cost).
      Deferred: invoke-form safepoints stay pinned; stackmap variant
      unmeasured.
- [x] **gc-P4** mostly-copying major compaction + auto-tuned growth target.
      *Gate:* heap shrink demonstrated; knob census = 1.
      DONE 2026-07-26 — docs/gc-p4-results.md has the numbers.  The
      pin-scan cliff died first (the "first order of business"): one
      PROT_NONE arena reservation at init + direct-map arena lookup +
      LOS sorted-range bsearch; the A/B was brutal (baseline binary
      stuck in the slow mode: >13 MINUTES for the self-compile the
      fixed binary does in ~60s, sampled ~95% inside
      mark_ejsvals_in_range→find_page_and_cell; fixed binary: 4 runs
      within 62-64s, minor pause max 10.9ms, GC ≈ 10% of wall).
      Compaction: conservative hits + registered generators set PINNED;
      post-sweep sparse-first evacuation with the source set chosen
      COMPLETELY before any evacuation (one-pass selection let an early
      destination later become a source via its stale live count);
      fixup = roots/modules/gc-frames/remset + all live cells.  Shrink
      gate: frag bench 37.6→8.3MB (4.5×), idempotent second collect;
      self-compile full GCs free ~5k pages each.  Growth target:
      full_gc_trigger() = EJS_GC_GROWTH% (default 50) of post-sweep
      footprint, 2-arena floor; knob census = 1.  EJS_GC_COMPACT=off
      for A/B.  Drive-bys: LOS tail-page leak on free;
      young-survivor-page full-sweep list corruption (young_page_freed).
- [x] **gc-P5** shapes intersection (sequenced by maam P4): trace bitmaps, inline
      slots, object-literal inline allocation, typed-slot elisions.
      DONE 2026-07-28 — docs/gc-p5-results.md has the numbers.
      Headlines: single-cell shaped objects (embedded slots behind the
      unchanged obj->slots ejsval; pointer-identity mode test; ctor
      birth-capacity hint on EJSFunction), barrier owner flipped to
      the wrapper object with Scan walking slot values, per-shape
      f64 trace masks, born-shaped literals extended to flag-off, and
      the 256-byte size class enabled (LOS allocs −69% on a compile
      workload).  litbench 1.84×, bench2 cells halved; emitted bump
      allocation for literals measured at ~6% of an alloc-heavy loop
      and deferred on that evidence (the ctor sink + hint already
      cover construction).  Matrix ×7, stress envs, and the --types
      diff lane (475 files, 0 divergent) green.
- [ ] **gc-P6** collector thread: concurrent mark (SATB) + STW survivor
      evacuation.
      *Gate:* STW independent of live-set size.
- [ ] **gc-P7** (optional) Brooks + load barrier for concurrent evacuation —
      only on Phase 6 evidence.
