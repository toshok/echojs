# GC plan: from conservative mark-sweep to a moving generational collector

A proposal for replacing echojs's stop-the-world conservative mark-and-sweep
collector (`runtime/ejs-gc.c`) with a generational, compacting, *mostly-copying*
collector — in independently-landable phases, each of which leaves the tree
green and shippable.

This document is deliberately honest about which of the requested attributes are
reachable cheaply, which are reachable expensively, and which are in tension
with decisions already baked into the engine (chiefly NaN-boxing and a
`-O2`-compiled mutator with values living in registers). The headline: **you can
have moving + generational + low-pause without precise stack maps, and given
this codebase that is almost certainly the right trade.** The path that *sounds*
like what you asked for — "precise, via LLVM stackmaps" — is a dead end here for
a concrete reason the codebase already discovered, and I'll show why.

## What we have today, as found

- **Collector** (`runtime/ejs-gc.c`, 1676 lines): stop-the-world, single-threaded
  ("very simple stop the world collector", `ejs-gc.c:1123`), tri-color
  mark-and-sweep, **non-moving**. Trigger is 60 MB of allocation since the last
  cycle (`ejs-gc.c:1408`), plus `GC.collect()`, allocation-failure fallbacks,
  and shutdown.
- **Allocator**: segregated free-lists in size classes 16–256 bytes over 32 MB
  arenas, with a per-page bump pointer for fresh pages (`alloc_from_page`,
  `ejs-gc.c:1305`) and a large-object store (LOS) for anything `> 256` bytes
  (`alloc_from_los`, `ejs-gc.c:1352`). Mark bits are a per-cell bitmap in each
  `PageInfo` (`ejs-gc.c:341`).
- **Heap is already precisely traceable.** Every object carries a per-class
  `Scan` spec-op (`ejs-object.h:161,190`) and there are typed scanners for
  strings, symbols, and closure environments (`_scan_from_ejsprimstr` etc.,
  `ejs-gc.c:755-789`). The collector already knows the exact outgoing edges of
  every heap object. This is the single most important asset we have.
- **Roots are *not* precise.** Three sources:
  1. an explicit root set — a linked list of `ejsval*`, ~142 registrations,
     almost all static global singletons (85 in `ejs-init.c` alone), plus 11
     genuinely dynamic ones in `ejs-promise.c:61-116` (`ejs-gc.c:188-193`,
     `mark_from_roots` at `:960`);
  2. module exotics, scanned from a static array (`mark_from_modules`, `:988`);
  3. **a conservative scan of the C stack and spilled registers**
     (`mark_thread_stack`, `:1060`; `MARK_REGISTERS`, `:1005-1058`), which reads
     every stack/register word and treats anything that *looks* like a heap
     pointer — tagged ejsval *or* raw untagged pointer, **including interior
     pointers** — as a root.
- **Value representation**: SpiderMonkey-style NaN-boxing (`runtime/ejsval.h`).
  A GC pointer lives *inline* in the low 47 bits of an 8-byte value
  (`EJSVAL_TO_GCTHING_IMPL`, `ejsval.h:929`); all heap addresses are forced
  below 2⁴⁷ (`mmap_boxable`, `ejs-gc.c:199`).
- **Compiler emits no GC support at all.** The eir backend (`lib/eir/emit.ts`)
  keeps JS locals as pure SSA values in registers — "locals never touch memory"
  (`emit.ts:9-11`); the only allocas are the outgoing-arg scratch area and a
  `&this` slot. There are **no** statepoints, stackmaps, gcroots, safepoints, or
  GC address-spaces. `lib/abi.ts:34-46` / `lib/compiler.ts:402-406` contain dead
  `llvm.gcroot` code with a comment explaining why it was abandoned (below).
- **No write barriers anywhere** (`runtime/`, `lib/` — nothing). No card table,
  no remembered set, no handle/`Rooted<>` abstraction.
- **Single-threaded.** No `pthread_create`, no workers, no TLS; the GC's
  `LOCK_*` macros are all empty no-ops (`ejs-gc.c:100-105`). One event loop on
  the main thread.
- **Build**: Buck2, Homebrew LLVM (nominally 16), runtime compiled `-O0`, user
  JS compiled `-O2`. A new collector `.c` goes in `runtime/BUCK`'s
  `shared_sources` (`:50-88`) and must compile as Objective-C on macOS.

## Your eight attributes, scored honestly

| # | Requested | Verdict |
|---|-----------|---------|
| 1 | Per-thread bump nursery; large → old gen | **Nursery: yes.** "Per-thread" is moot — the engine is single-threaded. Build a single-mutator bump nursery; route LOS-sized allocations straight to old gen. |
| 2 | Precise, via LLVM stackmaps | **Partly — and more hopefully than "no".** `gcroot`/`gc.statepoint` are out (they need reference-typed values; NaN-boxed ejsvals are `i64`). But `llvm.experimental.stackmap` records i64 locations and *does* work with NaN-boxing — fork-free precise **marking/pinning** roots, though not relocatable ones. Relocatable precise roots need either a value-rep change or a manual shadow stack. Full treatment in §"The LLVM precise-root question". |
| 3 | Fastest old→young write detection | **Yes: card marking + a generational store barrier.** Must be built from scratch and threaded through both the runtime stores and the compiler's store sites. The effect table already labels every store `E.WRITE` (`ops.ts:17`), which is where the barrier goes. |
| 4 | Moving | **Yes — via mostly-copying.** This is the core proposal. |
| 5 | Concurrent (collector runs alongside the mutator) | **First-class goal, staged.** The mutator stays single-threaded; the *collector* gets its own thread. Realistic first target: **concurrent marking** (SATB barrier) + a brief stop-the-world evacuation of survivors; then fully concurrent evacuation (forwarding/load barrier). A single mutator makes safepointing tractable. See §"Concurrent collection". |
| 6 | Extremely low pause | **Yes, via #5.** Generational alone makes minor pauses sub-millisecond (nursery + remembered set only). "Extremely low regardless of live set" comes from concurrent marking + concurrent evacuation — designed toward from Phase 3's barrier, not bolted on. |
| 7 | Competitive with V8 / SpiderMonkey | **Not gated by the collector.** echojs has no inline caches and uses per-object hash-map property stores (`_EJSPropertyMap`, `ejs-object.h:128`); it's AOT, not a JIT. GC is not its bottleneck and a great GC won't make it competitive. A generational mover *will* make allocation and collection cost competitive; the engine overall won't be. Said plainly so the effort is aimed right. |
| 8 | As few knobs as possible | **Yes, and easy to hold to.** One auto-tuned heap-growth target, no generation-size zoo. See §Knobs. |

