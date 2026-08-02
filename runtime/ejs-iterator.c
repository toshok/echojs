/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>
#include <stdlib.h>

#include "ejs-iterator.h"
#include "ejs-array.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-gc.h"
#include "ejs-generator.h"
#include "ejs-ops.h"
#include "ejs-string.h"
#include "ejs-symbol.h"

ejsval _ejs_Iterator EJSVAL_ALIGNMENT;
ejsval _ejs_IteratorHelper_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_WrapForValidIterator_prototype EJSVAL_ALIGNMENT;

#define MAX_SAFE_INTEGER_D 9007199254740991.0

// ------------------------------------------------------------------------
// abrupt-completion plumbing
//
// helper bodies must observe abrupt completions from arbitrary JS
// (next/done/value/return getters, callbacks) so they can complete the
// helper and close iterators the way the spec's generator closures do,
// so every JS re-entry point below runs under a catch.
// ------------------------------------------------------------------------

typedef struct { ejsval a; ejsval b; } EJSValPair;

static ejsval
get_thunk (void* data)
{
    EJSValPair* p = (EJSValPair*)data;
    return Get (p->a, p->b);
}

// EJS_FALSE with the exception in *out on abrupt completion
static EJSBool
Get_catch (ejsval* out, ejsval O, ejsval P)
{
    EJSValPair pair = { O, P };
    return _ejs_invoke_func_catch (out, get_thunk, &pair);
}

static ejsval
getmethod_thunk (void* data)
{
    EJSValPair* p = (EJSValPair*)data;
    return GetMethod (p->a, p->b);
}

static EJSBool
GetMethod_catch (ejsval* out, ejsval O, ejsval P)
{
    EJSValPair pair = { O, P };
    return _ejs_invoke_func_catch (out, getmethod_thunk, &pair);
}

static ejsval
tonumber_thunk (void* data)
{
    return ToNumber (*(ejsval*)data);
}

static EJSBool
ToNumber_catch (ejsval* out, ejsval v)
{
    return _ejs_invoke_func_catch (out, tonumber_thunk, &v);
}

static ejsval
tostring_thunk (void* data)
{
    return ToString (*(ejsval*)data);
}

static EJSBool
ToString_catch (ejsval* out, ejsval v)
{
    return _ejs_invoke_func_catch (out, tostring_thunk, &v);
}

static EJSBool
Call_catch (ejsval* out, ejsval fn, ejsval thisv, uint32_t argc, ejsval* args)
{
    return _ejs_invoke_closure_catch (out, fn, &thisv, argc, args, _ejs_undefined);
}

// ------------------------------------------------------------------------
// IteratorClose
// ------------------------------------------------------------------------

// IteratorClose with a throw completion: the original error wins;
// failures while looking up or calling return are swallowed.
static void
close_iterator_throw (ejsval iterator, ejsval error) __attribute__ ((noreturn));

static void
close_iterator_throw (ejsval iterator, ejsval error)
{
    ejsval method;
    if (GetMethod_catch (&method, iterator, _ejs_atom_return) && !EJSVAL_IS_UNDEFINED(method)) {
        ejsval rv;
        Call_catch (&rv, method, iterator, 0, NULL);
    }
    _ejs_throw (error);
}

// IteratorClose with a normal completion: errors propagate, and a
// non-object result from return() is a TypeError.
static void
close_iterator_normal (ejsval iterator)
{
    ejsval method = GetMethod (iterator, _ejs_atom_return);
    if (EJSVAL_IS_UNDEFINED(method))
        return;
    ejsval rv = _ejs_invoke_closure (method, &iterator, 0, NULL, _ejs_undefined);
    if (!EJSVAL_IS_OBJECT(rv))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "iterator's return method returned a non-object");
}

// argument validation failed after the receiver check: close the
// not-yet-started iterator record and throw
static void
validation_failure_close (ejsval iterator, EJSNativeErrorType error_type, const char* msg) __attribute__ ((noreturn));

static void
validation_failure_close (ejsval iterator, EJSNativeErrorType error_type, const char* msg)
{
    close_iterator_throw (iterator, _ejs_nativeerror_new_utf8 (error_type, msg));
}

// ------------------------------------------------------------------------
// iterator record plumbing
// ------------------------------------------------------------------------

// GetIteratorDirect's observable half: read next once so every
// subsequent step reuses it
static ejsval
get_next_method (ejsval iterator)
{
    return Get (iterator, _ejs_atom_next);
}

// IteratorStepValue over a cached next method.  EJS_TRUE with *value_out
// on a value, EJS_FALSE at exhaustion.  abrupt completions mark the
// helper (when given) completed and rethrow without closing.
static EJSBool
iterator_step (EJSIteratorHelper* helper, ejsval iterator, ejsval next_method, ejsval* value_out)
{
    ejsval error;

    ejsval result;
    if (!Call_catch (&result, next_method, iterator, 0, NULL)) {
        error = result;
        goto abrupt;
    }
    if (!EJSVAL_IS_OBJECT(result)) {
        error = _ejs_nativeerror_new_utf8 (EJS_TYPE_ERROR, "iterator result is not an object");
        goto abrupt;
    }

    ejsval donev;
    if (!Get_catch (&donev, result, _ejs_atom_done)) {
        error = donev;
        goto abrupt;
    }
    if (ToEJSBool (donev))
        return EJS_FALSE;

    ejsval value;
    if (!Get_catch (&value, result, _ejs_atom_value)) {
        error = value;
        goto abrupt;
    }
    *value_out = value;
    return EJS_TRUE;

abrupt:
    if (helper) {
        helper->done = EJS_TRUE;
        helper->running = EJS_FALSE;
    }
    _ejs_throw (error);
}

// ------------------------------------------------------------------------
// Iterator Helper objects
// ------------------------------------------------------------------------

