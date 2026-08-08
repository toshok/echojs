/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

// The collector's internal contract (the collector file split).
// Nothing here is API — ejs-gc.h is the public surface.  Module map:
//
//   ejs-gc.c        lifecycle API, allocator entry, cell free path,
//                   root registry, collection policy, GC JS object
//   ejs-gc-heap.c   arena reservation, arenas/pages, LOS + lookup,
//                   find_page_and_cell
//   ejs-gc-mark.c   worklist, precise + conservative scanners,
//                   gc-frame skip, generator stack bookkeeping
//   ejs-gc-minor.c  the nursery and the mostly-copying minor
//   ejs-gc-major.c  full collections: mark/sweep orchestration,
//                   major compaction, the epoch advance
//   ejs-gc-debug.c  EJS_GC_PROFILE / WATCH / VERIFY / PARANOID

#ifndef _ejs_gc_internal_h_
#define _ejs_gc_internal_h_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <setjmp.h>

#include "ejs-gc.h"
#include "ejs-function.h"
#include "ejs-generator.h"
#include "ejs-arguments.h"
#include "ejs-shapes.h"
#include "ejs-value.h"
#include "ejs-string.h"
#include "ejs-symbol.h"
#include "ejs-error.h"
#include "ejs-ops.h"
#include "ejsval.h"
#include "ejs-module.h"

#define clear_on_finalize 0

#define spew 0
#define sanity 0
#define gc_timings 0

#if spew
static int _ejs_spew_level = (spew);
#define SPEW(level,x) do { if ((level) < _ejs_spew_level) { x; } } while (0)
#else
#define SPEW(level,x)
#endif
#if sanity
#define SANITY(x) x
#else
#define SANITY(x)
#endif

#if EJS_BITS_PER_WORD == 64
// 2GB
#define MAX_HEAP_SIZE (2LL * 1024LL * 1024LL * 1024LL)
#else
// 128MB
#define MAX_HEAP_SIZE (128LL * 1024LL * 1024LL)
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define USABLE_PAGE_SIZE PAGE_SIZE

#define CELLS_OF_SIZE(size) (USABLE_PAGE_SIZE / (size))
#define CELLS_IN_PAGE(page) CELLS_OF_SIZE((page)->cell_size)

// arenas are reserved in ARENA_PAGES * PAGE_SIZE chunks.  ARENA_PAGES=8192 gives us an arena size of 32MB
#define ARENA_PAGES 8192
#define ARENA_SIZE (PAGE_SIZE*ARENA_PAGES)

#define PTR_TO_ARENA_MASK (uintptr_t)(~(ARENA_SIZE-1))

// turn a random pointer into an arena pointer
#define PTR_TO_ARENA(ptr) ((void*)((uintptr_t)(ptr) & PTR_TO_ARENA_MASK))
#define PTR_TO_ARENA_PAGE_BASE(ptr) ((void*)EJS_ALIGN(PTR_TO_ARENA(ptr) + sizeof(Arena), PAGE_SIZE))
#define PTR_TO_ARENA_PAGE_INDEX(ptr) ((((uintptr_t)(ptr) & ~PTR_TO_ARENA_MASK) - ((uintptr_t)PTR_TO_ARENA_PAGE_BASE(ptr) & ~PTR_TO_ARENA_MASK)) / PAGE_SIZE)

#define PTR_TO_CELL(ptr,info) (((char*)(ptr) - (char*)(info)->page_start) / (info)->cell_size)

#define IS_ALIGNED_TO(v,a) (((uintptr_t)(v) & ((a)-1)) == 0)
#define ALLOC_ALIGN 8
#define EJS_ALIGN(v,a) (((uintptr_t)(v) + (a)-1) & ~((a)-1))
#define IS_ALLOC_ALIGNED(v) IS_ALIGNED_TO(v, ALLOC_ALIGN)

#if IOS || OSX
#include <mach/vm_statistics.h>
#define MAP_FD VM_MAKE_TAG (VM_MEMORY_APPLICATION_SPECIFIC_16)
#else
#define MAP_FD -1
#endif

// two header bits from the gc-reserved range (57-63; see ejs-types.h).
// YOUNG: set at allocation, cleared on first survival (profiling) or
// promotion (the nursery).  PINNED: set on every conservative hit during
// a full collection — the compacting major must sweep that cell in
// place; cleared by compaction's fixup walk (or the profile census when
// compaction is off).
#define EJS_GC_HEADER_YOUNG  (1ULL << 57)
#define EJS_GC_HEADER_PINNED (1ULL << 58)

#if CONCURRENT
#error "not implemented"
#else
#define LOCK_PAGE(info)
#define UNLOCK_PAGE(info)
#define LOCK_GC()
#define UNLOCK_GC()
#define LOCK_ARENAS()
#define UNLOCK_ARENAS()
#endif

