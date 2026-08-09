/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

// marking: the tri-color worklist, the precise slot scanners, the
// conservative stack/register scanners with the gc-frame skip
// machinery, and the full-GC mark phases.

#include "ejs-gc-internal.h"

#define MAX_WORKLIST_SEGMENT_SIZE 512
typedef struct _WorkListSegmnt {
    EJS_SLIST_HEADER(struct _WorkListSegmnt);
    int size;
    GCObjectPtr work_list[MAX_WORKLIST_SEGMENT_SIZE];
} WorkListSegment;

typedef struct {
    WorkListSegment *list;
    WorkListSegment *free_list;
} WorkList;

static WorkList work_list;

void
_ejs_gc_worklist_init()
{
    work_list.list = NULL;
    work_list.free_list = NULL;
}

static void
_ejs_gc_worklist_push(GCObjectPtr obj)
{
    if (obj == NULL)
        return;

    WorkListSegment *segment;

    if (EJS_UNLIKELY(!work_list.list || work_list.list->size == MAX_WORKLIST_SEGMENT_SIZE)) {
        // we need a new segment
        if (work_list.free_list) {
            // take one from the free list
            segment = work_list.free_list;
            EJS_SLIST_DETACH_HEAD(segment, work_list.free_list);
        }
        else {
            segment = (WorkListSegment*)malloc (sizeof(WorkListSegment));
            segment->size = 0;
        }
        EJS_SLIST_ATTACH(segment, work_list.list);
    }
    else {
        segment = work_list.list;
    }

    segment->work_list[segment->size++] = obj;
}

static GCObjectPtr
_ejs_gc_worklist_pop()
{
    if (work_list.list == NULL || work_list.list->size == 0/* shouldn't happen, since we push the page to the free list if we hit 0 */)
        return NULL;

    WorkListSegment *segment = work_list.list;

    GCObjectPtr rv = segment->work_list[--segment->size];
    if (segment->size == 0) {
        EJS_SLIST_DETACH_HEAD(segment, work_list.list);
        EJS_SLIST_ATTACH(segment, work_list.free_list);
    }
    return rv;
}

#define WORKLIST_PUSH_AND_GRAY(x) EJS_MACRO_START        \
    if (is_white((GCObjectPtr)x)) {                      \
        _ejs_gc_worklist_push((GCObjectPtr)(x));         \
        set_gray ((GCObjectPtr)(x));                     \
    }                                                    \
    EJS_MACRO_END

#define WORKLIST_PUSH_AND_GRAY_CELL(x, cell) EJS_MACRO_START    \
    if (cell_is_white(cell)) {                                       \
        _ejs_gc_worklist_push((GCObjectPtr)(x));                \
        cell_set_gray(&cell);                                        \
    }                                                           \
    EJS_MACRO_END

static void
set_gray (GCObjectPtr ptr)
{
    uint32_t cell_idx;
    PageInfo *page = find_page_and_cell(ptr, &cell_idx);
    if (!page)
        return;

    cell_set_gray(&page->page_bitmap[cell_idx]);
}

static void
set_black (GCObjectPtr ptr)
{
    uint32_t cell_idx;
    PageInfo *page = find_page_and_cell(ptr, &cell_idx);
    if (!page)
        return;

    cell_set_black(&page->page_bitmap[cell_idx]);
}

static EJSBool
is_white (GCObjectPtr ptr)
{
    uint32_t cell_idx;
    PageInfo *page = find_page_and_cell(ptr, &cell_idx);
    if (!page)
        return EJS_FALSE;

    return cell_is_white(page->page_bitmap[cell_idx]);
}

// the mark-path scan callback.  Slot-based per the new
// EJSValueFunc contract — this non-moving path only reads through the
// slot; the mover's evacuation callback is what rewrites it.
static void
_scan_ejsvalue (ejsval* slot)
{
    ejsval val = *slot;
    if (!EJSVAL_IS_TRACEABLE_IMPL(val)) return;

    GCObjectPtr gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(val);

    if (gcptr == NULL) return;

    WORKLIST_PUSH_AND_GRAY(gcptr);
}