static EJSIteratorHelper*
iterator_helper_new (EJSIteratorHelperKind kind)
{
    EJSIteratorHelper* helper = _ejs_gc_new (EJSIteratorHelper);
    _ejs_init_object ((EJSObject*)helper, _ejs_IteratorHelper_prototype, &_ejs_IteratorHelper_specops);
    helper->kind = kind;
    helper->iterator = _ejs_undefined;
    helper->next_method = _ejs_undefined;
    helper->fn = _ejs_undefined;
    helper->inner = _ejs_undefined;
    helper->inner_next = _ejs_undefined;
    return helper;
}

static EJSIteratorHelper*
iterator_helper_check (ejsval v, const char* msg)
{
    if (!EJSVAL_IS_ITERATOR_HELPER(v))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, msg);
    return (EJSIteratorHelper*)EJSVAL_TO_OBJECT(v);
}

// invoke helper->fn(value, counter), incrementing the counter.  abrupt
// completions complete the helper and close the underlying iterator
// with the error.
static ejsval
helper_call_fn (EJSIteratorHelper* helper, ejsval value)
{
    ejsval call_args[2];
    call_args[0] = value;
    call_args[1] = NUMBER_TO_EJSVAL(helper->counter);
    helper->counter += 1;

    ejsval rv;
    if (!Call_catch (&rv, helper->fn, _ejs_undefined, 2, call_args)) {
        helper->done = EJS_TRUE;
        helper->running = EJS_FALSE;
        close_iterator_throw (helper->iterator, rv);
    }
    return rv;
}

// flatMap: GetIteratorFlattenable(mapped, reject-primitives), closing
// the outer iterator on any failure
static void
helper_open_inner (EJSIteratorHelper* helper, ejsval mapped)
{
    ejsval error;

    if (!EJSVAL_IS_OBJECT(mapped)) {
        error = _ejs_nativeerror_new_utf8 (EJS_TYPE_ERROR, "flatMap mapper must return an object");
        goto abrupt;
    }

    ejsval method;
    if (!GetMethod_catch (&method, mapped, _ejs_Symbol_iterator)) {
        error = method;
        goto abrupt;
    }

    ejsval inner;
    if (EJSVAL_IS_UNDEFINED(method)) {
        inner = mapped;
    }
    else {
        if (!Call_catch (&inner, method, mapped, 0, NULL)) {
            error = inner;
            goto abrupt;
        }
        if (!EJSVAL_IS_OBJECT(inner)) {
            error = _ejs_nativeerror_new_utf8 (EJS_TYPE_ERROR, "flatMap mapper result is not iterable");
            goto abrupt;
        }
    }

    ejsval inner_next;
    if (!Get_catch (&inner_next, inner, _ejs_atom_next)) {
        error = inner_next;
        goto abrupt;
    }

    helper->inner = inner;
    helper->inner_next = inner_next;
    _ejs_gc_remember (helper, inner);
    _ejs_gc_remember (helper, inner_next);
    return;

abrupt:
    helper->done = EJS_TRUE;
    helper->running = EJS_FALSE;
    close_iterator_throw (helper->iterator, error);
}

// concat: open the next queued iterable, or return EJS_FALSE when none
// remain
static EJSBool
helper_concat_open_next (EJSIteratorHelper* helper)
{
    if (helper->concat_index >= helper->concat_count)
        return EJS_FALSE;

    uint32_t i = helper->concat_index++;
    ejsval item = helper->concat_pairs[2 * i];
    ejsval method = helper->concat_pairs[2 * i + 1];

    ejsval error;

    ejsval inner;
    if (!Call_catch (&inner, method, item, 0, NULL)) {
        error = inner;
        goto abrupt;
    }
    if (!EJSVAL_IS_OBJECT(inner)) {
        error = _ejs_nativeerror_new_utf8 (EJS_TYPE_ERROR, "Iterator.concat: @@iterator method returned a non-object");
        goto abrupt;
    }

    ejsval inner_next;
    if (!Get_catch (&inner_next, inner, _ejs_atom_next)) {
        error = inner_next;
        goto abrupt;
    }

    helper->inner = inner;
    helper->inner_next = inner_next;
    _ejs_gc_remember (helper, inner);
    _ejs_gc_remember (helper, inner_next);
    return EJS_TRUE;

abrupt:
    helper->done = EJS_TRUE;
    helper->running = EJS_FALSE;
    _ejs_throw (error);
}

// step the underlying iterator, completing the helper (without closing)
// at exhaustion.  EJS_FALSE means the caller should return done.
static EJSBool
helper_step_underlying (EJSIteratorHelper* helper, ejsval* value_out)
{
    if (iterator_step (helper, helper->iterator, helper->next_method, value_out))
        return EJS_TRUE;
    helper->done = EJS_TRUE;
    return EJS_FALSE;
}

static ejsval
done_result ()
{
    return _ejs_create_iter_result (_ejs_undefined, _ejs_true);
}

static ejsval
helper_yield_buffer (EJSIteratorHelper* helper, uint32_t start, uint32_t count)
{
    ejsval chunk = _ejs_array_new_copy (count, helper->buffer + start);
    return _ejs_create_iter_result (chunk, _ejs_false);
}