typedef struct _PageInfo PageInfo;
typedef struct _LargeObjectInfo LargeObjectInfo;

typedef struct _Arena {
    void*     end;
    void*     pos;
    PageInfo* free_pages;
    void*     pages[ARENA_PAGES];
    PageInfo* page_infos[ARENA_PAGES];
    int       num_pages;
    // the nursery is a dedicated arena so "is young" is a
    // range check; old-gen page allocation skips nursery arenas
    EJSBool   is_nursery;
} Arena;

#define MAX_ARENAS (MAX_HEAP_SIZE / ARENA_SIZE)

// ---- the cell lifecycle ----------------------------------------
//
// One bitmap byte per page cell.  A cell is FREE or ALLOCATED, and an
// allocated cell carries a tri-color mark; every state predicate and
// transition lives in this block, and the encoding is private to it.
//
// White/black are EPOCH-RELATIVE: the color bits hold GRAY or the
// parity of the mark epoch the cell was last colored in.  color ==
// (mark_epoch & 1) is black (marked this epoch); the complement is
// white.  mark_epoch_advance() — called at exactly one site, the end
// of a full collection — thus turns every surviving black cell white
// in O(1) without touching a bitmap.  (The old collector expressed
// the same aging as a white_mask/black_mask swap mutated at the same
// site; the epoch is that flip made explicit and single-owner.)

typedef char BitmapCell;

#define CELL_COLOR_MASK 0x03
#define CELL_GRAY       0x02
#define CELL_FREE       0x04 // cell is in the free list for this page

extern unsigned int mark_epoch; // parity 1 at startup: black starts at color 1

static inline BitmapCell cell_black_color(void) { return (BitmapCell)(mark_epoch & 1); }
static inline BitmapCell cell_white_color(void) { return (BitmapCell)((mark_epoch & 1) ^ 1); }

// the ONLY place the white/black meaning ever changes
static inline void
mark_epoch_advance(void)
{
    mark_epoch++;
}

static inline EJSBool cell_is_free (BitmapCell c) { return (c & CELL_FREE) == CELL_FREE; }
static inline EJSBool cell_is_gray (BitmapCell c) { return (c & CELL_COLOR_MASK) == CELL_GRAY; }
static inline EJSBool cell_is_white(BitmapCell c) { return (c & CELL_COLOR_MASK) == cell_white_color(); }
static inline EJSBool cell_is_black(BitmapCell c) { return (c & CELL_COLOR_MASK) == cell_black_color(); }

static inline void cell_set_gray (BitmapCell* c) { *c = (BitmapCell)((*c & ~CELL_COLOR_MASK) | CELL_GRAY); }
static inline void cell_set_white(BitmapCell* c) { *c = (BitmapCell)((*c & ~CELL_COLOR_MASK) | cell_white_color()); }
static inline void cell_set_black(BitmapCell* c) { *c = (BitmapCell)((*c & ~CELL_COLOR_MASK) | cell_black_color()); }
static inline void cell_set_free (BitmapCell* c) { *c = CELL_FREE; }
static inline void cell_set_allocated(BitmapCell* c) { *c = (BitmapCell)(*c & ~CELL_FREE); }

struct _PageInfo {
    EJS_LIST_HEADER(struct _PageInfo);
    void*       bump_ptr;
    void*       page_start;
    void*       page_end;
    BitmapCell* page_bitmap;
    LargeObjectInfo *los_info;
    int32_t     cell_size;
    int16_t     num_cells;
    int16_t     num_free_cells;
    // 0 = old gen; 1 = active young page (bump-allocated,
    // allocated-ness = below bump); 2 = young survivor page (holds
    // pinned young objects, bitmap-authoritative, no further bumping)
    uint8_t     young;
};

struct _LargeObjectInfo {
    EJS_LIST_HEADER(struct _LargeObjectInfo);
    size_t alloc_size;
    PageInfo page_info;
};

#define OBJECT_SIZE_LOW_LIMIT_BITS 4  // smallest object we'll allocate (1<<4 = 16)
#define OBJECT_SIZE_HIGH_LIMIT_BITS 8 // max object size for the non-LOS allocator = 256

// heap_pages is indexed by ffs(cell_size) - OBJECT_SIZE_LOW_LIMIT_BITS,
// i.e. 16B -> 1 .. 256B -> 5 ([0] is unused); +2 covers the inclusive
// top class.  Single-cell shaped objects up to the 14-field cap
// (32+16+112 = 160) and >14-slot envs take pages, not the LOS.
#define HEAP_PAGELISTS_COUNT (OBJECT_SIZE_HIGH_LIMIT_BITS - OBJECT_SIZE_LOW_LIMIT_BITS) + 2