static void
_scan_from_ejsobject(EJSObject* obj)
{
    // freshly allocated objects are zeroed but not yet initialized (their
    // constructor may trigger a collection before _ejs_init_object runs);
    // there's nothing to scan in them yet.
    if (obj->ops == NULL)
        return;
    OP(obj,Scan)(obj, _scan_ejsvalue);
}

static void
_scan_from_ejsprimstr(EJSPrimString *primStr)
{
    EJSPrimStringType strtype = EJS_PRIMSTR_GET_TYPE(primStr);

    switch (strtype) {
    case EJS_STRING_ROPE:
        // inline _scan_ejsvalue's push logic here to save creating an ejsval from the primStr only to destruct
        // it in _scan_ejsvalue

        WORKLIST_PUSH_AND_GRAY(primStr->data.rope.left);
        WORKLIST_PUSH_AND_GRAY(primStr->data.rope.right);
        break;
    case EJS_STRING_DEPENDENT:
        WORKLIST_PUSH_AND_GRAY(primStr->data.dependent.dep);
        break;
    case EJS_STRING_FLAT:
        // nothing to do here
        break;
    }
}

static void
_scan_from_ejsprimsym(EJSPrimSymbol *primSymbol)
{
    _scan_ejsvalue (&primSymbol->description);
}

static void
_scan_from_ejsclosureenv(EJSClosureEnv *env)
{
    if (EJS_UNLIKELY(_ejs_gc_env_guard))
        _ejs_gc_validate_closureenv(NULL, env, "mark_scan");
    for (uint32_t i = 0; i < env->length; i ++) {
        _scan_ejsvalue (&env->slots[i]);
    }
}

GCObjectPtr *stack_bottom;

void
_ejs_gc_mark_thread_stack_bottom(GCObjectPtr* btm)
{
    stack_bottom = btm;
    // the write barrier's transient-slot upper bound: the main
    // stack's bottom
    _ejs_heap.current_stack_end = (void*)btm;
}

static void
mark_pointers_in_range(GCObjectPtr* low, GCObjectPtr* high)
{
    GCObjectPtr* p;
    for (p = low; p < high-1; p++) {
        GCObjectPtr gcptr;

#if OSX
        // really a 64 bit check here, since for 64 bit systems, ejsvals can be stuck in registers, so we need to check if it's a valid
        // ejsval gcthing as well.
        ejsval ep = *(ejsval*)p;
        if (EJSVAL_IS_GCTHING_IMPL(ep))
            gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(ep);
        else
#endif
            gcptr = *p;

        if (gcptr == NULL)            continue; // skip nulls.
        if ((char*)gcptr < conservative_lo || (char*)gcptr >= conservative_hi)
            continue; // cheap prefilter: outside every arena/LOS block

        uint32_t cell_idx;

        PageInfo *page = find_page_and_cell(gcptr, &cell_idx);
        if (!page)                    continue; // skip values outside our heap.

        // XXX more checks before we start treating the pointer like a GCObjectPtr?
        BitmapCell cell = page->page_bitmap[cell_idx];
        if (!cell_is_allocated(page, cell_idx, cell)) continue;

        // during a minor collection conservative hits PIN young
        // cells in place; nothing else is this collection's business
        if (in_minor_gc) { minor_conservative_hit(page, cell_idx); continue; }

        // a conservative hit PINS: the compacting major must sweep this
        // cell in place.  Recorded even when the target is already
        // marked (the white check below is a marking optimization, not
        // a pin filter).  profile_note_pin sets the same bit plus stats.
        if (gc_profile) profile_note_pin(page, cell_idx, gcptr);
        else *(GCObjectHeader*)(page->page_start + ((size_t)cell_idx * page->cell_size)) |= EJS_GC_HEADER_PINNED;

        if (!cell_is_white(cell)) continue; // skip pointers to gray/black cells

        // canonicalize interior pointers to the start of their cell; the
        // worklist processing reads the object header from the pointer.
        gcptr = page->page_start + (cell_idx * page->cell_size);

        WORKLIST_PUSH_AND_GRAY_CELL(gcptr, page->page_bitmap[cell_idx]);
    }
}