## The root problem: why conservative roots and moving are in tension

A moving collector must find and *rewrite* every pointer to a moved object. The
heap edges we can already enumerate precisely. The **roots** we cannot, and here
is the bind:

1. **NaN-boxing puts raw pointers inside `i64` values.** To use LLVM's precise
   GC (`gcroot` / `gc.statepoint` + `RewriteStatepointsForGC`), a GC reference
   must be a *reference-typed* SSA value (`ptr addrspace(1)`). An ejsval is an
   `i64` that is *sometimes* a pointer and sometimes a double/int/bool. You
   cannot hand LLVM an `i64` and ask it to relocate it. The codebase already hit
   this wall — the `gcroot` call in `abi.ts:34-46` is commented out with exactly
   this reasoning ("with the nan boxing we kinda lose out as the llvm IR code
   doesn't permit non-reference types to be gc roots"). Using LLVM's machinery
   would mean **un-NaN-boxing the value representation** — a change that touches
   every file in `runtime/`, throws away the boxing's speed and density, and is
   not on the table.

2. **Even if JS frames were precise, the C runtime isn't.** Every
   `EJS_NATIVE_FUNC` (`ejs.h:100`) holds bare `ejsval` locals across allocation
   points — `Array.prototype.map` keeps `O`, `A`, `kValue`, … live across a loop
   that allocates every iteration (`ejs-array.c:1171-1225`), and this pattern is
   pervasive. There is no handle/`Rooted<>` scope anywhere (`§4` of the survey).
   Making these precise means introducing a handle API and rewriting hundreds of
   runtime functions to use it — the SpiderMonkey "exact rooting" migration,
   which took Mozilla years.

So precise rooting is not one project; it's two large ones (de-box the compiler
*and* handle-ize the runtime), and the first is foreclosed by NaN-boxing.

**The resolution is to stop fighting it.** A collector can be moving *without*
precise roots if it can tolerate a set of ambiguous, un-rewritable references —
by refusing to move exactly the objects those references point at. That is
Bartlett's *mostly-copying* collector, and it is a near-perfect fit for an
engine that already does conservative scanning and already has precise heap
tracing.

The rest of the "can't we just make roots precise?" question — including your
specific asks about bending `gcroot` and about typing ejsval as a reference — is
answered in full in the next section before we get to the collector proper.

## The LLVM precise-root question, in full

You asked whether we can bend `llvm.gcroot`, add our own mechanism without
forking LLVM, or type ejsval as a reference instead of an `i64`. Here is the
full lay of the land — more hopeful than the one-line "dead end", with two sharp
caveats.

### How the machinery actually works (nothing moves on its own)

Worth pinning down first, because it's the crux of the "why not just type it as a
reference?" question: **LLVM has no GC runtime, and address spaces are not regions
of your heap.** `addrspace(1)` is a compile-time *type tag* meaning "this pointer
is a GC-managed reference" (the address space `RewriteStatepointsForGC` treats as
GC is a convention, conventionally 1). It does not correspond to
from-space/to-space, nursery/old-gen, or any physical region — those are entirely
your runtime's concept, invisible to LLVM. Nothing is "automatically relocated
between spaces."

All actual moving and pointer-rewriting is done by **your collector code**.
LLVM's whole contribution is at compile time, three things:
1. **identify** which live SSA values are GC references (by their `addrspace(1)`
   type) at each safepoint;
2. **emit a stackmap** recording *where* each live reference sits (register or
   stack slot) at that safepoint, into a section your runtime parses;
3. **insert `gc.relocate`** so that after a safepoint the compiled code re-reads
   each GC pointer from its (possibly collector-updated) slot instead of reusing
   a stale copy it was holding in a register.

Runtime loop: mutator hits a safepoint → your collector walks the stackmap and,
per slot, reads the pointer, moves the object, writes the new address back → the
`gc.relocate`-lowered code reloads the updated pointer. Step 3 is the entire
reason statepoints exist and why "just scan the stack" can't *move*: without a
forced reload the compiler could keep a pre-move pointer in a callee-saved
register across the call, and your slot update would be silently ignored.

So — *can't the collector just interrogate each value and skip the NaN-boxed
scalars?* On the read side, **yes**: the collector is your code and can decline to
move any slot. The breakage isn't there; it's the compile-time contract, next.

### What LLVM offers, and why each does or doesn't fit an i64 ejsval

**1. `llvm.gcroot` (classic shadow-stack intrinsic).** Signature is
`@llvm.gcroot(ptr %ptrloc, ptr %metadata)` where `%ptrloc` must be an *alloca of
pointer type*; a registered `GCStrategy` (e.g. `ShadowStackGC`) threads those
slots onto a list. It deals in **pointer-typed stack slots**. An ejsval is an
`i64` that is only *sometimes* a pointer; you can't hand `gcroot` an `i64` slot,
and bitcasting makes a boxed double's bits into a bogus "root." This is exactly
the wall the abandoned code in `lib/abi.ts:34-46` hit. Unusable as-is.

