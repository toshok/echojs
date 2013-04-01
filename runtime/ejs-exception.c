/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

//#define NO_ZEROCOST_EXCEPTIONS 1

#include "ejs-exception.h"
#include "ejs-string.h"
#include "ejs-ops.h"
#include <stdint.h>
#include <sys/types.h>
#include <execinfo.h>


#define spew 0
#if spew
#define SPEW(x) x
#else
#define SPEW(x)
#endif

/***********************************************************************
 * 64-bit implementation.
 **********************************************************************/

// unwind library types and functions
// Mostly adapted from Itanium C++ ABI: Exception Handling
//   http://www.codesourcery.com/cxx-abi/abi-eh.html

struct _Unwind_Exception;
struct _Unwind_Context;

typedef enum {
    _UA_SEARCH_PHASE = 1,
    _UA_CLEANUP_PHASE = 2,
    _UA_HANDLER_FRAME = 4,
    _UA_FORCE_UNWIND = 8,
    _UA_END_OF_STACK = 16
} _Unwind_Action;

typedef enum {
    _URC_NO_REASON = 0,
    _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
    _URC_FATAL_PHASE2_ERROR = 2,
    _URC_FATAL_PHASE1_ERROR = 3,
    _URC_NORMAL_STOP = 4,
    _URC_END_OF_STACK = 5,
    _URC_HANDLER_FOUND = 6,
    _URC_INSTALL_CONTEXT = 7,
    _URC_CONTINUE_UNWIND = 8
} _Unwind_Reason_Code;

struct dwarf_eh_bases
{
    uintptr_t tbase;
    uintptr_t dbase;
    uintptr_t func;
};

EJS_BEGIN_DECLS
extern uintptr_t _Unwind_GetIP (struct _Unwind_Context *);
extern uintptr_t _Unwind_GetCFA (struct _Unwind_Context *);
extern uintptr_t _Unwind_GetLanguageSpecificData(struct _Unwind_Context *);


// C++ runtime types and functions
// Mostly adapted from Itanium C++ ABI: Exception Handling
//   http://www.codesourcery.com/cxx-abi/abi-eh.html

typedef void (*terminate_handler) ();

// mangled std::set_terminate()
extern terminate_handler _ZSt13set_terminatePFvvE(terminate_handler);
extern void *__cxa_allocate_exception(size_t thrown_size);
extern void __cxa_throw(void *exc, void *typeinfo, void (*destructor)(void *)) __attribute__((noreturn));
extern void *__cxa_begin_catch(void *exc);
extern void __cxa_end_catch(void);
extern void __cxa_rethrow(void);
extern void *__cxa_current_exception_type(void);

#ifdef NO_ZEROCOST_EXCEPTIONS
/* fixme _sj0 for objc too? */
#define CXX_PERSONALITY __gxx_personality_sj0
#define EJS_PERSONALITY __ejs_personality_v0
#else
#define CXX_PERSONALITY __gxx_personality_v0
#define EJS_PERSONALITY __ejs_personality_v0
#endif

extern _Unwind_Reason_Code 
CXX_PERSONALITY(int version,
                _Unwind_Action actions,
                uint64_t exceptionClass,
                struct _Unwind_Exception *exceptionObject,
                struct _Unwind_Context *context);

EJS_END_DECLS

// ejs's internal exception types and data

extern const void *ejs_ehtype_vtable[];

struct ejs_typeinfo {
    // Position of vtable and name fields must match C++ typeinfo object
    const void **vtable;  // always ejs_ehtype_vtable+2
    const char *name;     // c++ typeinfo string
};

struct ejs_exception {
    ejsval val;
    struct ejs_typeinfo tinfo;
};


static void _ejs_exception_noop(void) { } 
static char _ejs_exception_false(void) { return 0; } 
static char _ejs_exception_true(void) { return 1; } 
static char _ejs_exception_do_catch(struct ejs_typeinfo *catch_tinfo, 
                                    struct ejs_typeinfo *throw_tinfo, 
                                    void **throw_obj_p, 
                                    unsigned outer);