// gc-frame slots are stack memory, so the conservative
// stack scan would see every precisely-rooted value a second time and
// pin it through its own slot — precision would never move anything.
// During a minor, the scan skips the frame records of the stack being
// scanned (their slots are walked precisely and rewritten).  Full GC
// never skips: it relies on the conservative scan seeing the slots.
typedef struct { char* lo; char* hi; } FrameSkipRange;
#define MAX_FRAME_SKIP 1024
static FrameSkipRange frame_skip[MAX_FRAME_SKIP];
static int frame_skip_count;

void
set_frame_skip_chain(void* chain_head)
{
    frame_skip_count = 0;
    for (EJSGCFrame* f = (EJSGCFrame*)chain_head; f; f = f->prev) {
        if (frame_skip_count == MAX_FRAME_SKIP) break; // partial skip = extra pins only
        char* lo = (char*)f;
        char* hi = lo + 16 + 8 * f->count;
        // insertion sort by lo; chains are short and near-sorted
        int i = frame_skip_count++;
        while (i > 0 && frame_skip[i - 1].lo > lo) {
            frame_skip[i] = frame_skip[i - 1];
            i--;
        }
        frame_skip[i].lo = lo;
        frame_skip[i].hi = hi;
    }
}

void
clear_frame_skip(void)
{
    frame_skip_count = 0;
}

static void
mark_ejsvals_in_range(void* low, void* high)
{
    // per-call skip cursor: ranges below `low` are behind us
    int fr = 0;
    while (fr < frame_skip_count && frame_skip[fr].hi <= (char*)low) fr++;
    void* p = low;
#if IOS
    while (((uintptr_t)p) & 0x7) {
        p++;
    }
#endif
    for (; p < high - sizeof(ejsval); p += sizeof(ejsval)) {
        // inside a gc-frame record?  its slots are precise roots
        while (fr < frame_skip_count && frame_skip[fr].hi <= (char*)p) fr++;
        if (fr < frame_skip_count && (char*)p >= frame_skip[fr].lo) continue;
        ejsval candidate_val = *((ejsval*)p);
        GCObjectPtr gcptr;
        if (EJSVAL_IS_GCTHING_IMPL(candidate_val)) {
            gcptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(candidate_val);
        }
        else {
            // also treat the slot as a raw, untagged pointer: optimized
            // (opt -O2) code compiled by ejs unboxes closure envs and
            // objects once and keeps/spills the raw pointer, with the
            // tagged ejsval potentially dead.
            gcptr = *(GCObjectPtr*)p;
        }

        if (gcptr == NULL)            continue; // skip nulls.
        if ((char*)gcptr < conservative_lo || (char*)gcptr >= conservative_hi)
            continue; // cheap prefilter: outside every arena/LOS block

        uint32_t cell_idx;
        PageInfo *page = find_page_and_cell(gcptr, &cell_idx);
        if (page) {
            // XXX more checks before we start treating the pointer like a GCObjectPtr?
            BitmapCell cell = page->page_bitmap[cell_idx];
            if (!cell_is_allocated(page, cell_idx, cell)) continue;

            // minor collections only pin young cells here
            if (in_minor_gc) { minor_conservative_hit(page, cell_idx); continue; }

            // a conservative hit PINS: the compacting major must sweep
            // this cell in place (recorded even when already marked)
            if (gc_profile) profile_note_pin(page, cell_idx, gcptr);
            else *(GCObjectHeader*)(page->page_start + ((size_t)cell_idx * page->cell_size)) |= EJS_GC_HEADER_PINNED;

            if (!cell_is_white(cell)) continue; // skip pointers to gray/black cells

            // canonicalize interior pointers to the start of their cell; the
            // worklist processing reads the object header from the pointer.
            gcptr = page->page_start + (cell_idx * page->cell_size);

            WORKLIST_PUSH_AND_GRAY_CELL(gcptr, page->page_bitmap[cell_idx]);
        }
    }
}