**2. `llvm.experimental.gc.statepoint` + `gc.relocate` (the moving-GC path).**
GC references are `ptr addrspace(1)`; calls become statepoints;
`RewriteStatepointsForGC` inserts `gc.relocate` so every post-call use reloads
the possibly-moved pointer. The relocate is the crucial part — it's the *only*
thing in LLVM that makes moving-through-the-stack sound, because it forces the
reload. But GC values must be **real pointers**: incompatible with polymorphic
i64 ejsvals.

**3. `llvm.experimental.stackmap` / `patchpoint` (the sleeper — works with
NaN-boxing).** `stackmap(i64 id, i32 shadow, ...live values...)` records the
**locations** (register or stack slot) of arbitrary-typed operands — **including
`i64`** — into an `__LLVM_StackMaps` section. So you list the live ejsvals at
each safepoint; the collector parses the map, reads each location, applies the
NaN-box tag test, and gets a **precise root set** with none of conservative
scanning's false positives (integers that look like pointers, dead slots,
interior-pointer ambiguity). No fork, no un-boxing. **The caveat:** a stackmap
*records* a location, it does not *relocate*. LLVM still treats the SSA value as
invariant — it may keep copies in other registers, rematerialize, or CSE it — so
overwriting the recorded slot is not guaranteed to be seen by every later use.
Hence stackmaps give precise **marking/pinning**, not general stack relocation.
(Closing that gap is exactly what statepoints are for.)

**4. Custom `GCStrategy` + `GCMetadataPrinter` (fork-free extension points).**
You can register your own strategy and stackmap emitter in-tree, controlling
safepoint placement and map *format*. What this does **not** change is the type
discipline — a strategy still consumes `gcroot`/statepoint-shaped IR. Older LLVM
exposed `GCStrategy::performCustomLowering` to rewrite `gcroot`/`gcread`/`gcwrite`
yourself; it was deprecated/removed as statepoints took over and isn't a stable
base in LLVM 16. GCStrategy customizes *emission*, not *semantics*.

**5. "Adding our own" mechanism.** A genuinely new *intrinsic* means editing
LLVM's tablegen — a fork; don't. The fork-free equivalent: mark safepoints with
a convention (a call to a known symbol, or `stackmap`) and run an **out-of-tree
LLVM pass** (loadable via the pass-plugin interface — no fork) that does the
lowering using the frontend's own liveness/type info to select the GC roots.
That pass can either emit `stackmap` intrinsics or spill live GC values into a
frame you control (a shadow stack). Both are fork-free and both work with i64.

### Could ejsval be a *reference type* instead of an i64?

Directly to your follow-up: **not uniformly, and not without changing how it's
used.** The subtlety is worth stating precisely, because it is *not* "LLVM will
relocate a double behind your back" — per the mechanics above, LLVM moves nothing
and your collector can inspect any slot and skip the scalars. The real
incompatibility is between two contracts on the same 64 bits:

- **NaN-boxing needs an integer view.** Every tag test and every unbox of a
  double/int32 is integer bit-twiddling on the value.
- **A `ptr addrspace(1)` value may only be touched as a relocatable pointer.** To
  bit-twiddle it you must `ptrtoint`, and the integer you extract is invalidated
  the instant a collection moves the object (its address changed; your captured
  integer didn't). In a *non-integral* address space (`ni:` — the mode that
  exists precisely for tagged/boxed pointers), `ptrtoint`/`inttoptr` aren't even
  meaningful bit-preserving ops, so you can't NaN-box in it at all. Separately,
  the optimizer may assume pointer semantics (e.g. two bitwise-equal addrspace(1)
  values denote the same object) that NaN-boxed scalars violate.

So a value can be *a thing LLVM relocates* **or** *a thing you NaN-box*, not both.
Soundness requires an invariant NaN-boxing violates: *every value of the
GC-reference type is actually a pointer, touched only as a pointer.*

Three real options follow from that:

- **Non-integral address space (`ni:`) — keeps the bits, doesn't buy moving.**
  LLVM lets you mark an address space *non-integral* so the optimizer won't do
  `inttoptr`/`ptrtoint` round-trips or assume the pointer's bits equal an integer
  address (Julia carries GC refs this way). You *can* thus carry a tagged/boxed
  value as a pointer type without the optimizer miscompiling it. But it does
  **not** teach `RewriteStatepointsForGC` that some of those "pointers" are
  really doubles — so it does not make the polymorphic case safe to relocate. It
  helps only once references are already split out from immediates.

- **Split the representation (tagged pointers) — the sound way to get a reference
  type.** Make *references* (object/string/env) real `ptr addrspace(1)` values
  and *immediates* (int31/bool/null/undefined) non-pointer, with doubles either
  boxed or kept on a NaN-box side-path. Now the GC pointer type genuinely only
  ever holds pointers, statepoints work, and you get LLVM-managed moving. This
  **does** change "our use of it": doubles no longer share the pointer slot via
  NaN tricks. It's the object-model change in §"Object-model changes" (value
  representation), and it's the only way to a uniformly reference-typed ejsval.

- **Type-split via the oracle (hybrid) — reference type where it's provably a
  reference.** Where the maam type-oracle proves a value is an object/string/env,
  represent *that value* as `ptr addrspace(1)` and let statepoints relocate it
  precisely; where the type is `any` or number, keep the i64 NaN-box and
  pin/shadow-stack it. ejsval stops being one uniform LLVM type and becomes
  representation-selected per value — a bigger compiler change, but exactly what
  the planned specialization work (maam-plan Phase 3.6) already sets up. This is
  the most sophisticated end state and the one that most directly grants your
  original "precise via LLVM" wish, for the typed fraction of the program.

### The three fork-free destinations, ranked for echojs

- **(i) Precise *pinning* roots via `stackmap` — recommended upgrade to
  mostly-copying.** Keep NaN-boxing. The frontend (which already tags safepoints
  via the `E.GC` effect, `ops.ts:15`) emits `llvm.experimental.stackmap` listing
  live values; the collector reads exact roots and pins their blocks — strictly
  fewer pinned blocks than conservative scanning, no C-runtime changes, no
  un-boxing. Doesn't let stack-rooted objects *move*, but in mostly-copying they
  don't need to. The natural precision upgrade if Phase-0 says conservative
  pinning is too coarse.