static EJS_NATIVE_FUNC(_ejs_IteratorHelper_prototype_next) {
    EJSIteratorHelper* helper = iterator_helper_check (*_this, "Iterator Helper.prototype.next called with incompatible this");

    if (helper->done)
        return done_result();
    if (helper->running)
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Iterator Helper is already running");
    helper->running = EJS_TRUE;

    ejsval rv = _ejs_undefined;
    ejsval value = _ejs_undefined;

    switch (helper->kind) {
    case EJS_ITERATOR_HELPER_MAP: {
        if (!helper_step_underlying (helper, &value)) { rv = done_result(); break; }
        ejsval mapped = helper_call_fn (helper, value);
        rv = _ejs_create_iter_result (mapped, _ejs_false);
        break;
    }
    case EJS_ITERATOR_HELPER_FILTER: {
        for (;;) {
            if (!helper_step_underlying (helper, &value)) { rv = done_result(); break; }
            ejsval selected = helper_call_fn (helper, value);
            if (ToEJSBool (selected)) {
                rv = _ejs_create_iter_result (value, _ejs_false);
                break;
            }
        }
        break;
    }
    case EJS_ITERATOR_HELPER_TAKE: {
        if (helper->limit <= 0) {
            helper->done = EJS_TRUE;
            helper->running = EJS_FALSE;
            close_iterator_normal (helper->iterator);
            return done_result();
        }
        if (helper->limit != INFINITY)
            helper->limit -= 1;
        if (!helper_step_underlying (helper, &value)) { rv = done_result(); break; }
        rv = _ejs_create_iter_result (value, _ejs_false);
        break;
    }
    case EJS_ITERATOR_HELPER_DROP: {
        if (!helper->skipped) {
            helper->skipped = EJS_TRUE;
            double remaining = helper->limit;
            while (remaining > 0) {
                if (remaining != INFINITY)
                    remaining -= 1;
                if (!helper_step_underlying (helper, &value)) break;
            }
            if (helper->done) { rv = done_result(); break; }
        }
        if (!helper_step_underlying (helper, &value)) { rv = done_result(); break; }
        rv = _ejs_create_iter_result (value, _ejs_false);
        break;
    }
    case EJS_ITERATOR_HELPER_FLATMAP: {
        for (;;) {
            if (!EJSVAL_IS_UNDEFINED(helper->inner)) {
                // inner-step abrupt completions close the outer iterator
                ejsval error;
                ejsval result;
                if (!Call_catch (&result, helper->inner_next, helper->inner, 0, NULL)) {
                    error = result;
                    goto flatmap_abrupt;
                }
                if (!EJSVAL_IS_OBJECT(result)) {
                    error = _ejs_nativeerror_new_utf8 (EJS_TYPE_ERROR, "iterator result is not an object");
                    goto flatmap_abrupt;
                }
                ejsval donev;
                if (!Get_catch (&donev, result, _ejs_atom_done)) {
                    error = donev;
                    goto flatmap_abrupt;
                }
                if (ToEJSBool (donev)) {
                    helper->inner = _ejs_undefined;
                    helper->inner_next = _ejs_undefined;
                    continue;
                }
                ejsval inner_value;
                if (!Get_catch (&inner_value, result, _ejs_atom_value)) {
                    error = inner_value;
                    goto flatmap_abrupt;
                }
                rv = _ejs_create_iter_result (inner_value, _ejs_false);
                break;

            flatmap_abrupt:
                helper->done = EJS_TRUE;
                helper->running = EJS_FALSE;
                close_iterator_throw (helper->iterator, error);
            }
            if (!helper_step_underlying (helper, &value)) { rv = done_result(); break; }
            ejsval mapped = helper_call_fn (helper, value);
            helper_open_inner (helper, mapped);
        }
        break;
    }
    case EJS_ITERATOR_HELPER_CHUNKS: {
        uint32_t size = (uint32_t)helper->limit;
        if (helper->underlying_done) {
            helper->done = EJS_TRUE;
            rv = done_result();
            break;
        }
        for (;;) {
            if (!iterator_step (helper, helper->iterator, helper->next_method, &value)) {
                helper->underlying_done = EJS_TRUE;
                if (helper->buffer_count > 0) {
                    uint32_t count = helper->buffer_count;
                    helper->buffer_count = 0;
                    rv = helper_yield_buffer (helper, 0, count);
                }
                else {
                    helper->done = EJS_TRUE;
                    rv = done_result();
                }
                break;
            }
            helper->buffer[helper->buffer_count++] = value;
            _ejs_gc_remember (helper, value);
            if (helper->buffer_count == size) {
                helper->buffer_count = 0;
                rv = helper_yield_buffer (helper, 0, size);
                break;
            }
        }
        break;
    }
    case EJS_ITERATOR_HELPER_WINDOWS: {
        uint32_t size = (uint32_t)helper->limit;
        if (helper->underlying_done) {
            helper->done = EJS_TRUE;
            rv = done_result();
            break;
        }
        for (;;) {
            if (!iterator_step (helper, helper->iterator, helper->next_method, &value)) {
                helper->underlying_done = EJS_TRUE;
                // allow-partial: a source shorter than the window still
                // yields one undersized window
                if (helper->allow_partial && helper->buffer_count > 0 && !helper->window_yielded) {
                    helper->window_yielded = EJS_TRUE;
                    rv = helper_yield_buffer (helper, 0, helper->buffer_count);
                }
                else {
                    helper->done = EJS_TRUE;
                    rv = done_result();
                }
                break;
            }
            if (helper->buffer_count == size) {
                // slide: discard the oldest value
                for (uint32_t i = 1; i < size; i++)
                    helper->buffer[i - 1] = helper->buffer[i];
                helper->buffer_count = size - 1;
            }
            helper->buffer[helper->buffer_count++] = value;
            _ejs_gc_remember (helper, value);
            if (helper->buffer_count == size) {
                helper->window_yielded = EJS_TRUE;
                rv = helper_yield_buffer (helper, 0, size);
                break;
            }
        }
        break;
    }
    case EJS_ITERATOR_HELPER_CONCAT: {
        for (;;) {
            if (EJSVAL_IS_UNDEFINED(helper->inner)) {
                if (!helper_concat_open_next (helper)) {
                    helper->done = EJS_TRUE;
                    rv = done_result();
                    break;
                }
            }
            if (!iterator_step (helper, helper->inner, helper->inner_next, &value)) {
                helper->inner = _ejs_undefined;
                helper->inner_next = _ejs_undefined;
                continue;
            }
            rv = _ejs_create_iter_result (value, _ejs_false);
            break;
        }
        break;
    }
    }

    helper->running = EJS_FALSE;
    return rv;
}