const void *ejs_ehtype_vtable[] = {
    NULL,  // typeinfo's vtable? - fixme 
    NULL,  // typeinfo's typeinfo - fixme
    (void*)_ejs_exception_noop,      // in-place destructor?
    (void*)_ejs_exception_noop,      // destructor?
    (void*)_ejs_exception_true,      // __is_pointer_p
    (void*)_ejs_exception_false,     // __is_function_p
    (void*)_ejs_exception_do_catch,  // __do_catch
    (void*)_ejs_exception_false,     // __do_upcast
};

struct ejs_typeinfo EJS_EHTYPE_ejsvalue = {
    ejs_ehtype_vtable+2, 
    "ejsvalue", 
    // XXX nanboxing breaks this NULL
};



#if false
/***********************************************************************
 * _ejs_default_uncaught_exception_handler
 * Default uncaught exception handler. Expected to be overridden by Foundation.
 **********************************************************************/
static void _ejs_default_uncaught_exception_handler(EJSObject* exception)
{
}
static ejs_uncaught_exception_handler uncaught_handler = _ejs_default_uncaught_exception_handler;


/***********************************************************************
 * ejs_setUncaughtExceptionHandler
 * Set a handler for uncaught Objective-C exceptions. 
 * Returns the previous handler. 
 **********************************************************************/
ejs_uncaught_exception_handler 
ejs_setUncaughtExceptionHandler(ejs_uncaught_exception_handler fn)
{
    ejs_uncaught_exception_handler result = uncaught_handler;
    uncaught_handler = fn;
    return result;
}
#endif

/***********************************************************************
 * Exception personality
 **********************************************************************/

static void call_alt_handlers(struct _Unwind_Context *ctx);

_Unwind_Reason_Code 
EJS_PERSONALITY(int version,
                _Unwind_Action actions,
                uint64_t exceptionClass,
                struct _Unwind_Exception *exceptionObject,
                struct _Unwind_Context *context)
{
    EJSBool unwinding = ((actions & _UA_CLEANUP_PHASE)  ||  
                         (actions & _UA_FORCE_UNWIND));

    SPEW(printf ("EXCEPTIONS: %s through frame [ip=%p sp=%p] "
                 "for exception %p\n", 
                 unwinding ? "unwinding" : "searching", 
                 (void*)(_Unwind_GetIP(context)-1),
                 (void*)_Unwind_GetCFA(context), exceptionObject));

    // If we're executing the unwind, call this frame's alt handlers, if any.
    if (unwinding) {
        SPEW(printf ("call_alt_handlers!\n"););
        //call_alt_handlers(context);
    }

    // Let C++ handle the unwind itself.
    return CXX_PERSONALITY(version, actions, exceptionClass, 
                           exceptionObject, context);
}


/***********************************************************************
 * Compiler ABI
 **********************************************************************/

static void _ejs_exception_destructor(void *exc_gen) {
    struct ejs_exception *exc = (struct ejs_exception *)exc_gen;
    // remove the gc root for the throw exception
    _ejs_gc_remove_root (&exc->val);
}


void _ejs_exception_throw(ejsval val)
{
    struct ejs_exception *exc = 
        (struct ejs_exception*)__cxa_allocate_exception(sizeof(struct ejs_exception));

    exc->val = val;
    // need to root the exception until it's caught
    __ejs_gc_add_named_root(&exc->val, "exception in flight");

    exc->tinfo.vtable = ejs_ehtype_vtable+2;

    SPEW({
            char* utf8_exc = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(ToString(val)));
            printf ("EXCEPTIONS: throwing %p (object %s)\n", exc, utf8_exc);
            free (utf8_exc);
        });
    
    //    EJS_RUNTIME_EJS_EXCEPTION_THROW(obj);  // dtrace probe to log throw activity
    __cxa_throw(exc, &exc->tinfo, &_ejs_exception_destructor);
    __builtin_trap();
}


void _ejs_exception_rethrow(void)
{
    SPEW(printf ("EXCEPTIONS: rethrowing current exception\n"));
    
    //    EJS_RUNTIME_EJS_EXCEPTION_RETHROW(); // dtrace probe to log throw activity.
    __cxa_rethrow();
    __builtin_trap();
}


ejsval _ejs_begin_catch(void *exc_gen)
{
    SPEW(printf ("EXCEPTIONS: handling exception %p at %p\n", exc_gen, __builtin_return_address(0)));
    return (ejsval)__cxa_begin_catch(exc_gen);
}