- **(ii) Precise *relocatable* roots via a manual shadow stack — keeps
  NaN-boxing, enables moving through the stack.** The frontend spills live
  GC-typed ejsvals into an explicit per-frame struct (chained thread-wide) at
  safepoints and reloads after. Because the values live in *memory you own*, the
  collector rewrites them and the mutator reloads the moved pointer — relocation
  without statepoints and without un-boxing. Cost: spill/reload at safepoints for
  live GC values (gives up "locals never touch memory" *at safepoints only*), and
  the C runtime needs handle scopes to participate or stays pinned.
- **(iii) Precise relocatable roots via native statepoints — needs the value-rep
  split above.** Only reachable once GC references are real addrspace(1)
  pointers. Then LLVM does the relocation bookkeeping, and it composes with the
  oracle hybrid.

The through-line: **you need neither a fork nor the end of NaN-boxing to get
*precise* roots — only to get *LLVM-managed relocatable* roots.** Precise pinning
(stackmaps) and precise relocation-via-shadow-stack are both fork-free and
NaN-box-compatible; native statepoint relocation is the only option gated on the
value representation.

## The proposal: a mostly-copying generational collector

### Core idea (Bartlett, 1988; generational variant Bartlett 1989)

- Partition the heap into **pages/blocks** owned by a space (nursery, old gen).
- **Roots are still scanned conservatively** — C stack, spilled registers,
  `-O2` JS frames, the existing root set, exactly as today. Each ambiguous root
  that resolves into a heap block **promotes that block in place**: the block is
  logically moved to to-space *without relocating its contents* ("pinned" this
  cycle). This is cheap — it's a flag flip on the block, not a copy.
- Every **precisely-known** reference (heap-internal edges via the `Scan` ops,
  and the precise root set) is **evacuated**: the target object is copied to a
  fresh to-space block and a **forwarding pointer** is left behind; the
  referring slot is rewritten to the new address.
- Because the heap is precisely traceable, all *heap→heap* edges get rewritten.
  Because roots are conservative, all *root→heap* edges pin rather than move.
  Both are sound; the only cost of conservatism is a little floating garbage and
  some un-compacted (pinned) blocks per cycle.

This gives you attributes **1, 3, 4, 6, 8** with **zero compiler root changes**
and **zero rewrite of the C runtime**. It degrades gracefully: in the worst case
(everything pinned) it's a non-moving mark-sweep, i.e. no worse than today.

### Why it fits echojs specifically

- The conservative scanner already exists and is battle-tested, including the
  nasty parts — interior pointers into closure-env slots, raw unboxed pointers
  in registers, callee-saved SIMD spills (`ejs-gc.c:823-833, 1016-1041`). We
  *keep* all of it; it becomes the "block pinning" oracle instead of the "mark
  everything" oracle.
- Precise heap tracing already exists (`Scan` ops). Evacuation reuses it almost
  verbatim: where the mark phase today grays a target, the moving phase copies
  it and updates the slot.
- The mutator is single-threaded, so there's no safepoint-coordination or
  read-barrier problem for the moving itself — the world is already stopped
  inside `_ejs_gc_alloc`.

### Heap architecture

```
        ┌───────── nursery ─────────┐   ┌──────────── old gen ────────────┐
alloc → │ bump pointer, one block   │   │ block-structured, per-block      │
        │ chain; small objs only    │   │ pin/age/mark metadata; compacted │
        └───────────────────────────┘   │ by evacuation of unpinned blocks │
                                         └──────────────────────────────────┘
                          ┌──── LOS ────┐
  size > threshold  ───→  │ mmap'd, never moved, per-object header │
                          └──────────────────────────────────────┘
```

- **Nursery**: a chain of blocks allocated by a single bump pointer
  (generalizing the existing per-page `bump_ptr`, `ejs-gc.c:1317`). Fast path is
  pointer-increment + limit-check, inlinable. Objects larger than a block
  fraction, and all current LOS-sized objects, bypass the nursery and are born
  in old gen / LOS (your attribute 1).
- **Minor collection** evacuates live nursery survivors into old-gen blocks,
  using the **remembered set** (old→young pointers) plus the conservatively
  scanned roots as its root set. Ambiguous roots pin at **cell** granularity (not
  whole pages — see below); movable survivors, *including neighbors sharing a
  block with a pinned cell*, are evacuated out; a block that contained no pins is
  returned wholesale, and a block that contained one becomes an old-gen free-list
  block holding just the pinned cell(s).
- **Major collection** traces the whole heap; unpinned old-gen blocks are
  evacuated/compacted, pinned blocks are swept in place (mark-sweep fallback for
  exactly the blocks conservatism forces).
- **Reuse the arena/block machinery** already in `ejs-gc.c` (`Arena`, `PageInfo`,
  `alloc_page_from_arena`) — the space partitioning is a relabeling and a
  metadata extension of what's there, not a from-scratch allocator.

### Forwarding pointers and the object header

`GCObjectHeader` is a bare `uint32_t` today (`ejs-types.h:30`): scan-type in the
low 16 bits, user flags in the high 16 (`ejs-gc.h:21-23`). A mover needs, per
object: a forwarded bit + a place to stash the forwarding address, plus (for
generational) an age and a card/log bit.

- **Standard Cheney trick needs no extra space**: copy the object to to-space
  *first*, then overwrite the from-space copy's first word with the forwarding
  pointer (low bits are free — objects are 8-aligned) and a forwarded tag. The
  original first word is already safely in to-space. So forwarding itself is
  free.