// walk the gc-frame chain — precise, relocatable JS-frame roots.
// Records live in stack frames that stay mapped for exactly as long as
// they are linked (returns unlink, catches re-link their own frame past
// unwound callees).
void
walk_gc_frames(void (*slot_fn)(ejsval*))
{
    for (EJSGCFrame* f = (EJSGCFrame*)_ejs_heap.gc_frame_head; f; f = f->prev)
        for (uintptr_t i = 0; i < f->count; i++)
            slot_fn(&f->slots[i]);
}

static void
mark_root_slot(ejsval* root)
{
    num_roots++;
    ejsval rootval = *root;
    if (!EJSVAL_IS_GCTHING_IMPL(rootval))
        return;
    GCObjectPtr root_ptr = (GCObjectPtr)EJSVAL_TO_GCTHING_IMPL(rootval);
    if (root_ptr == NULL)
        return;
    uint32_t cell_idx;
    PageInfo* page = find_page_and_cell(root_ptr, &cell_idx);
    if (!page)
        return;

    BitmapCell cell = page->page_bitmap[cell_idx];
    if (cell_is_free(cell))   return; // skip free cells
    if (!cell_is_white(cell)) return; // skip pointers to gray/black cells
    WORKLIST_PUSH_AND_GRAY_CELL(root_ptr, page->page_bitmap[cell_idx]);
}

void
mark_from_roots()
{
    SPEW (2, _ejs_log ("marking from roots"));
    root_registry_foreach (mark_root_slot);
    SPEW (2, _ejs_log ("done marking from roots"));
}

void
mark_from_modules()
{
    SPEW(2, _ejs_log ("marking from module exotics"));

    for (int i = 0; i < _ejs_num_modules; i ++) {
        EJSObject* mod = (EJSObject*)_ejs_modules[i];
        // modules are static globals whose object headers aren't set up
        // until _ejs_require_init; if a collection happens before that
        // (e.g. EJS_GC_EVERY_N_ALLOC during _ejs_init) there's nothing to
        // scan yet.
        if (mod->ops == NULL)
            continue;
        _scan_from_ejsobject(mod);
    }
}

#if TARGET_CPU_ARM
#define MARK_REGISTERS EJS_MACRO_START \
    GCObjectPtr __r0, __r1, __r2, __r3, __r4, __r5, __r6, __r7, __r8, __r9, __r10, __r11, __r12, __end; \
    __asm ("str r0, %0; str r1, %1; str r2, %2; str r3, %3; str r4, %4; str r5, %5; str r6, %6;" \
           "str r7, %7; str r8, %8; str r9, %9; str r10, %10; str r11, %11; str r12, %12;" \
          : "=m"(__r0), "=m"(__r1), "=m"(__r2), "=m"(__r3), "=m"(__r4),  \
            "=m"(__r5), "=m"(__r6), "=m"(__r7), "=m"(__r8),  "=m"(__r9), \
            "=m"(__r10), "=m"(__r11), "=m"(__r12));                      \
                                                                         \
    mark_pointers_in_range(&__end, &__r0);                               \
    EJS_MACRO_END
#elif TARGET_CPU_ARM64
// spill the callee-saved registers (x19-x28, plus fp) and treat them as
// roots.  code compiled by ejs (opt -O2) keeps live ejsvals in callee-saved
// registers across calls, and the mostly -O0 runtime doesn't reliably save
// all of them anywhere the stack scan would see.  (an empty MARK_REGISTERS
// here let live objects be collected and their cells reused -> heap
// corruption.)
#define MARK_REGISTERS EJS_MACRO_START                                  \
    GCObjectPtr __regs[21];                                             \
    __asm volatile ("stp x19, x20, [%0, #0]\n\t"                        \
                    "stp x21, x22, [%0, #16]\n\t"                       \
                    "stp x23, x24, [%0, #32]\n\t"                       \
                    "stp x25, x26, [%0, #48]\n\t"                       \
                    "stp x27, x28, [%0, #64]\n\t"                       \
                    "str x29, [%0, #80]\n\t"                            \
                    /* llvm will spill gprs into the callee-saved simd   \
                       registers under pressure, so scan those too */    \
                    "stp d8, d9,   [%0, #88]\n\t"                        \
                    "stp d10, d11, [%0, #104]\n\t"                       \
                    "stp d12, d13, [%0, #120]\n\t"                       \
                    "stp d14, d15, [%0, #136]"                          \
                    : : "r"(__regs) : "memory");                        \
    __regs[19] = __regs[20] = NULL;                                     \
    /* mark_pointers_in_range scans [low, high-1) */                    \
    mark_pointers_in_range(__regs, __regs + 21);                        \
    EJS_MACRO_END