void _ejs_end_catch(void)
{
    SPEW(printf ("EXCEPTIONS: finishing handler\n"));
    __cxa_end_catch();
}


static char _ejs_exception_do_catch(struct ejs_typeinfo *catch_tinfo, 
                                    struct ejs_typeinfo *throw_tinfo, 
                                    void **throw_obj_p, 
                                    unsigned outer)
{
    ejsval exception;

    if (throw_tinfo->vtable != ejs_ehtype_vtable+2) {
        // Only ejs types can be caught here.
        SPEW(printf ("EXCEPTIONS: skipping catch(?)\n"));
        return 0;
    }

    // `catch (EJSObject*)` always catches ejs types.
    if (catch_tinfo == &EJS_EHTYPE_ejsvalue) {
        SPEW(printf ("EXCEPTIONS: catch(EJSValue*)\n"));
        return 1;
    }

    exception = *(ejsval*)throw_obj_p;
    SPEW(printf ("EXCEPTIONS: catch()\n"));
    return 1;
}


/***********************************************************************
 * _ejs_terminate
 * Custom std::terminate handler.
 *
 * The uncaught exception callback is implemented as a std::terminate handler. 
 * 1. Check if there's an active exception
 * 2. If so, check if it's an Objective-C exception
 * 3. If so, call the previous terminate handler.
 * 4. If not, dump the ejs exception
 **********************************************************************/
static terminate_handler old_terminate = NULL;
static void _ejs_terminate(void)
{
    SPEW(printf ("EXCEPTIONS: terminating\n"));

    if (! __cxa_current_exception_type()) {
        // No current exception.
        (*old_terminate)();
    }
    else {
        // for right now assume that we got here from an ejs exception.
        printf ("unhandled exception: \n");
        printf ("trace:\n");

        void* callstack[128];
        int i, frames = backtrace(callstack, 128);
        char** strs = backtrace_symbols(callstack, frames);
        for (i = 0; i < frames; ++i) {
            printf("%s\n", strs[i]);
        }
        free(strs);
        exit(-1);

#if false
        // XXX toshok: we're going to need to make this bit of code
        // either objc or c++ to get that language impl of try/catch.

        // There is a current exception. Check if it's an objc/c++ exception.
        @try {
            __cxa_rethrow();
        } @catch (id e) {
            // It's an objc object. Call Foundation's handler, if any.
            //(*uncaught_handler)(e);
            (*old_terminate)();
        } @catch (...) {
            // It's not an ejs object. Continue to C++ terminate.
            (*old_terminate)();
        }
#endif
    }
}


/***********************************************************************
 * alt handler support - zerocost implementation only
 **********************************************************************/

#ifdef NO_ZEROCOST_EXCEPTIONS

__private_extern__ void _destroyAltHandlerList(struct alt_handler_list *list)
{
}

static void call_alt_handlers(struct _Unwind_Context *ctx)
{
    // unsupported in sjlj environments
}

#else

#include <libunwind.h>

// Dwarf eh data encodings
#define DW_EH_PE_omit      0xff  // no data follows

#define DW_EH_PE_absptr    0x00
#define DW_EH_PE_uleb128   0x01
#define DW_EH_PE_udata2    0x02
#define DW_EH_PE_udata4    0x03
#define DW_EH_PE_udata8    0x04
#define DW_EH_PE_sleb128   0x09
#define DW_EH_PE_sdata2    0x0A
#define DW_EH_PE_sdata4    0x0B
#define DW_EH_PE_sdata8    0x0C

#define DW_EH_PE_pcrel     0x10
#define DW_EH_PE_textrel   0x20
#define DW_EH_PE_datarel   0x30
#define DW_EH_PE_funcrel   0x40
#define DW_EH_PE_aligned   0x50  // fixme

#define DW_EH_PE_indirect  0x80  // gcc extension


/***********************************************************************
 * read_uleb
 * Read a LEB-encoded unsigned integer from the address stored in *pp.
 * Increments *pp past the bytes read.
 * Adapted from DWARF Debugging Information Format 1.1, appendix 4
 **********************************************************************/