static EJS_NATIVE_FUNC(_ejs_IteratorHelper_prototype_return) {
    EJSIteratorHelper* helper = iterator_helper_check (*_this, "Iterator Helper.prototype.return called with incompatible this");

    if (helper->running)
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Iterator Helper is already running");
    if (helper->done)
        return done_result();

    helper->done = EJS_TRUE;

    if (helper->kind == EJS_ITERATOR_HELPER_CONCAT) {
        // inner iterators open lazily; nothing to forward to before the
        // first is opened or after one is exhausted
        if (!EJSVAL_IS_UNDEFINED(helper->inner))
            close_iterator_normal (helper->inner);
        return done_result();
    }

    if (helper->kind == EJS_ITERATOR_HELPER_FLATMAP && !EJSVAL_IS_UNDEFINED(helper->inner)) {
        // close the inner iterator first; an error there still closes
        // the outer before propagating
        ejsval method;
        if (!GetMethod_catch (&method, helper->inner, _ejs_atom_return))
            close_iterator_throw (helper->iterator, method);
        if (!EJSVAL_IS_UNDEFINED(method)) {
            ejsval inner_rv;
            if (!Call_catch (&inner_rv, method, helper->inner, 0, NULL))
                close_iterator_throw (helper->iterator, inner_rv);
        }
    }

    close_iterator_normal (helper->iterator);
    return done_result();
}

// ------------------------------------------------------------------------
// %Iterator.prototype% helper methods
// ------------------------------------------------------------------------

// shared prelude: receiver must be an object, fn must be callable
// (closing the not-yet-started receiver if not), then GetIteratorDirect
static EJSIteratorHelper*
helper_from_callable (ejsval O, ejsval fn, EJSIteratorHelperKind kind, const char* not_object_msg, const char* not_callable_msg)
{
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, not_object_msg);
    if (!IsCallable(fn))
        validation_failure_close (O, EJS_TYPE_ERROR, not_callable_msg);

    ejsval next = get_next_method (O);

    EJSIteratorHelper* helper = iterator_helper_new (kind);
    helper->iterator = O;
    helper->next_method = next;
    helper->fn = fn;
    _ejs_gc_remember (helper, O);
    _ejs_gc_remember (helper, next);
    _ejs_gc_remember (helper, fn);
    return helper;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_map) {
    ejsval mapper = argc > 0 ? args[0] : _ejs_undefined;
    EJSIteratorHelper* helper = helper_from_callable (*_this, mapper, EJS_ITERATOR_HELPER_MAP,
                                                      "Iterator.prototype.map called on non-object",
                                                      "Iterator.prototype.map mapper isn't a function");
    return OBJECT_TO_EJSVAL((EJSObject*)helper);
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_filter) {
    ejsval predicate = argc > 0 ? args[0] : _ejs_undefined;
    EJSIteratorHelper* helper = helper_from_callable (*_this, predicate, EJS_ITERATOR_HELPER_FILTER,
                                                      "Iterator.prototype.filter called on non-object",
                                                      "Iterator.prototype.filter predicate isn't a function");
    return OBJECT_TO_EJSVAL((EJSObject*)helper);
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_flatMap) {
    ejsval mapper = argc > 0 ? args[0] : _ejs_undefined;
    EJSIteratorHelper* helper = helper_from_callable (*_this, mapper, EJS_ITERATOR_HELPER_FLATMAP,
                                                      "Iterator.prototype.flatMap called on non-object",
                                                      "Iterator.prototype.flatMap mapper isn't a function");
    return OBJECT_TO_EJSVAL((EJSObject*)helper);
}

// take/drop limit validation: ToNumber, then RangeError (closing the
// receiver) for NaN, out-of-safe-range, or negative limits
static double
validate_limit (ejsval O, ejsval limit, const char* not_object_msg)
{
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, not_object_msg);

    ejsval numv;
    if (!ToNumber_catch (&numv, limit))
        close_iterator_throw (O, numv);

    double num = EJSVAL_TO_NUMBER(numv);
    if (isnan (num))
        validation_failure_close (O, EJS_RANGE_ERROR, "limit must not be NaN");
    if (isfinite (num) && num > MAX_SAFE_INTEGER_D)
        validation_failure_close (O, EJS_RANGE_ERROR, "limit is too large");

    // ToIntegerOrInfinity
    double integer = isinf (num) ? num : trunc (num);
    if (integer < 0)
        validation_failure_close (O, EJS_RANGE_ERROR, "limit must not be negative");

    return integer;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_take) {
    ejsval limit = argc > 0 ? args[0] : _ejs_undefined;
    double integer_limit = validate_limit (*_this, limit, "Iterator.prototype.take called on non-object");

    ejsval next = get_next_method (*_this);

    EJSIteratorHelper* helper = iterator_helper_new (EJS_ITERATOR_HELPER_TAKE);
    helper->iterator = *_this;
    helper->next_method = next;
    helper->limit = integer_limit;
    _ejs_gc_remember (helper, *_this);
    _ejs_gc_remember (helper, next);
    return OBJECT_TO_EJSVAL((EJSObject*)helper);
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_drop) {
    ejsval limit = argc > 0 ? args[0] : _ejs_undefined;
    double integer_limit = validate_limit (*_this, limit, "Iterator.prototype.drop called on non-object");

    ejsval next = get_next_method (*_this);

    EJSIteratorHelper* helper = iterator_helper_new (EJS_ITERATOR_HELPER_DROP);
    helper->iterator = *_this;
    helper->next_method = next;
    helper->limit = integer_limit;
    _ejs_gc_remember (helper, *_this);
    _ejs_gc_remember (helper, next);
    return OBJECT_TO_EJSVAL((EJSObject*)helper);
}