#elif TARGET_CPU_AMD64
#define MARK_REGISTERS EJS_MACRO_START \
    GCObjectPtr __rax, __rbx, __rcx, __rdx, __rsi, __rdi, __rbp, __rsp, __r8, __r9, __r10, __r11, __r12, __r13, __r14, __r15, __end; \
    __asm ("movq %%rax, %0; movq %%rbx, %1; movq %%rcx, %2; movq %%rdx, %3; movq %%rsi, %4;" \
           "movq %%rdi, %5; movq %%rbp, %6; movq %%rsp, %7; movq %%r8, %8;  movq %%r9, %9;" \
           "movq %%r10, %10; movq %%r11, %11; movq %%r12, %12; movq %%r13, %13; movq %%r14, %14; movq %%r15, %15;" \
          : "=m"(__rax), "=m"(__rbx), "=m"(__rcx), "=m"(__rdx), "=m"(__rsi), \
            "=m"(__rdi), "=m"(__rbp), "=m"(__rsp), "=m"(__r8),  "=m"(__r9), \
            "=m"(__r10), "=m"(__r11), "=m"(__r12), "=m"(__r13), "=m"(__r14), "=m"(__r15)); \
                                                                        \
    mark_pointers_in_range(&__end, &__rax);                             \
    EJS_MACRO_END
#elif TARGET_CPU_X86
#define MARK_REGISTERS // just keep the build limping along
#else
#error "put code here to mark registers"
#endif

void
mark_thread_stack()
{
    prof_pin_source = PROF_SRC_REGS;
    MARK_REGISTERS;
    prof_pin_source = PROF_SRC_CSTACK;

    GCObjectPtr stack_top = NULL;

    mark_ejsvals_in_range(((void*)&stack_top) + sizeof(GCObjectPtr), (void*)stack_bottom);
}

// mark a known heap object as a root (page cell or LOS both resolve
// through find_page_and_cell; the pointer must be an object base)
void
mark_object_root(GCObjectPtr ptr)
{
    uint32_t cell_idx;
    PageInfo* page = find_page_and_cell(ptr, &cell_idx);
    if (!page)
        return;
    BitmapCell cell = page->page_bitmap[cell_idx];
    if (!cell_is_allocated(page, cell_idx, cell))
        return;
    if (in_minor_gc) {
        // minor collections: a young root pins; an old root's slots may hold
        // young references, so queue it for the precise minor scan
        // (duplicates are harmless — evacuation is idempotent)
        if (page->young) minor_conservative_hit(page, cell_idx);
        else minor_wl_push(ptr);
        return;
    }
    if (!cell_is_white(cell))
        return;
    WORKLIST_PUSH_AND_GRAY_CELL(ptr, page->page_bitmap[cell_idx]);
}

void
process_worklist()
{
    GCObjectPtr p;
    while ((p = _ejs_gc_worklist_pop())) {
        set_black (p);
        GCObjectHeader* headerp = (GCObjectHeader*)p;
        if ((*headerp & EJS_SCAN_TYPE_OBJECT) != 0)
            _scan_from_ejsobject((EJSObject*)p);
        else if ((*headerp & EJS_SCAN_TYPE_PRIMSTR) != 0)
            _scan_from_ejsprimstr((EJSPrimString*)p);
        else if ((*headerp & EJS_SCAN_TYPE_PRIMSYM) != 0)
            _scan_from_ejsprimsym((EJSPrimSymbol*)p);
        else if ((*headerp & EJS_SCAN_TYPE_CLOSUREENV) != 0)
            _scan_from_ejsclosureenv((EJSClosureEnv*)p);
    }

    EJS_ASSERT(work_list.list == NULL);
}