static uintptr_t read_uleb(uintptr_t *pp)
{
    uintptr_t result = 0;
    uintptr_t shift = 0;
    unsigned char byte;
    do {
        byte = *(const unsigned char *)(*pp)++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    return result;
}


/***********************************************************************
 * read_sleb
 * Read a LEB-encoded signed integer from the address stored in *pp.
 * Increments *pp past the bytes read.
 * Adapted from DWARF Debugging Information Format 1.1, appendix 4
 **********************************************************************/
static intptr_t read_sleb(uintptr_t *pp)
{
    uintptr_t result = 0;
    uintptr_t shift = 0;
    unsigned char byte;
    do {
        byte = *(const unsigned char *)(*pp)++;
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    if ((shift < 8*sizeof(intptr_t))  &&  (byte & 0x40)) {
        result |= ((intptr_t)-1) << shift;
    }
    return result;
}


/***********************************************************************
 * read_address
 * Reads an encoded address from the address stored in *pp.
 * Increments *pp past the bytes read.
 * The data is interpreted according to the given dwarf encoding 
 * and base addresses.
 **********************************************************************/
static uintptr_t read_address(uintptr_t *pp, 
                              const struct dwarf_eh_bases *bases, 
                              unsigned char encoding)
{
    uintptr_t result = 0;
    uintptr_t oldp = *pp;

    // fixme need DW_EH_PE_aligned?

#define READ(type)                              \
    result = *(type *)(*pp);                    \
    *pp += sizeof(type);

    if (encoding == DW_EH_PE_omit) return 0;

    switch (encoding & 0x0f) {
    case DW_EH_PE_absptr:
        READ(uintptr_t);
        break;
    case DW_EH_PE_uleb128:
        result = read_uleb(pp);
        break;
    case DW_EH_PE_udata2:
        READ(uint16_t);
        break;
    case DW_EH_PE_udata4:
        READ(uint32_t);
        break;
#if __LP64__
    case DW_EH_PE_udata8:
        READ(uint64_t);
        break;
#endif
    case DW_EH_PE_sleb128:
        result = read_sleb(pp);
        break;
    case DW_EH_PE_sdata2:
        READ(int16_t);
        break;
    case DW_EH_PE_sdata4:
        READ(int32_t);
        break;
#if __LP64__
    case DW_EH_PE_sdata8:
        READ(int64_t);
        break;
#endif
    default:
        SPEW(printf("unknown DWARF EH encoding 0x%x at %p\n", 
                    encoding, (void *)*pp));
        break;
    }

#undef READ

    if (result) {
        switch (encoding & 0x70) {
        case DW_EH_PE_pcrel:
            // fixme correct?
            result += (uintptr_t)oldp;
            break;
        case DW_EH_PE_textrel:
            result += bases->tbase;
            break;
        case DW_EH_PE_datarel:
            result += bases->dbase;
            break;
        case DW_EH_PE_funcrel:
            result += bases->func;
            break;
        case DW_EH_PE_aligned:
            SPEW(printf ("unknown DWARF EH encoding 0x%x at %p\n", 
                         encoding, (void *)*pp));
            break;
        default:
            // no adjustment
            break;
        }

        if (encoding & DW_EH_PE_indirect) {
            result = *(uintptr_t *)result;
        }
    }

    return (uintptr_t)result;
}


static EJSBool isEjsExceptionCatcher(uintptr_t lsda, uintptr_t ip, 
                                     const struct dwarf_eh_bases* bases,
                                     uintptr_t* try_start, uintptr_t* try_end)
{
    unsigned char LPStart_enc = *(const unsigned char *)lsda++;    

    if (LPStart_enc != DW_EH_PE_omit) {
        read_address(&lsda, bases, LPStart_enc); // LPStart
    }

    unsigned char TType_enc = *(const unsigned char *)lsda++;
    if (TType_enc != DW_EH_PE_omit) {
        read_uleb(&lsda);  // TType
    }

    unsigned char call_site_enc = *(const unsigned char *)lsda++;
    uintptr_t length = read_uleb(&lsda);
    uintptr_t call_site_table = lsda;
    uintptr_t call_site_table_end = call_site_table + length;
    uintptr_t action_record_table = call_site_table_end;

    uintptr_t action_record = 0;
    uintptr_t p = call_site_table;

    while (p < call_site_table_end) {
        uintptr_t start = read_address(&p, bases, call_site_enc);
        uintptr_t len = read_address(&p, bases, call_site_enc);
        uintptr_t pad = read_address(&p, bases, call_site_enc);
        uintptr_t action = read_uleb(&p);

        if (ip < bases->func + start) {
            // no more source ranges
            return FALSE;
        } 
        else if (ip < bases->func + start + len) {
            // found the range
            if (!pad) return FALSE;  // ...but it has no landing pad
            // found the landing pad
            action_record = action ? action_record_table + action - 1 : 0;
            *try_start = bases->func + start;
            *try_end = bases->func + start + len;
            break;
        }        
    }
    
    if (!action_record) return FALSE;  // no catch handlers

    // has handlers, destructors, and/or throws specifications
    // Use this frame if it has any handlers
    EJSBool has_handler = FALSE;
    p = action_record;
    intptr_t offset;
    do {
        intptr_t filter = read_sleb(&p);
        uintptr_t temp = p;
        offset = read_sleb(&temp);
        p += offset;
        
        if (filter < 0) {
            // throws specification - ignore
        } else if (filter == 0) {
            // destructor - ignore
        } else /* filter >= 0 */ {
            // catch handler - use this frame
            has_handler = EJS_TRUE;
            break;
        }
    } while (offset);
    
    return has_handler;
}


struct frame_range {
    uintptr_t ip_start;
    uintptr_t ip_end;
    uintptr_t cfa;
};

static struct frame_range findHandler(void)
{
    // walk stack looking for frame with ejs catch handler
    unw_context_t    uc;
    unw_cursor_t    cursor; 
    unw_proc_info_t    info;
    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    while ( (unw_step(&cursor) > 0) && (unw_get_proc_info(&cursor, &info) == UNW_ESUCCESS) ) {
        // must use ejs personality handler
        if ( info.handler != (uintptr_t)__ejs_personality_v0 )
            continue;
        // must have landing pad
        if ( info.lsda == 0 )
            continue;
        // must have landing pad that catches ejs exceptions
        struct dwarf_eh_bases bases;
        bases.tbase = 0;  // from unwind-dw2-fde-darwin.c:examine_objects()
        bases.dbase = 0;  // from unwind-dw2-fde-darwin.c:examine_objects()
        bases.func = info.start_ip;
        unw_word_t ip;
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        uintptr_t try_start;
        uintptr_t try_end;
        if ( isEjsExceptionCatcher(info.lsda, ip, &bases, &try_start, &try_end) ) {
            unw_word_t cfa;
            unw_get_reg(&cursor, UNW_REG_SP, &cfa);
            return (struct frame_range){try_start, try_end, cfa};
        }
    }

    return (struct frame_range){0, 0, 0};
}

#if false

// This data structure assumes the number of 
// active alt handlers per frame is small.
struct alt_handler_data {
    uintptr_t ip_start;
    uintptr_t ip_end;
    uintptr_t cfa;
    ejs_exception_handler fn;
    void *context;
};

struct alt_handler_list {
    unsigned int allocated;
    unsigned int used;
    struct alt_handler_data *handlers;
};


static struct alt_handler_list *
fetch_handler_list(EJSBool create)
{
    _ejs_pthread_data *data = _ejs_fetch_pthread_data(create);
    if (!data) return NULL;

    struct alt_handler_list *list = data->handlerList;
    if (!list) {
        if (!create) return NULL;
        list = _calloc_internal(1, sizeof(*list));
        data->handlerList = list;
    }

    return list;
}


__private_extern__ void _destroyAltHandlerList(struct alt_handler_list *list)
{
    if (list) {
        if (list->handlers) {
            _free_internal(list->handlers);
        }
        _free_internal(list);
    }
}

uintptr_t _ejs_addExceptionHandler(ejs_exception_handler fn, void *context)
{ 
    // Find the closest enclosing frame with ejs catch handlers
    struct frame_range target_frame = findHandler();
    if (!target_frame.ip_start) {
        // No suitable enclosing handler found.
        return 0;
    }

    // Record this alt handler for the discovered frame.
    struct alt_handler_list *list = fetch_handler_list(YES);
    unsigned int i = 0;

    if (list->used == list->allocated) {
        list->allocated = list->allocated*2 ?: 4;
        list->handlers = _realloc_internal(list->handlers, list->allocated * sizeof(list->handlers[0]));
        bzero(&list->handlers[list->used], (list->allocated - list->used) * sizeof(list->handlers[0]));
        i = list->used;
    }
    else {
        for (i = 0; i < list->allocated; i++) {
            if (list->handlers[i].ip_start == 0  &&  
                list->handlers[i].ip_end == 0  &&  
                list->handlers[i].cfa == 0) 
                {
                    break;
                }
        }
        if (i == list->allocated) {
            _ejs_fatal("alt handlers in ejs runtime are buggy!");
        }
    }

    struct alt_handler_data *data = &list->handlers[i];

    data->ip_start = target_frame.ip_start;
    data->ip_end = target_frame.ip_end;
    data->cfa = target_frame.cfa;
    data->fn = fn;
    data->context = context;
    list->used++;

    if (PrintAltHandlers) {
        SPEW(printf("ALT HANDLERS: installing alt handler %d %p(%p) on "
                    "frame [ip=%p..%p sp=%p]\n", i+1, data->fn, data->context, 
                    (void *)data->ip_start, (void *)data->ip_end, 
                    (void *)data->cfa));
    }

    if (list->used > 1000) {
        static int warned = 0;
        if (!warned) {
            SPEW(printf("ALT HANDLERS: *** over 1000 alt handlers installed; "
                        "this is probably a bug\n"));
            warned = 1;
        }
    }

    return i+1;
}


void _ejs_removeExceptionHandler(uintptr_t token)
{
    if (!token) {
        // ejs_addExceptionHandler failed
        return;
    }
    unsigned int i = (unsigned int)(token - 1);
    
    struct alt_handler_list *list = fetch_handler_list(NO);
    if (!list  ||  list->used == 0) {
        // no handlers present
        SPEW(printf("ALT HANDLERS: *** can't remove alt handler %lu "
                    "(no alt handlers present)\n", token));
        return;
    }
    if (i >= list->allocated) {
        // bogus token
        SPEW(printf("ALT HANDLERS: *** can't remove alt handler %lu "
                    "(current max is %u)\n", token, list->allocated));
        return;
    }

    struct alt_handler_data *data = &list->handlers[i];
    SPEW(printf("ALT HANDLERS: removing   alt handler %d %p(%p) on "
                "frame [ip=%p..%p sp=%p]\n", i+1, data->fn, data->context, 
                (void *)data->ip_start, (void *)data->ip_end, 
                (void *)data->cfa));
    bzero(data, sizeof(*data));
    list->used--;
}

// called in order registered, to match 32-bit _NSAddAltHandler2
// fixme reverse registration order matches c++ destructors better
static void call_alt_handlers(struct _Unwind_Context *ctx)
{
    uintptr_t ip = _Unwind_GetIP(ctx) - 1;
    uintptr_t cfa = _Unwind_GetCFA(ctx);
    unsigned int i;
    
    struct alt_handler_list *list = fetch_handler_list(FALSE);
    if (!list  ||  list->used == 0) return;

    for (i = 0; i < list->allocated; i++) {
        struct alt_handler_data *data = &list->handlers[i];
        if (ip >= data->ip_start  &&  ip < data->ip_end  &&  data->cfa == cfa) 
            {
                // Copy and clear before the callback, in case the 
                // callback manipulates the alt handler list.
                struct alt_handler_data copy = *data;
                bzero(data, sizeof(*data));
                list->used--;
                SPEW(printf("EXCEPTIONS: calling alt handler %p(%p) from "
                            "frame [ip=%p..%p sp=%p]\n", copy.fn, copy.context, 
                            (void *)copy.ip_start, (void *)copy.ip_end, 
                            (void *)copy.cfa));
                if (copy.fn) (*copy.fn)(nil, copy.context);
            }
    }
}
#endif

// ! NO_ZEROCOST_EXCEPTIONS
#endif


void _ejs_exception_init(void)
{
    // call std::set_terminate
    old_terminate = _ZSt13set_terminatePFvvE(&_ejs_terminate);
}