- **Widen the header to 64 bits anyway**, for age/pin/mark/log bits and to keep
  the mask bookkeeping sane. For `EJSObject` this is *free*: `gc_header` (4B) is
  today followed by 4B of padding before the 8-byte `ops` pointer, so widening
  to 8B leaves `ops` at offset 8 and **shifts no other field** — critically,
  compiled code's view of object layout (`lib/types.ts`) is unchanged. For
  `EJSClosureEnv` (`gc_header` then `uint32_t length`, `ejs-closureenv.h:9-13`)
  and `EJSPrimString` (`gc_header`, `length`, `hash`, `ejs-string.h:77-95`) the
  second word shifts; those two layouts and their compiler mirrors must move in
  lockstep. This is mechanical but must be a single atomic change.

### Pinning granularity, and the premature-promotion trap

The obvious failure mode of mostly-copying: a single pinned object drags an
entire page. Classic Bartlett *does* pin at page granularity — it flips the
page's space id (a metadata operation, no copy — promotion is cheap; that is the
whole appeal) and leaves everything on the page in place, dragging live-but-
movable neighbors *and* short-lived garbage into the older generation. Freshly
allocated objects are the ones most likely to be live in registers / on the stack
at a collection, so this hits the **nursery hardest**, exactly where premature
promotion is most wasteful.

echojs is not stuck with page granularity. The allocator is **segregated with
fixed per-page cell sizes**, and `find_page_and_cell` / `PTR_TO_CELL`
(`ejs-gc.c:78, 519-560`) already resolves any interior pointer to an exact cell.
So an ambiguous root can pin **the cell**, not the page: evacuate every movable
survivor (including neighbors of a pinned cell) out, leave only the pinned cells,
and convert the block to an old-gen free-list block whose other cells rejoin the
allocator (the current allocator is already free-list based). Pinning then costs
**pinned bytes**, not **blocks-touched × page-size**.

Two residual costs remain and must be measured, not hand-waved:
1. **Pinned objects tenure early and don't compact** this cycle — wasteful if they
   were about to die.
2. **Any block with ≥1 pinned cell can't be wholesale bump-reset**, so it leaves
   the clean-nursery fast path; enough scattered pins and you accumulate
   partially-full retained blocks. Track a *retained-block count*, not just a
   pinned-byte count.

This is the strongest argument for the stackmap-precision upgrade (destination
(i)), and a *targeted* one: precise roots matter most for the **young generation**,
because that's where conservative false-positives translate directly into
premature promotion. (Retaining a pinned block in the young gen and retrying next
cycle avoids early tenuring but forfeits the clean bump-reset and adds holey young
blocks — usually not worth it; promote-and-move-on is simpler and pinned objects
mostly would have survived the minor cycle anyway.)

### Moving each object kind

- **EJSObject**: copy the struct; the out-of-line `EJSPropertyMap`
  (`ejs-object.h:128`) is malloc'd, *not* in the GC heap — leave it in place, it
  moves with nobody. Its contained ejsvals are already visited by the object's
  `Scan` op and get rewritten there. (Longer term the property map is a good
  candidate to pull into the GC heap so it compacts too, but not required.)
- **EJSClosureEnv**: copy header + `length` + `slots[]`; rewrite each slot.
  Watch the **interior-pointer** case — optimized code holds
  `_ejs_closureenv_get_slot_ref` addresses (`emit.ts:624-641`) with the env base
  dead. A raw interior pointer to a slot is an *ambiguous root* → it pins the
  env's block. Correct and safe; costs a little compaction. (This is the single
  biggest source of pinning and worth measuring early.)
- **Strings** (`ejs-string.h`): flat strings with an inline buffer copy fine;
  flat strings with an **out-of-line** buffer (`EJS_PRIMSTR_HAS_OOL_BUFFER`) keep
  the malloc'd buffer in place and just carry the pointer. Ropes and dependent
  strings hold `EJSPrimString*` children — rewrite them like any other edge
  (already enumerated by `_scan_from_ejsprimstr`, `ejs-gc.c:755`).
- **LOS**: never moved (as today). Ambiguous roots to LOS objects are a no-op
  beyond marking.
- **Generators**: today their stacks are **not scanned at all**
  (`mark_generator_stacks` is a stub, `ejs-gc.c:1086`) — a latent correctness
  bug. A suspended generator frame holds live ejsvals; under a mover it must at
  minimum pin whatever it references. This must be fixed *before* moving, or
  generators will corrupt. Flagged as a Phase-0 prerequisite.

### Write barriers and the remembered set

Generational collection requires catching **old→young** stores so minor
collections can treat old-gen writers as roots without scanning all of old gen.

- **Mechanism**: card marking. Divide old gen into cards (e.g. 512 B); a store
  into an old-gen object dirties its card; minor collection scans only dirty
  cards. Card marking is the cheapest known barrier (an unconditional shift +
  byte store), which is why you asked for it and why it's the right call.
- **Where the barrier goes** — echojs makes this unusually clean because the
  effect table already classifies every store as `E.WRITE` (`ops.ts:17`):
  - **Runtime stores**: `_ejs_object_setprop` and the propertymap insert path
    (`ejs-object.c`) — one barrier at the store point covers most object writes.
  - **Compiler-inlined stores**: env-slot stores (`emit.ts:639`) and module-slot
    stores (`emit.ts:600`) are raw `store`s with no runtime call — the emitter
    must emit a card-dirty alongside each. These are the only inlined ejsval
    stores, so the surface is small and enumerable.
  - Array element stores go through `_ejs_object_setprop` today, so they're
    covered by the runtime barrier until/unless arrays get a fast path.