// allocated-ness of a young ACTIVE page's cell is the bump rule:
// everything below the bump cursor is an object, the bitmap holds only
// collection colors
static inline EJSBool
young_cell_is_allocated(PageInfo* page, uint32_t cell_idx)
{
    return page->page_start + (size_t)cell_idx * page->cell_size < page->bump_ptr;
}

// allocated-ness of a cell: old pages answer from the bitmap; ACTIVE
// young pages (young==1) answer from the bump rule; SURVIVOR young
// pages (young==2) are bitmap-authoritative again (their pinned cells
// were re-marked at minor sweep)
static inline EJSBool
cell_is_allocated(PageInfo* page, uint32_t cell_idx, BitmapCell cell)
{
    if (page->young == 1) return young_cell_is_allocated(page, cell_idx);
    return !cell_is_free(cell);
}

// rewrite an ejsval's payload in place, preserving its NaN-box tag
static inline void
rewrite_slot_payload(ejsval* slot, GCObjectPtr to)
{
    slot->asBits = (slot->asBits & ~EJSVAL_PAYLOAD_MASK)
        | ((uint64_t)(uintptr_t)to & EJSVAL_PAYLOAD_MASK);
}

// the private half of the (single) isolate's heap context (_ejs_heap
// in ejs-gc.h is the emitted-code seam; this is everything else)
typedef struct {
    // the nursery arenas: contiguous (carved back-to-back at init,
    // before any old-gen arena), so [nursery_base, nursery_end) is one
    // span and is-young stays a two-compare range check.  Multiple
    // arenas because survivor pins (thousands of suspended generators)
    // can hold hundreds of MB of pages hostage; a starved nursery
    // pushes allocation onto the old-page fallback — correct (born
    // dirty) but slow.
#define EJS_GC_MAX_NURSERY_ARENAS 32
    Arena* nursery_arenas[EJS_GC_MAX_NURSERY_ARENAS];
    int    nursery_arena_count;
    PageInfo* young_current[EJS_GC_NUM_SIZE_CLASSES];
    EJSList young_pages; // all young pages not currently being bumped
    EJSBool verify;      // EJS_GC_VERIFY: old-gen barrier-coverage check per minor
    size_t young_alloced;  // bytes of young pages handed out this cycle
    size_t young_budget;   // minor-collection trigger (EJS_GC_NURSERY_BUDGET)
    // budget adaptivity: when a minor's fixed costs (sticky pins, frame
    // chains, dirty rescans over a huge live set) dwarf the mutator
    // window, the trigger amortizes upward; an explicit
    // EJS_GC_NURSERY_BUDGET pins it (young_budget_fixed)
    EJSBool  young_budget_fixed;
    uint64_t last_minor_end_us;
    // minor worklist (objects whose slots still need processing)
    GCObjectPtr* wl;
    int wl_count, wl_cap;
    // the remset's second buffer.  A minor collection SWAPS buffers up
    // front and processes the snapshot; slots whose referent stays young
    // (pinned) re-append into the live buffer — old→young edges CARRY
    // across cycles for as long as the target remains in the nursery.
    // Buffers grow independently (the live one grows in
    // _ejs_gc_remember_slow), so each carries its own capacity and the
    // swap exchanges both.
    void** remset_other;
    int32_t remset_other_capacity;
    // stats (reported under EJS_GC_PROFILE)
    uint64_t minors, minor_usec_total, minor_usec_max;
    uint64_t promoted_objs, promoted_bytes, minor_pins, remset_peak, overflow_minors;
} EJSHeapPriv;

// conservative-pin attribution for EJS_GC_PROFILE
enum {
    PROF_SRC_CSTACK = 0,   // conservative C-stack ranges (incl. suspended segments)
    PROF_SRC_REGS = 1,     // spilled register file
    PROF_SRC_GENSTACK = 2, // suspended generator stacks + saved ucontexts
    PROF_SRC_COUNT
};

// ---- the collection policy (ejs-gc.c) ---------------------------
typedef enum {
    GC_POLICY_YOUNG_ALLOC, // a nursery allocation is about to run
    GC_POLICY_OLD_ALLOC,   // an old-gen/LOS allocation is about to run
    GC_POLICY_AFTER_MINOR, // a minor just retired; promotions grew the old gen
    GC_POLICY_ALLOC_FAILED // allocator out of memory: forced full
} GCPolicyEvent;

void gc_policy(GCPolicyEvent ev, const char* reason);

// ---- shared state ----------------------------------------------