// chunks/windows size validation: no coercion — the size must already
// be an integral Number in [1, 2^32-1]
static uint32_t
validate_chunk_size (ejsval O, ejsval size, const char* not_object_msg)
{
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, not_object_msg);

    if (!EJSVAL_IS_NUMBER(size))
        validation_failure_close (O, EJS_TYPE_ERROR, "size must be a Number");

    double num = EJSVAL_TO_NUMBER(size);
    if (isnan (num) || isinf (num) || trunc (num) != num)
        validation_failure_close (O, EJS_TYPE_ERROR, "size must be an integral Number");
    if (num < 1 || num > 4294967295.0)
        validation_failure_close (O, EJS_RANGE_ERROR, "size out of range");

    return (uint32_t)num;
}

static ejsval
buffered_helper_new (ejsval O, uint32_t size, EJSIteratorHelperKind kind)
{
    ejsval next = get_next_method (O);

    EJSIteratorHelper* helper = iterator_helper_new (kind);
    helper->iterator = O;
    helper->next_method = next;
    helper->limit = size;
    helper->buffer = (ejsval*)calloc (size, sizeof (ejsval));
    _ejs_gc_remember (helper, O);
    _ejs_gc_remember (helper, next);
    return OBJECT_TO_EJSVAL((EJSObject*)helper);
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_chunks) {
    ejsval size = argc > 0 ? args[0] : _ejs_undefined;
    uint32_t chunk_size = validate_chunk_size (*_this, size, "Iterator.prototype.chunks called on non-object");
    return buffered_helper_new (*_this, chunk_size, EJS_ITERATOR_HELPER_CHUNKS);
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_windows) {
    ejsval size = argc > 0 ? args[0] : _ejs_undefined;
    ejsval undersized = argc > 1 ? args[1] : _ejs_undefined;
    uint32_t window_size = validate_chunk_size (*_this, size, "Iterator.prototype.windows called on non-object");

    // undersized: undefined means "only-full"
    EJSBool allow_partial = EJS_FALSE;
    if (!EJSVAL_IS_UNDEFINED(undersized)) {
        EJSBool only_full = EJS_FALSE;
        if (EJSVAL_IS_STRING(undersized)) {
            only_full = SameValueZero (undersized, _ejs_string_new_utf8 ("only-full"));
            allow_partial = SameValueZero (undersized, _ejs_string_new_utf8 ("allow-partial"));
        }
        if (!only_full && !allow_partial)
            validation_failure_close (*_this, EJS_TYPE_ERROR, "undersized must be \"only-full\" or \"allow-partial\"");
    }

    ejsval helperv = buffered_helper_new (*_this, window_size, EJS_ITERATOR_HELPER_WINDOWS);
    ((EJSIteratorHelper*)EJSVAL_TO_OBJECT(helperv))->allow_partial = allow_partial;
    return helperv;
}

// ------------------------------------------------------------------------
// %Iterator.prototype% terminal methods
// ------------------------------------------------------------------------

// callback invocation for the terminal loops: abrupt completions close
// the iterator with the error
static ejsval
terminal_call_fn (ejsval iterator, ejsval fn, ejsval value, double counter)
{
    ejsval call_args[2];
    call_args[0] = value;
    call_args[1] = NUMBER_TO_EJSVAL(counter);
    ejsval rv;
    if (!Call_catch (&rv, fn, _ejs_undefined, 2, call_args))
        close_iterator_throw (iterator, rv);
    return rv;
}