- **Simplification for a single-threaded mutator**: the barrier is a plain
  (non-atomic) card store — no CAS, no fences. This is a real, permanent win
  from being single-threaded; keep it until/unless concurrency (#5) forces
  atomics.

### The root scan, reused verbatim

Nothing about the conservative scan changes in structure. `mark_pointers_in_range`
/ `mark_ejsvals_in_range` (`ejs-gc.c:799-876`) already resolve an arbitrary word
to a heap cell and canonicalize interior pointers. Under the mover, resolving a
word to a block **pins that block** instead of graying a cell. The register
spill (`MARK_REGISTERS`) and stack range walk are unchanged. The precise root
set and module scan already produce exact `ejsval*` slots → those get
**rewritten** on evacuation (they're precise), while the stack/register scan
pins (it's ambiguous). This split — precise roots rewrite, ambiguous roots pin —
is the whole trick.

## Object-model changes that would make GC easier

Since you're not married to NaN-boxing, here's the menu — from "high leverage,
moderate cost" to "big swing" — with an eye to what each buys the *collector*.
The value-representation question is where the reference-type discussion from the
LLVM section lands.

### Value representation

- **Keep NaN-boxing.** Densest, fastest for float-heavy code, pointer inline.
  Costs: opaque to LLVM's GC (previous section), and every mover must rewrite
  inline pointers. Fully compatible with mostly-copying + stackmap-pinning +
  shadow-stack relocation — i.e. you can keep it through Phases 0–6.
- **Tagged pointers (Smi-style): heap refs are real `ptr addrspace(1)`, small
  ints/immediates are tag-bit non-pointers, doubles boxed (or a NaN-box side-path
  for doubles only).** This is the change that makes ejsval a *uniformly
  reference-typed* value and unlocks native LLVM statepoints — GC values become
  honest pointers `RewriteStatepointsForGC` understands (mark the addrspace
  *non-integral*, `ni:1`, so no int↔ptr optimization corrupts them). Cost:
  doubles become heap objects, hurting numeric JS — the classic V8-Smi vs.
  JSC-NaN-box trade. Worth it only if native statepoint relocation (destination
  (iii)) is the goal.
- **Hybrid, oracle-driven.** NaN-box `any`/number values; carry
  statically-proven references as addrspace(1) pointers. The per-value
  representation split described in the LLVM section. Recovers float density on
  the untyped paths while making the typed fraction natively relocatable.

Recommendation: **don't change the value representation for the mostly-copying
collector — it doesn't need it.** Reach for tagged pointers or the hybrid only if
you decide precise *LLVM-relocatable* roots are worth the numeric-perf hit. That
decision can be deferred past Phases 0–4.

### Object header → shapes / hidden classes (highest leverage — helps GC *and* speed)

Today an `EJSObject` is `{ gc_header, ops, proto, map }` (`ejs-object.h:229-234`)
where `map` is a **malloc'd, out-of-line hash table** of `ejsval`s
(`_EJSPropertyMap`, `:128`). For the collector this is the worst shape: variable
indirection, *not* in the GC heap (so never compacted), scanned through a
function-pointer callback. For execution it's also worst-case: every property
access is a hash lookup and there are no inline caches.

Replacing per-object hash maps with **hidden classes / shapes + inline slot
arrays** (V8/SpiderMonkey style) would, for the collector:
- make objects **fixed-size and trivially copyable** (shape id + contiguous slot
  array),
- lay all ejsval slots **contiguous**, enabling **precise, branch-free tracing**
  from a per-shape pointer-offset bitmap instead of a virtual `Scan` call,
- pull property storage **into the GC heap** so it compacts,
- and (the real prize) enable **inline caches** later — the thing that actually
  moves echojs toward V8, which #7 otherwise can't reach.

This dovetails with the maam type-oracle already planned: it computes *per
allocation-site layouts* (`result.layouts()` — field names, offsets, `TypeSig`s;
`docs/maam-plan.md`). That *is* a static shape assignment — objects can be born
with their hidden class instead of building a hash map dynamically. **This is the
single highest-leverage change in this document**: one move that is a GC change,
a speed change, and a type-system change at once.

### Trace metadata in the header

Independent of the above: replace `scan_type` bits + the virtual `Scan` op with a
**per-shape pointer-offset bitmap** referenced from the header. Tracing becomes
"for each set bit, follow this slot," no indirect call — faster mark/evacuate and
trivially moving-aware. The widened 64-bit header (Phase 1) has room for a
shape/trace-map index.

### Closure-env slot addressing (kills the biggest pin source)

The largest expected source of pinning is optimized code holding **raw interior
pointers into closure-env slots** with the env base dead (`emit.ts:624-641`;
interior-pointer handling at `ejs-gc.c:823-833`). Two ways to let envs move:
- have `env_load`/`env_store` recompute the slot address from a live base +
  index at each use, rather than materializing and holding a raw `EjsValue*`
  across a safepoint (an emitter change), or
- give envs a **Brooks-style forwarding word** (which a concurrent mover wants
  anyway — see below) so an interior pointer can be relocated by following the
  forward.
Either converts the env from "pins its block" to "relocatable," which is likely
the difference between a good and a bad pin rate.

### Pretenuring from allocation-site lifetime

The oracle (or simple runtime feedback) can mark allocation sites whose objects
reliably survive → **born directly in old gen**, skipping nursery churn. Small
change, pure win, needs the generational infra from Phase 3.

## Concurrent collection (the collector on its own thread)

You want the collector to run **concurrently with the mutator**. The mutator
stays single-threaded; the collector becomes the engine's second thread, so
mutator pauses shrink to brief handshakes. This is a first-class goal, and the
design below builds toward it from Phase 3 rather than bolting it on — chiefly by
choosing a write barrier now that a concurrent marker can reuse.

### What "concurrent" requires, in order of difficulty

1. **A collector thread + cooperative safepoints.** The mutator polls a safepoint
   flag at allocation and at chosen back-edges/calls; the collector requests a
   handshake for phase transitions and the root snapshot. With a *single* mutator
   this is one handshake, not an N-thread stop-the-world protocol — dramatically
   simpler than Go/JVM. The conservative (or stackmap) root scan happens during a
   brief STW snapshot; everything else runs concurrently.

2. **Concurrent marking with a snapshot-at-the-beginning (SATB) write barrier.**
   While the collector marks, the mutator keeps mutating; to not lose objects the
   barrier **logs the overwritten (old) value** of every ejsval store so the
   collector still traces it. This is why Phase 3's generational card barrier
   must *also* log old values: a card records *where* an old→young pointer is
   (generational need); the logged old value feeds the SATB mark queue
   (concurrent need). **One barrier, two consumers** — getting it right in Phase 3
   is what makes concurrency a later *addition* rather than a *rewrite*.

3. **Concurrent sweeping / survivor evacuation** — two tiers:
   - **Concurrent mark + brief STW evacuation (the sweet spot; target first).**
     Marking is the long phase and runs off the mutator. In a mostly-copying heap
     evacuation copies only the *unpinned survivors* — a small fraction — so the
     STW compaction is short. This alone turns the pause from "proportional to
     live set" into "proportional to survivors, determined concurrently."
     SpiderMonkey and others lived here productively for years. Needs SATB
     marking (2) + a STW root re-scan, nothing more exotic.
   - **Fully concurrent evacuation (move while the mutator runs).** The hard tier:
     the mutator may touch an object mid-copy. Needs a **load/read barrier** so
     every ejsval load resolves through a forwarding pointer to the moved copy —
     either a **Brooks forwarding word** in each object header (simple,
     rep-agnostic, works with i64 NaN-boxing, costs a word + an indirection per
     load; the widened header has room) or **ZGC-style colored pointers** (steal
     bits for mark/remap state + a self-healing load barrier — tighter under
     NaN-boxing since the tag bits are taken, though the 3 low alignment bits and
     sub-2⁴⁷ high bits are available). Brooks is the pragmatic choice and composes
     with the env-forwarding fix above.

### The honest caveat: conservative/pinning roots vs. concurrent *moving*

Concurrent *marking* pairs with any rooting scheme — pinned blocks simply get
marked in place. Concurrent *moving of stack-reachable objects* is the subtle
part: a conservatively-pinned (or stackmap-pinned, destination (i)) object can't
be relocated, which is *fine* — you concurrently evacuate the unpinned heap and
leave pinned blocks where they are; the load barrier resolves their identity
forward trivially. What you must not do is concurrently *move* an object named
only by a non-relocatable root. So: **concurrent marking works with everything;
concurrent moving of the heap interior works with everything; concurrent moving
of *stack-reachable* objects additionally needs relocatable roots (shadow stack
(ii) or statepoints (iii)).** Since the heap interior is the vast majority of
live data, the sweet-spot design (concurrent mark + STW survivor evacuation)
already gets you most of the pause win with plain conservative roots.

### How this reorders the plan

Concurrency changes *which barrier we build in Phase 3* and adds two later
phases, but does **not** touch Phases 0–2. Phase 3's barrier logs old values
(SATB-ready) from day one; a new **Phase 5** delivers concurrent marking + STW
survivor evacuation; **Phase 6** delivers fully concurrent evacuation via Brooks
forwarding + a load barrier. The single-mutator assumption is what makes all of
this materially easier than in a multi-threaded runtime and should be preserved
as long as possible — Web Workers, if they ever land, are what would force the
hard multi-mutator protocols.

## Knobs (attribute 8)

One primary knob: a **heap-growth target** — collect when live-set × (1 + g) is
reached, auto-tuning `g` from recent survival rates (Go's `GOGC` idea, but
self-adjusting rather than user-set). Derived automatically, not exposed:
nursery size (a small fixed multiple of the last minor survivor volume), old-gen
block size, card size, promotion age. Keep the existing `EJS_GC_*` env vars as
*debug* overrides only (`EJS_GC_DISABLE`, `EJS_GC_EVERY_N_ALLOC`,
`ejs-gc.c:708-711`). No generation-size tuning, no pause-time goals, no
survivor-ratio dials. If a knob can be derived from a measurement, derive it.

## Phased plan (each phase lands green and shippable)

The bias, as with the eir and maam plans, is toward small phases that each keep
the whole test suite passing and can be reverted independently.

- **Phase 0 — Prerequisites & instrumentation.** Fix generator stack scanning
  (`mark_generator_stacks` stub, `ejs-gc.c:1086`) so suspended generators pin
  their referents — a correctness prerequisite for *any* mover, and a real bug
  today. Add heap-audit instrumentation: object counts by kind/size, survival
  rate per cycle, and a *pin-rate estimator* — walk the conservative roots and
  report, **separately for young and old gen**, the *pinned bytes*, the *count of
  blocks that would be retained* (contain ≥1 pinned cell and so can't be
  bump-reset), and how many pins come from stack/register words vs. interior
  env-slot pointers. **Gate: this breakdown in hand.** It decides whether
  mostly-copying is worth it (low, clustered pins), whether the young-gen pin rate
  alone justifies stackmap-precise nursery roots, and whether interior-pointer
  ambiguity is pervasive enough that the env-slot fix or precise roots are
  unavoidable. Measure before building.

- **Phase 1 — Header widening & forwarding.** Widen `GCObjectHeader` to 64 bits
  and land the coordinated struct/`lib/types.ts` layout change for `EJSObject`
  (free), `EJSClosureEnv`, `EJSPrimString`. Add forwarding-pointer read/write
  helpers and a `forward(obj)` that copies + stamps. No behavior change yet
  (nothing moves); this is pure plumbing, verified by existing tests. **Gate:
  green on all three bootstrap targets.**

- **Phase 2 — Block-structured spaces + mostly-copying *major* collector.**
  Relabel arenas/pages into nursery vs. old-gen blocks; add per-block metadata
  (pin/age/mark). Replace mark-sweep with a mostly-copying full collector:
  conservative roots pin blocks, precise edges evacuate. No generations yet —
  every collection is a full compact. This is the riskiest phase; keep the old
  collector behind a build flag for A/B and differential testing. **Gate:
  identical program output vs. the mark-sweep collector across the whole test
  suite + a stress mode that collects every N allocations; heap-shrink
  demonstrated on a fragmenting benchmark.**

- **Phase 3 — Generational: nursery + SATB-ready barrier.** Add the bump nursery,
  route large allocations to old gen, add the card table and the store barrier
  (runtime `_ejs_object_setprop` + the two inlined compiler store sites,
  `emit.ts:600,639`). Minor collections evacuate nursery survivors using dirty
  cards + roots. **Build the barrier to log old values from day one** (card +
  old-value log), so concurrent SATB marking in Phase 5 is a consumer of an
  existing barrier, not a rewrite — this is the load-bearing decision that makes
  concurrency a first-class outcome. **Gate: minor-pause p99 sub-millisecond on
  the benchmark corpus; allocation throughput ≥ current; barrier emits old-value
  log; no regression in the `--types` diff lane.**

- **Phase 4 — Tuning & knob elimination.** Auto-tune the growth target from
  survival rates; derive nursery/block/card sizes; delete every tunable that can
  be computed. **Gate: one documented knob; benchmarks within noise of the
  hand-tuned Phase-3 configuration.**

- **Phase 5 — Concurrent marking + STW survivor evacuation.** Introduce the
  collector thread and single-mutator safepoint handshake; move the mark phase
  off the mutator using the Phase-3 SATB log; keep a brief STW root snapshot and
  a brief STW evacuation of unpinned survivors (small, because mostly-copying).
  **Gate: mark runs concurrently; mutator STW time bounded and independent of
  live-set size; output identical to Phase 4 under collection-stress.**

- **Phase 6 — Fully concurrent evacuation.** Add a Brooks forwarding word +
  load barrier so unpinned objects move while the mutator runs; stack-reachable
  objects remain pinned unless (i)/(ii)/(iii) rooting has been adopted. **Gate:
  major pause bounded independent of heap size on a large-heap benchmark.** An
  optional cheaper predecessor — *incremental* (single-threaded, time-sliced)
  old-gen marking — can bound major pauses without the collector thread if Phase
  5's threading proves troublesome; treat it as a fallback rung, not a
  requirement.

Phases 0–4 deliver attributes 1, 3, 4, 6 (minor), 8 and the achievable part of 7
with no threads and no value-rep change. Phases 5–6 deliver attribute 5 and the
strong form of 6; they are planned for, not speculative, and their feasibility is
front-loaded by the Phase-3 barrier decision.

## Risks, named

- **Pin rate is the whole ballgame — and it bites the nursery first.** If
  conservative roots pin many cells per cycle — plausible given the interior-
  pointer env-slot pattern and heavy register spilling at `-O2` — you get
  premature promotion and retained blocks, and the compaction win erodes. It's
  worst in the young gen, where fresh objects are heavily stack/register-
  referenced (see §"Pinning granularity"). Cell-granularity pinning bounds the
  cost to pinned *bytes*, but the retained-block count still climbs with scatter.
  Phase 0 measures both, per generation, *before* commitment. If the young-gen
  rate is bad, the targeted fix is stackmap-precise *nursery* roots (destination
  (i)); the broader fallback is making JS frames precise via compiler-emitted
  spill-of-live-GC-values at `E.GC` safepoints (the effect table already marks
  them, `ops.ts:15`), shrinking ambiguity to just the C runtime. Both are known
  changes; keep them in the back pocket, don't lead with them.
- **The header widening is a coordinated cross-language change.** Runtime structs
  and `lib/types.ts` must move together or compiled code reads objects at the
  wrong offsets. Land it atomically (Phase 1) with the old collector still
  active so it's independently verifiable.
- **Interior pointers into moved objects.** The conservative scan accepts
  interior pointers (`ejs-gc.c:823-833`); a pinned block is fine, but we must
  *never* evacuate an object that any ambiguous interior pointer targets. The
  pin-the-block rule handles this only if block ownership is resolved from
  interior pointers correctly — reuse `find_page_and_cell`'s existing interior
  canonicalization (`:830`).
- **Objective-C compilation on macOS.** The new collector file compiles as ObjC
  (`runtime/BUCK:118-127`); keep it clean C that survives `-x objective-c`.
- **`-O0` runtime vs `-O2` mutator asymmetry** already bites the register spill
  (`ejs-gc.c:1016-1022`); the mover inherits every one of those assumptions.
  Don't assume the compiler won't invent a pointer representation the scanner
  hasn't seen — the differential stress mode in Phase 2 is the safety net.
- **This will not make echojs competitive with V8.** Worth repeating so the
  effort is scoped right: the collector is not the bottleneck; property-map hash
  lookups and the absence of inline caches are. A generational mover makes GC a
  non-issue, which is the right and sufficient goal for *this* work.

## Alternatives considered

- **Keep conservative mark-sweep, add generations only (non-moving).** Cheapest;
  gets you minor-pause wins and the card barrier without any moving or header
  work. But no compaction → fragmentation persists, and you asked for moving.
  Worth noting as the "half-measure" if Phase-2 risk proves unpalatable — Phases
  0, 3 (barrier), and a non-moving nursery are independently valuable.
- **Precise roots via LLVM `gc.statepoint` / a reference-typed ejsval, or precise
  roots via stackmaps / a shadow stack.** All four are dissected in
  §"The LLVM precise-root question": statepoints and a reference-typed ejsval are
  gated on a value-rep change (they need real pointers); `llvm.experimental.stackmap`
  gives fork-free precise *pinning* that lowers the pin rate under mostly-copying
  and is the recommended precision upgrade; a manual shadow stack gives precise
  *relocatable* roots while keeping NaN-boxing, at spill/reload cost, and is the
  Phase-0-triggered fallback if the pin rate demands relocation of stack-reachable
  objects. None is needed for the baseline collector.
- **A third-party collector (mmtk, Boehm generational, Immix as a library).**
  mmtk is the serious option and its binding model fits an AOT runtime, but it
  wants precise roots or careful conservative-root support and would still hit
  the C-runtime rooting problem; adopting it is not obviously less work than the
  staged plan above and forfeits the reuse of echojs's existing precise `Scan`
  ops and conservative scanner. Revisit if the hand-rolled collector stalls.