// mode/knob flags
extern EJSBool gc_disabled;         // EJS_GC_DISABLE       (ejs-gc.c)
extern int collect_every_alloc;     // EJS_GC_EVERY_N_ALLOC (ejs-gc.c)
extern EJSBool compact_enabled;     // EJS_GC_COMPACT       (ejs-gc.c)
extern EJSBool nursery_enabled;     // EJS_GC_NURSERY       (ejs-gc-minor.c)
extern EJSBool gc_profile;          // EJS_GC_PROFILE       (ejs-gc-debug.c)
extern EJSBool gc_paranoid;         // EJS_GC_PARANOID      (ejs-gc-debug.c)
extern uintptr_t gc_watch_addr;     // EJS_GC_WATCH         (ejs-gc-debug.c)

// allocator accounting (ejs-gc.c)
extern size_t alloc_size;           // old-gen bytes ever allocated (promotions included)
extern size_t alloc_size_at_last_gc;
extern int num_allocs;              // the every-N stress counter
extern int total_allocs;

// heap geography (ejs-gc-heap.c)
extern EJSList heap_pages[];
extern LargeObjectInfo *los_list;
extern Arena *heap_arenas[];
extern int num_arenas;
extern char *conservative_lo;       // conservative-scan prefilter bounds
extern char *conservative_hi;

// collection state
extern EJSBool in_minor_gc;          // (ejs-gc-minor.c)
extern EJSBool in_compact_fixup;     // (ejs-gc-major.c)
extern EJSHeapPriv heap_priv;        // (ejs-gc-minor.c)
extern EJSBool minor_scan_saw_young; // (ejs-gc-minor.c) set when a scan leaves a pinned-young referent
extern size_t heap_size_at_last_gc;  // (ejs-gc-major.c) post-sweep footprint, drives full_gc_trigger
extern int num_roots;                // (ejs-gc-major.c) per-cycle census counter
extern GCObjectPtr *stack_bottom;    // (ejs-gc-mark.c)

// profiling state written outside ejs-gc-debug.c
extern struct timeval prof_start_tv;
extern int prof_pin_source;          // PROF_SRC_*, set by the scanners
extern const char* prof_gc_reason;

// ---- cross-module functions ------------------------------------

// ejs-gc.c
void root_registry_foreach(void (*fn)(ejsval*));
void root_registry_shutdown(void);
void finalize_object(GCObjectPtr p);
void _ejs_finalize_obj(GCObjectPtr ptr, Arena* arena, PageInfo* info, uint32_t cell_idx);

// ejs-gc-heap.c
void heap_space_init(void);
Arena* arena_new(void);
PageInfo* alloc_page_from_arena(Arena *arena, size_t cell_size);
PageInfo* find_page_and_cell(GCObjectPtr ptr, uint32_t *cell_idx);
PageInfo* alloc_new_page(size_t cell_size);
GCObjectPtr alloc_from_page(PageInfo *info);
GCObjectPtr alloc_from_los(size_t size, EJSScanType scan_type);
void release_to_los(LargeObjectInfo *lobj);
void old_gen_walk(void (*fn)(GCObjectPtr));
size_t calc_heap_size(void);

// ejs-gc-mark.c
void _ejs_gc_worklist_init(void);
void mark_thread_stack(void);
void mark_generator_stacks(void);
void mark_from_roots(void);
void mark_from_modules(void);
void mark_object_root(GCObjectPtr ptr);
void process_worklist(void);
void walk_gc_frames(void (*slot_fn)(ejsval*));
void set_frame_skip_chain(void* chain_head);
void clear_frame_skip(void);

// ejs-gc-minor.c
void _ejs_gc_minor_collect(const char* reason);
GCObjectPtr young_alloc_slow(int idx, size_t cell_size, EJSScanType scan_type);
void young_normalize_for_full_gc(void);
void young_page_freed(PageInfo* info, Arena* arena);
void nursery_init(void);
void remset_rebuild_after_full_gc(void);
void minor_conservative_hit(PageInfo* page, uint32_t cell_idx);
void minor_wl_push(GCObjectPtr p);
void minor_fixup_evacuated(GCObjectPtr from, GCObjectPtr to, size_t cell_size);

// ejs-gc-major.c
void _ejs_gc_collect_inner(EJSBool shutting_down);

// ejs-gc-debug.c
void profile_note_alloc(size_t size, int ffs_bucket, EJSScanType scan_type);
void profile_note_pin(PageInfo* page, uint32_t cell_idx, GCObjectPtr raw);
void profile_pre_sweep(void);
void profile_report_cycle_end(uint64_t pause_usec);
void profile_report_shutdown(void);
void gc_watch_hit(const char* what, void* p);
void paranoid_sweep_check(void);
int paranoid_report_referrers(GCObjectPtr p);
extern void** paranoid_stack_floor; // raw-stack sweep floor, set at minor entry
void verify_check_object(GCObjectPtr p);
extern ejsval* verify_bad_slot;
void _ejs_gc_dump_heap_stats(void);

#endif /* _ejs_gc_internal_h_ */