static void
terminal_prelude (ejsval O, ejsval fn, const char* not_object_msg, const char* not_callable_msg)
{
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, not_object_msg);
    if (!IsCallable(fn))
        validation_failure_close (O, EJS_TYPE_ERROR, not_callable_msg);
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_reduce) {
    ejsval reducer = argc > 0 ? args[0] : _ejs_undefined;

    terminal_prelude (*_this, reducer,
                      "Iterator.prototype.reduce called on non-object",
                      "Iterator.prototype.reduce reducer isn't a function");

    ejsval O = *_this;
    ejsval next = get_next_method (O);

    ejsval accumulator;
    double counter;
    if (argc < 2) {
        if (!iterator_step (NULL, O, next, &accumulator))
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "reduce of empty iterator with no initial value");
        counter = 1;
    }
    else {
        accumulator = args[1];
        counter = 0;
    }

    ejsval value;
    while (iterator_step (NULL, O, next, &value)) {
        ejsval call_args[3];
        call_args[0] = accumulator;
        call_args[1] = value;
        call_args[2] = NUMBER_TO_EJSVAL(counter);
        ejsval rv;
        if (!Call_catch (&rv, reducer, _ejs_undefined, 3, call_args))
            close_iterator_throw (O, rv);
        accumulator = rv;
        counter += 1;
    }
    return accumulator;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_toArray) {
    if (!EJSVAL_IS_OBJECT(*_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Iterator.prototype.toArray called on non-object");

    ejsval O = *_this;
    ejsval next = get_next_method (O);

    ejsval array = _ejs_array_new (0, EJS_FALSE);
    ejsval value;
    while (iterator_step (NULL, O, next, &value))
        _ejs_array_push_dense (array, 1, &value);
    return array;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_forEach) {
    ejsval fn = argc > 0 ? args[0] : _ejs_undefined;
    terminal_prelude (*_this, fn,
                      "Iterator.prototype.forEach called on non-object",
                      "Iterator.prototype.forEach callback isn't a function");

    ejsval O = *_this;
    ejsval next = get_next_method (O);

    double counter = 0;
    ejsval value;
    while (iterator_step (NULL, O, next, &value))
        terminal_call_fn (O, fn, value, counter++);
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_some) {
    ejsval predicate = argc > 0 ? args[0] : _ejs_undefined;
    terminal_prelude (*_this, predicate,
                      "Iterator.prototype.some called on non-object",
                      "Iterator.prototype.some predicate isn't a function");

    ejsval O = *_this;
    ejsval next = get_next_method (O);

    double counter = 0;
    ejsval value;
    while (iterator_step (NULL, O, next, &value)) {
        if (ToEJSBool (terminal_call_fn (O, predicate, value, counter++))) {
            close_iterator_normal (O);
            return _ejs_true;
        }
    }
    return _ejs_false;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_every) {
    ejsval predicate = argc > 0 ? args[0] : _ejs_undefined;
    terminal_prelude (*_this, predicate,
                      "Iterator.prototype.every called on non-object",
                      "Iterator.prototype.every predicate isn't a function");

    ejsval O = *_this;
    ejsval next = get_next_method (O);

    double counter = 0;
    ejsval value;
    while (iterator_step (NULL, O, next, &value)) {
        if (!ToEJSBool (terminal_call_fn (O, predicate, value, counter++))) {
            close_iterator_normal (O);
            return _ejs_false;
        }
    }
    return _ejs_true;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_find) {
    ejsval predicate = argc > 0 ? args[0] : _ejs_undefined;
    terminal_prelude (*_this, predicate,
                      "Iterator.prototype.find called on non-object",
                      "Iterator.prototype.find predicate isn't a function");

    ejsval O = *_this;
    ejsval next = get_next_method (O);

    double counter = 0;
    ejsval value;
    while (iterator_step (NULL, O, next, &value)) {
        if (ToEJSBool (terminal_call_fn (O, predicate, value, counter++))) {
            close_iterator_normal (O);
            return value;
        }
    }
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_includes) {
    ejsval search = argc > 0 ? args[0] : _ejs_undefined;
    ejsval skipv = argc > 1 ? args[1] : _ejs_undefined;

    if (!EJSVAL_IS_OBJECT(*_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Iterator.prototype.includes called on non-object");

    ejsval O = *_this;

    // skippedElements must already be +/-Infinity or an integral Number
    double to_skip = 0;
    if (!EJSVAL_IS_UNDEFINED(skipv)) {
        if (!EJSVAL_IS_NUMBER(skipv))
            validation_failure_close (O, EJS_TYPE_ERROR, "skippedElements must be a Number");
        double num = EJSVAL_TO_NUMBER(skipv);
        if (isnan (num) || (isfinite (num) && trunc (num) != num))
            validation_failure_close (O, EJS_TYPE_ERROR, "skippedElements must be an integral Number");
        to_skip = num;
    }
    if (to_skip < 0)
        validation_failure_close (O, EJS_RANGE_ERROR, "skippedElements must not be negative");
    if (isfinite (to_skip) && to_skip > MAX_SAFE_INTEGER_D)
        validation_failure_close (O, EJS_RANGE_ERROR, "skippedElements is too large");

    ejsval next = get_next_method (O);

    double skipped = 0;
    ejsval value;
    while (iterator_step (NULL, O, next, &value)) {
        if (skipped < to_skip) {
            skipped += 1;
            continue;
        }
        if (SameValueZero (value, search)) {
            close_iterator_normal (O);
            return _ejs_true;
        }
    }
    return _ejs_false;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_join) {
    ejsval separator = argc > 0 ? args[0] : _ejs_undefined;

    if (!EJSVAL_IS_OBJECT(*_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Iterator.prototype.join called on non-object");

    ejsval O = *_this;

    ejsval sep;
    if (EJSVAL_IS_UNDEFINED(separator))
        sep = _ejs_string_new_utf8 (",");
    else if (!ToString_catch (&sep, separator))
        close_iterator_throw (O, sep);

    ejsval next = get_next_method (O);

    ejsval result = _ejs_atom_empty;
    EJSBool first = EJS_TRUE;
    ejsval value;
    while (iterator_step (NULL, O, next, &value)) {
        if (!first)
            result = _ejs_string_concat (result, sep);
        first = EJS_FALSE;
        if (!EJSVAL_IS_NULL_OR_UNDEFINED(value)) {
            ejsval s;
            if (!ToString_catch (&s, value))
                close_iterator_throw (O, s);
            result = _ejs_string_concat (result, s);
        }
    }
    return result;
}

// %Iterator.prototype% [ @@dispose ] ( )
static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_dispose) {
    ejsval method = GetMethod (*_this, _ejs_atom_return);
    if (!EJSVAL_IS_UNDEFINED(method))
        _ejs_invoke_closure (method, _this, 0, NULL, _ejs_undefined);
    return _ejs_undefined;
}

// ------------------------------------------------------------------------
// weird accessors: constructor and @@toStringTag are accessor pairs
// whose setter emulates assignment on instances while protecting the
// prototype itself (SetterThatIgnoresPrototypeProperties)
// ------------------------------------------------------------------------

static void
setter_that_ignores_prototype_properties (ejsval thisv, ejsval home, ejsval key, ejsval v)
{
    if (!EJSVAL_IS_OBJECT(thisv))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "cannot assign on a non-object receiver");
    if (EJSVAL_EQ(thisv, home))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "cannot assign to a read-only property of the iterator prototype");

    EJSObject* obj = EJSVAL_TO_OBJECT(thisv);
    ejsval exc = _ejs_undefined;
    EJSPropertyDesc* desc = OP(obj, GetOwnProperty)(thisv, key, &exc);
    if (desc == NULL) {
        // CreateDataPropertyOrThrow
        if (!_ejs_object_define_value_property (thisv, key, v,
                                                EJS_PROP_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE))
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "could not define property");
    }
    else {
        if (!OP(obj, Set)(thisv, key, v, thisv))
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "could not set property");
    }
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_get_constructor) {
    return _ejs_Iterator;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_set_constructor) {
    ejsval v = argc > 0 ? args[0] : _ejs_undefined;
    setter_that_ignores_prototype_properties (*_this, _ejs_Iterator_prototype, _ejs_atom_constructor, v);
    return _ejs_undefined;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_get_toStringTag) {
    return _ejs_atom_Iterator;
}

static EJS_NATIVE_FUNC(_ejs_Iterator_prototype_set_toStringTag) {
    ejsval v = argc > 0 ? args[0] : _ejs_undefined;
    setter_that_ignores_prototype_properties (*_this, _ejs_Iterator_prototype, _ejs_Symbol_toStringTag, v);
    return _ejs_undefined;
}

// ------------------------------------------------------------------------
// the Iterator constructor
// ------------------------------------------------------------------------

// abstract: only usable as a superclass
static EJS_NATIVE_FUNC(_ejs_Iterator_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget) || EJSVAL_EQ(newTarget, _ejs_Iterator))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Iterator is abstract and cannot be constructed directly");

    ejsval O = OrdinaryCreateFromConstructor (newTarget, _ejs_Iterator_prototype, &_ejs_Object_specops);
    *_this = O;
    return O;
}

// Iterator.from ( O )
static EJS_NATIVE_FUNC(_ejs_Iterator_from) {
    ejsval O = argc > 0 ? args[0] : _ejs_undefined;

    // GetIteratorFlattenable(O, iterate-string-primitives)
    if (!EJSVAL_IS_OBJECT(O) && !EJSVAL_IS_STRING(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Iterator.from requires an object or string");

    ejsval method = GetMethod (O, _ejs_Symbol_iterator);

    ejsval iterator;
    if (EJSVAL_IS_UNDEFINED(method))
        iterator = O;
    else
        iterator = _ejs_invoke_closure (method, &O, 0, NULL, _ejs_undefined);

    if (!EJSVAL_IS_OBJECT(iterator))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Iterator.from: value is not iterable");

    ejsval next = get_next_method (iterator);

    // OrdinaryHasInstance(%Iterator%, iterator): conforming iterators
    // pass through unwrapped.  Iterator.prototype is non-writable and
    // non-configurable, so the prototype-chain walk checks it directly.
    ejsval proto = EJSVAL_TO_OBJECT(iterator)->proto;
    while (EJSVAL_IS_OBJECT(proto)) {
        if (EJSVAL_EQ(proto, _ejs_Iterator_prototype))
            return iterator;
        proto = EJSVAL_TO_OBJECT(proto)->proto;
    }

    EJSWrapForValidIterator* wrap = _ejs_gc_new (EJSWrapForValidIterator);
    _ejs_init_object ((EJSObject*)wrap, _ejs_WrapForValidIterator_prototype, &_ejs_WrapForValidIterator_specops);
    wrap->iterator = iterator;
    wrap->next_method = next;
    _ejs_gc_remember (wrap, iterator);
    _ejs_gc_remember (wrap, next);
    return OBJECT_TO_EJSVAL((EJSObject*)wrap);
}

// Iterator.concat ( ...items )
static EJS_NATIVE_FUNC(_ejs_Iterator_concat) {
    // validate every argument up front, in order
    ejsval* pairs = argc ? (ejsval*)calloc (2 * (size_t)argc, sizeof (ejsval)) : NULL;

    for (uint32_t i = 0; i < argc; i++) {
        if (!EJSVAL_IS_OBJECT(args[i])) {
            free (pairs);
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Iterator.concat arguments must be objects");
        }
        ejsval method;
        if (!GetMethod_catch (&method, args[i], _ejs_Symbol_iterator)) {
            free (pairs);
            _ejs_throw (method);
        }
        if (EJSVAL_IS_UNDEFINED(method)) {
            free (pairs);
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Iterator.concat arguments must be iterable");
        }
        pairs[2 * i] = args[i];
        pairs[2 * i + 1] = method;
    }

    EJSIteratorHelper* helper = iterator_helper_new (EJS_ITERATOR_HELPER_CONCAT);
    helper->concat_pairs = pairs;
    helper->concat_count = argc;
    for (uint32_t i = 0; i < 2 * argc; i++)
        _ejs_gc_remember (helper, pairs[i]);
    return OBJECT_TO_EJSVAL((EJSObject*)helper);
}

// ------------------------------------------------------------------------
// %WrapForValidIteratorPrototype%
// ------------------------------------------------------------------------

static EJSWrapForValidIterator*
wrap_check (ejsval v, const char* msg)
{
    if (!EJSVAL_IS_WRAP_FOR_VALID_ITERATOR(v))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, msg);
    return (EJSWrapForValidIterator*)EJSVAL_TO_OBJECT(v);
}

static EJS_NATIVE_FUNC(_ejs_WrapForValidIterator_prototype_next) {
    EJSWrapForValidIterator* wrap = wrap_check (*_this, "next called with incompatible this");
    ejsval iterator = wrap->iterator;
    return _ejs_invoke_closure (wrap->next_method, &iterator, 0, NULL, _ejs_undefined);
}

static EJS_NATIVE_FUNC(_ejs_WrapForValidIterator_prototype_return) {
    EJSWrapForValidIterator* wrap = wrap_check (*_this, "return called with incompatible this");
    ejsval iterator = wrap->iterator;
    ejsval method = GetMethod (iterator, _ejs_atom_return);
    if (EJSVAL_IS_UNDEFINED(method))
        return _ejs_create_iter_result (_ejs_undefined, _ejs_true);
    return _ejs_invoke_closure (method, &iterator, 0, NULL, _ejs_undefined);
}

// ------------------------------------------------------------------------
// init
// ------------------------------------------------------------------------

void
_ejs_iterator_helpers_init (ejsval global)
{
    _ejs_Class_initialize (&_ejs_IteratorHelper_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WrapForValidIterator_specops, &_ejs_Object_specops);

    // the Iterator constructor, sharing the long-lived
    // %Iterator.prototype% every builtin iterator already chains to
    _ejs_gc_add_root (&_ejs_Iterator);
    _ejs_Iterator = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Iterator, _ejs_Iterator_impl);
    _ejs_object_define_value_property (global, _ejs_atom_Iterator, _ejs_Iterator,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

    _ejs_object_define_value_property (_ejs_Iterator, _ejs_atom_prototype, _ejs_Iterator_prototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_Iterator, _ejs_atom_length, NUMBER_TO_EJSVAL(0),
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    // ES6 17: @@iterator's spec name is "[Symbol.iterator]"; the shared
    // %Iterator.prototype% installer names it with the symbol value, so
    // restate it here
    ejsval iterator_fn = Get (_ejs_Iterator_prototype, _ejs_Symbol_iterator);
    _ejs_object_define_value_property (iterator_fn, _ejs_atom_name, _ejs_string_new_utf8 ("[Symbol.iterator]"),
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

#define STATIC_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Iterator, x, _ejs_Iterator_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    STATIC_METHOD(from);
    STATIC_METHOD(concat);

#undef STATIC_METHOD

    // constructor and @@toStringTag are accessor pairs with
    // prototype-protecting setters
    ejsval ctor_get = _ejs_function_new_native (_ejs_null, _ejs_atom_constructor, _ejs_Iterator_prototype_get_constructor);
    ejsval ctor_set = _ejs_function_new_native (_ejs_null, _ejs_atom_constructor, _ejs_Iterator_prototype_set_constructor);
    _ejs_object_define_accessor_property (_ejs_Iterator_prototype, _ejs_atom_constructor, ctor_get, ctor_set,
                                          EJS_PROP_FLAGS_GETTER_SET | EJS_PROP_FLAGS_SETTER_SET | EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE);

    ejsval tag_get = _ejs_function_new_native (_ejs_null, _ejs_Symbol_toStringTag, _ejs_Iterator_prototype_get_toStringTag);
    ejsval tag_set = _ejs_function_new_native (_ejs_null, _ejs_Symbol_toStringTag, _ejs_Iterator_prototype_set_toStringTag);
    _ejs_object_define_accessor_property (_ejs_Iterator_prototype, _ejs_Symbol_toStringTag, tag_get, tag_set,
                                          EJS_PROP_FLAGS_GETTER_SET | EJS_PROP_FLAGS_SETTER_SET | EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Iterator_prototype, x, _ejs_Iterator_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    PROTO_METHOD(map);
    PROTO_METHOD(filter);
    PROTO_METHOD(take);
    PROTO_METHOD(drop);
    PROTO_METHOD(flatMap);
    PROTO_METHOD(chunks);
    PROTO_METHOD(windows);
    PROTO_METHOD(reduce);
    PROTO_METHOD(toArray);
    PROTO_METHOD(forEach);
    PROTO_METHOD(some);
    PROTO_METHOD(every);
    PROTO_METHOD(find);
    PROTO_METHOD(includes);
    PROTO_METHOD(join);

#undef PROTO_METHOD

    ejsval dispose = _ejs_function_new_native (_ejs_null, _ejs_atom_dispose, _ejs_Iterator_prototype_dispose);
    _ejs_object_define_value_property (_ejs_Iterator_prototype, _ejs_Symbol_dispose, dispose,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

    // %IteratorHelperPrototype%
    _ejs_gc_add_root (&_ejs_IteratorHelper_prototype);
    _ejs_IteratorHelper_prototype = _ejs_object_new (_ejs_Iterator_prototype, &_ejs_Object_specops);

#define HELPER_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_IteratorHelper_prototype, x, _ejs_IteratorHelper_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    HELPER_METHOD(next);
    HELPER_METHOD(return);

#undef HELPER_METHOD

    _ejs_object_define_value_property (_ejs_IteratorHelper_prototype, _ejs_Symbol_toStringTag, _ejs_atom_IteratorHelper,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    // %WrapForValidIteratorPrototype%
    _ejs_gc_add_root (&_ejs_WrapForValidIterator_prototype);
    _ejs_WrapForValidIterator_prototype = _ejs_object_new (_ejs_Iterator_prototype, &_ejs_Object_specops);

#define WRAP_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_WrapForValidIterator_prototype, x, _ejs_WrapForValidIterator_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    WRAP_METHOD(next);
    WRAP_METHOD(return);

#undef WRAP_METHOD
}

// ------------------------------------------------------------------------
// specops
// ------------------------------------------------------------------------

static EJSObject*
_ejs_iterator_helper_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new (EJSIteratorHelper);
}

static void
_ejs_iterator_helper_specop_finalize (EJSObject* obj)
{
    EJSIteratorHelper* helper = (EJSIteratorHelper*)obj;
    free (helper->buffer);
    free (helper->concat_pairs);
    _ejs_Object_specops.Finalize (obj);
}

static void
_ejs_iterator_helper_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSIteratorHelper* helper = (EJSIteratorHelper*)obj;

    scan_func (&helper->iterator);
    scan_func (&helper->next_method);
    scan_func (&helper->fn);
    scan_func (&helper->inner);
    scan_func (&helper->inner_next);
    if (helper->buffer)
        for (uint32_t i = 0; i < helper->buffer_count; i++)
            scan_func (&helper->buffer[i]);
    if (helper->concat_pairs)
        for (uint32_t i = 0; i < 2 * helper->concat_count; i++)
            scan_func (&helper->concat_pairs[i]);

    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(IteratorHelper,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 OP_INHERIT, // [[Get]]
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 _ejs_iterator_helper_specop_allocate,
                 _ejs_iterator_helper_specop_finalize,
                 _ejs_iterator_helper_specop_scan
                 )

static EJSObject*
_ejs_wrap_for_valid_iterator_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new (EJSWrapForValidIterator);
}

static void
_ejs_wrap_for_valid_iterator_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSWrapForValidIterator* wrap = (EJSWrapForValidIterator*)obj;
    scan_func (&wrap->iterator);
    scan_func (&wrap->next_method);
    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(WrapForValidIterator,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 OP_INHERIT, // [[Get]]
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 _ejs_wrap_for_valid_iterator_specop_allocate,
                 OP_INHERIT, // finalize: no out-of-object state
                 _ejs_wrap_for_valid_iterator_specop_scan
                 )
