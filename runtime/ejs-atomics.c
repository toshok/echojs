/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ejs-atomics.h"
#include "ejs-bigint.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-string.h"
#include "ejs-symbol.h"

ejsval _ejs_SharedArrayBuffer EJSVAL_ALIGNMENT;
ejsval _ejs_SharedArrayBuffer_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_Atomics EJSVAL_ALIGNMENT;

// ------------------------------------------------------------------------
// shared conversion helpers
// ------------------------------------------------------------------------

// 7.1.5 ToIntegerOrInfinity: NaN and -0 normalize to +0, infinities
// pass through
static double
ToIntegerOrInfinity (ejsval v)
{
    double d = EJSVAL_TO_NUMBER(ToNumber(v));
    if (isnan(d)) return 0;
    d = trunc(d);
    if (d == 0) return 0; // -0 -> +0
    return d;
}

// 7.1.22 ToIndex: RangeError outside [0, 2^53-1]
static double
ToIndexDouble (ejsval v, const char* msg)
{
    if (EJSVAL_IS_UNDEFINED(v)) return 0;
    double d = ToIntegerOrInfinity(v);
    if (!(d >= 0) || d > 9007199254740991.0)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, msg);
    return d;
}

// 7.1.13 ToBigInt: Numbers are a TypeError, not a coercion
static ejsval
AtomicsToBigInt (ejsval v)
{
    if (EJSVAL_IS_OBJECT(v))
        v = ToPrimitive(v, TO_PRIM_HINT_NUMBER);

    if (EJSVAL_IS_BIGINT(v))
        return v;
    if (EJSVAL_IS_BOOLEAN(v))
        return _ejs_bigint_new_from_int64 (EJSVAL_TO_BOOLEAN(v) ? 1 : 0);
    if (EJSVAL_IS_STRING(v)) {
        ejsval rv = _ejs_bigint_from_string (v);
        if (EJSVAL_IS_UNDEFINED(rv))
            _ejs_throw_nativeerror_utf8 (EJS_SYNTAX_ERROR, "Cannot convert string to a BigInt");
        return rv;
    }
    _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Cannot convert value to a BigInt");
    return _ejs_undefined; // not reached
}

// the BigInt's value modulo 2^64 as raw bits
static uint64_t
BigIntValueToUint64Bits (ejsval bigint)
{
    EJSBigInt* bi = EJSVAL_TO_BIGINT(bigint);
    uint64_t bits = bi->length ? bi->digits[0] : 0;
    return bi->sign ? (uint64_t)0 - bits : bits;
}

// an already-truncated integer double modulo 2^32, as raw bits.  The
// number-element arrays are at most 4 bytes wide, so 32 bits of
// modulus suffice; narrower elements truncate further on store.
static uint32_t
IntegerDoubleToUint32Bits (double d)
{
    if (!isfinite(d)) return 0;
    double m = fmod(d, 4294967296.0);
    if (m < 0) m += 4294967296.0;
    return (uint32_t)m;
}

// ------------------------------------------------------------------------
// SharedArrayBuffer
// ------------------------------------------------------------------------

static EJSSharedArrayBuffer*
_ejs_sharedarraybuffer_check (ejsval v, const char* msg)
{
    if (!EJSVAL_IS_SHAREDARRAYBUFFER(v))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, msg);
    return (EJSSharedArrayBuffer*)EJSVAL_TO_OBJECT(v);
}

// allocates the full max_byte_length up front, so grow() never has to
// move the data block out from under live views
static ejsval
_ejs_sharedarraybuffer_new_with_proto (ejsval proto, uint32_t byte_length, uint32_t max_byte_length, EJSBool growable)
{
    void* block = NULL;
    uint32_t alloc_size = growable ? max_byte_length : byte_length;
    if (alloc_size) {
        block = calloc (1, alloc_size);
        if (!block)
            _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "SharedArrayBuffer allocation failed");
    }

    EJSSharedArrayBuffer* sab = (EJSSharedArrayBuffer*)_ejs_gc_alloc (sizeof(EJSSharedArrayBuffer), EJS_SCAN_TYPE_OBJECT);
    _ejs_init_object ((EJSObject*)sab, proto, &_ejs_ArrayBuffer_specops);

    sab->buf.dependent = EJS_FALSE;
    sab->buf.detached = EJS_FALSE;
    sab->buf.size = byte_length;
    sab->buf.data.alloced_buf = block;
    sab->buf.data.dependent.offset = (int)EJS_SHAREDARRAYBUFFER_TAG;
    sab->max_byte_length = max_byte_length;
    sab->growable = growable;

    return OBJECT_TO_EJSVAL((EJSObject*)sab);
}

// ES2026 25.2.3.1 SharedArrayBuffer ( length [ , options ] )
static EJS_NATIVE_FUNC(_ejs_SharedArrayBuffer_impl) {
    if (EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Constructor SharedArrayBuffer requires 'new'");

    ejsval length = _ejs_undefined;
    ejsval options = _ejs_undefined;
    if (argc > 0) length = args[0];
    if (argc > 1) options = args[1];

    double byte_length = ToIndexDouble (length, "Invalid SharedArrayBuffer length");

    // GetArrayBufferMaxByteLengthOption
    EJSBool growable = EJS_FALSE;
    double max_byte_length = 0;
    if (EJSVAL_IS_OBJECT(options)) {
        ejsval mbl = Get (options, _ejs_atom_maxByteLength);
        if (!EJSVAL_IS_UNDEFINED(mbl)) {
            max_byte_length = ToIndexDouble (mbl, "Invalid SharedArrayBuffer maxByteLength");
            growable = EJS_TRUE;
        }
    }

    // AllocateSharedArrayBuffer: the growable range check precedes the
    // prototype fetch, the data block allocation follows it
    if (growable && byte_length > max_byte_length)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "SharedArrayBuffer length exceeds maxByteLength");

    ejsval proto = GetPrototypeFromConstructor (newTarget, _ejs_SharedArrayBuffer_prototype);

    // the data block: size is an int, so anything past INT32_MAX is an
    // impossible allocation
    if (byte_length > 2147483647.0 || max_byte_length > 2147483647.0)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "SharedArrayBuffer allocation failed");

    *_this = _ejs_sharedarraybuffer_new_with_proto (proto, (uint32_t)byte_length,
                                                    growable ? (uint32_t)max_byte_length : (uint32_t)byte_length,
                                                    growable);
    return *_this;
}

// 25.2.5.2 get SharedArrayBuffer.prototype.byteLength
static EJS_NATIVE_FUNC(_ejs_SharedArrayBuffer_prototype_get_byteLength) {
    EJSSharedArrayBuffer* sab = _ejs_sharedarraybuffer_check (*_this, "get SharedArrayBuffer.prototype.byteLength called with incompatible this");
    return NUMBER_TO_EJSVAL(sab->buf.size);
}

// 25.2.5.4 get SharedArrayBuffer.prototype.growable
static EJS_NATIVE_FUNC(_ejs_SharedArrayBuffer_prototype_get_growable) {
    EJSSharedArrayBuffer* sab = _ejs_sharedarraybuffer_check (*_this, "get SharedArrayBuffer.prototype.growable called with incompatible this");
    return BOOLEAN_TO_EJSVAL(sab->growable);
}

// 25.2.5.5 get SharedArrayBuffer.prototype.maxByteLength
static EJS_NATIVE_FUNC(_ejs_SharedArrayBuffer_prototype_get_maxByteLength) {
    EJSSharedArrayBuffer* sab = _ejs_sharedarraybuffer_check (*_this, "get SharedArrayBuffer.prototype.maxByteLength called with incompatible this");
    return NUMBER_TO_EJSVAL(sab->growable ? (double)sab->max_byte_length : (double)sab->buf.size);
}

// 25.2.5.3 SharedArrayBuffer.prototype.grow ( newLength )
static EJS_NATIVE_FUNC(_ejs_SharedArrayBuffer_prototype_grow) {
    ejsval newLength = _ejs_undefined;
    if (argc > 0) newLength = args[0];

    EJSSharedArrayBuffer* sab = _ejs_sharedarraybuffer_check (*_this, "SharedArrayBuffer.prototype.grow called with incompatible this");
    if (!sab->growable)
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "SharedArrayBuffer.prototype.grow called on a non-growable SharedArrayBuffer");

    double new_byte_length = ToIndexDouble (newLength, "Invalid SharedArrayBuffer length");
    if (new_byte_length < sab->buf.size || new_byte_length > sab->max_byte_length)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "SharedArrayBuffer.prototype.grow: invalid length");

    // the block is allocated (and zeroed) to max_byte_length at
    // construction, so growing is just the length update
    sab->buf.size = (int)new_byte_length;
    return _ejs_undefined;
}

// 25.2.5.6 SharedArrayBuffer.prototype.slice ( start, end )
static int
_ejs_sab_clamp_index (double rel, int len)
{
    if (rel < 0) {
        double r = len + rel;
        return r < 0 ? 0 : (int)r;
    }
    return rel > len ? len : (int)rel;
}

static EJS_NATIVE_FUNC(_ejs_SharedArrayBuffer_prototype_slice) {
    EJSSharedArrayBuffer* sab = _ejs_sharedarraybuffer_check (*_this, "SharedArrayBuffer.prototype.slice called with incompatible this");
    int len = sab->buf.size;

    double rel_start = (argc > 0) ? ToIntegerOrInfinity (args[0]) : 0;
    int first = _ejs_sab_clamp_index (rel_start, len);

    double rel_end = (argc > 1 && !EJSVAL_IS_UNDEFINED(args[1])) ? ToIntegerOrInfinity (args[1]) : len;
    int final = _ejs_sab_clamp_index (rel_end, len);

    int new_len = final - first;
    if (new_len < 0) new_len = 0;

    ejsval rv = _ejs_sharedarraybuffer_new_with_proto (_ejs_SharedArrayBuffer_prototype, new_len, new_len, EJS_FALSE);
    if (new_len) {
        // re-read this's block pointer after the allocation (GC may
        // have moved nothing here -- the block is malloc'd -- but the
        // sab cell itself can move, so refetch through the ejsval)
        EJSSharedArrayBuffer* src = (EJSSharedArrayBuffer*)EJSVAL_TO_OBJECT(*_this);
        EJSSharedArrayBuffer* dst = (EJSSharedArrayBuffer*)EJSVAL_TO_OBJECT(rv);
        memcpy (dst->buf.data.alloced_buf, (char*)src->buf.data.alloced_buf + first, new_len);
    }
    return rv;
}

// ------------------------------------------------------------------------
// Atomics
// ------------------------------------------------------------------------

typedef enum {
    ATOMICS_OP_ADD,
    ATOMICS_OP_AND,
    ATOMICS_OP_CAS,
    ATOMICS_OP_EXCHANGE,
    ATOMICS_OP_LOAD,
    ATOMICS_OP_OR,
    ATOMICS_OP_STORE,
    ATOMICS_OP_SUB,
    ATOMICS_OP_XOR
} EJSAtomicsOp;

static EJSBool
_ejs_atomics_buffer_is_detached (ejsval bufferval)
{
    EJSArrayBuffer* buf = EJSVAL_TO_ARRAYBUFFER(bufferval);
    if (buf->detached) return EJS_TRUE;
    if (buf->dependent) return _ejs_atomics_buffer_is_detached (buf->data.dependent.buf);
    return EJS_FALSE;
}

// 25.4.3.1 ValidateIntegerTypedArray: an integer-element (or, when
// waitable, Int32/BigInt64-element) typed array over a live buffer
static EJSTypedArray*
ValidateIntegerTypedArray (ejsval v, EJSBool waitable)
{
    if (!EJSVAL_IS_TYPEDARRAY(v))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Atomics operation on a value that is not an integer typed array");

    EJSTypedArray* ta = EJSVAL_TO_TYPEDARRAY(v);
    switch (ta->element_type) {
    case EJS_TYPEDARRAY_INT32:
    case EJS_TYPEDARRAY_BIGINT64:
        break;
    case EJS_TYPEDARRAY_INT8:
    case EJS_TYPEDARRAY_UINT8:
    case EJS_TYPEDARRAY_INT16:
    case EJS_TYPEDARRAY_UINT16:
    case EJS_TYPEDARRAY_UINT32:
    case EJS_TYPEDARRAY_BIGUINT64:
        if (waitable)
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Atomics wait/notify requires an Int32Array or BigInt64Array");
        break;
    default:
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Atomics operation on a typed array with a non-integer element type");
    }

    if (_ejs_atomics_buffer_is_detached (ta->buffer))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Atomics operation on a detached buffer");

    return ta;
}

// 25.4.3.2 ValidateAtomicAccess
static uint32_t
ValidateAtomicAccess (EJSTypedArray* ta, ejsval index)
{
    double d = ToIndexDouble (index, "Atomics access index out of range");
    if (d >= ta->length)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "Atomics access index out of range");
    return (uint32_t)d;
}

static void*
_ejs_atomics_elem_ptr (EJSTypedArray* ta, uint32_t idx)
{
    char* base = (char*)_ejs_typedarray_get_data ((EJSObject*)ta);
    return base + (size_t)idx * _ejs_typed_array_elsizes[ta->element_type];
}

// the shared read-modify-write core.  Single-threaded runtime: plain
// memory operations are sequentially consistent, no fences needed.
static ejsval
_ejs_atomics_rmw (uint32_t argc, ejsval* args, EJSAtomicsOp op)
{
    ejsval tav   = argc > 0 ? args[0] : _ejs_undefined;
    ejsval index = argc > 1 ? args[1] : _ejs_undefined;

    EJSTypedArray* ta = ValidateIntegerTypedArray (tav, EJS_FALSE);
    uint32_t idx = ValidateAtomicAccess (ta, index);
    EJSTypedArrayType elem_type = ta->element_type;
    EJSBool is_bigint = (elem_type == EJS_TYPEDARRAY_BIGINT64 || elem_type == EJS_TYPEDARRAY_BIGUINT64);

    // operand conversion runs arbitrary JS (valueOf), so it must
    // complete before the element pointer is taken
    uint64_t opbits = 0, expbits = 0, repbits = 0;
    ejsval store_rv = _ejs_undefined;
    if (is_bigint) {
        if (op == ATOMICS_OP_CAS) {
            expbits = BigIntValueToUint64Bits (AtomicsToBigInt (argc > 2 ? args[2] : _ejs_undefined));
            repbits = BigIntValueToUint64Bits (AtomicsToBigInt (argc > 3 ? args[3] : _ejs_undefined));
        }
        else if (op != ATOMICS_OP_LOAD) {
            ejsval bi = AtomicsToBigInt (argc > 2 ? args[2] : _ejs_undefined);
            opbits = BigIntValueToUint64Bits (bi);
            store_rv = bi; // Atomics.store returns the unwrapped BigInt
        }
    }
    else {
        if (op == ATOMICS_OP_CAS) {
            expbits = IntegerDoubleToUint32Bits (ToIntegerOrInfinity (argc > 2 ? args[2] : _ejs_undefined));
            repbits = IntegerDoubleToUint32Bits (ToIntegerOrInfinity (argc > 3 ? args[3] : _ejs_undefined));
        }
        else if (op != ATOMICS_OP_LOAD) {
            double d = ToIntegerOrInfinity (argc > 2 ? args[2] : _ejs_undefined);
            opbits = IntegerDoubleToUint32Bits (d);
            store_rv = NUMBER_TO_EJSVAL(d); // Atomics.store returns the unwrapped integer
        }
    }

    void* p = _ejs_atomics_elem_ptr (ta, idx);

    ejsval oldval = _ejs_undefined;

#define ATOMICS_RMW_CASE(TYPE, ctype, uctype, MAKE_OLD)                 \
    case EJS_TYPEDARRAY_##TYPE: {                                       \
        ctype old = *(ctype*)p;                                         \
        uctype uold = (uctype)old;                                      \
        uctype unew = uold;                                             \
        switch (op) {                                                   \
        case ATOMICS_OP_ADD:      unew = (uctype)(uold + (uctype)opbits); break; \
        case ATOMICS_OP_SUB:      unew = (uctype)(uold - (uctype)opbits); break; \
        case ATOMICS_OP_AND:      unew = (uctype)(uold & (uctype)opbits); break; \
        case ATOMICS_OP_OR:       unew = (uctype)(uold | (uctype)opbits); break; \
        case ATOMICS_OP_XOR:      unew = (uctype)(uold ^ (uctype)opbits); break; \
        case ATOMICS_OP_EXCHANGE:                                       \
        case ATOMICS_OP_STORE:    unew = (uctype)opbits; break;         \
        case ATOMICS_OP_CAS:      if (uold == (uctype)expbits) unew = (uctype)repbits; break; \
        case ATOMICS_OP_LOAD:     break;                                \
        }                                                               \
        if (op != ATOMICS_OP_LOAD)                                      \
            *(ctype*)p = (ctype)unew;                                   \
        oldval = MAKE_OLD(old);                                         \
    } break;

#define ATOMICS_OLD_NUM(x)     NUMBER_TO_EJSVAL((double)(x))
#define ATOMICS_OLD_BIGINT(x)  _ejs_bigint_new_from_int64((int64_t)(x))
#define ATOMICS_OLD_BIGUINT(x) _ejs_bigint_new_from_uint64((uint64_t)(x))

    switch (elem_type) {
    ATOMICS_RMW_CASE(INT8,      int8_t,   uint8_t,  ATOMICS_OLD_NUM)
    ATOMICS_RMW_CASE(UINT8,     uint8_t,  uint8_t,  ATOMICS_OLD_NUM)
    ATOMICS_RMW_CASE(INT16,     int16_t,  uint16_t, ATOMICS_OLD_NUM)
    ATOMICS_RMW_CASE(UINT16,    uint16_t, uint16_t, ATOMICS_OLD_NUM)
    ATOMICS_RMW_CASE(INT32,     int32_t,  uint32_t, ATOMICS_OLD_NUM)
    ATOMICS_RMW_CASE(UINT32,    uint32_t, uint32_t, ATOMICS_OLD_NUM)
    ATOMICS_RMW_CASE(BIGINT64,  int64_t,  uint64_t, ATOMICS_OLD_BIGINT)
    ATOMICS_RMW_CASE(BIGUINT64, uint64_t, uint64_t, ATOMICS_OLD_BIGUINT)
    default: EJS_ASSERT(EJS_FALSE); break;
    }

#undef ATOMICS_RMW_CASE
#undef ATOMICS_OLD_NUM
#undef ATOMICS_OLD_BIGINT
#undef ATOMICS_OLD_BIGUINT

    if (op == ATOMICS_OP_STORE)
        return store_rv;
    return oldval;
}

static EJS_NATIVE_FUNC(_ejs_Atomics_add)             { return _ejs_atomics_rmw (argc, args, ATOMICS_OP_ADD); }
static EJS_NATIVE_FUNC(_ejs_Atomics_and)             { return _ejs_atomics_rmw (argc, args, ATOMICS_OP_AND); }
static EJS_NATIVE_FUNC(_ejs_Atomics_compareExchange) { return _ejs_atomics_rmw (argc, args, ATOMICS_OP_CAS); }
static EJS_NATIVE_FUNC(_ejs_Atomics_exchange)        { return _ejs_atomics_rmw (argc, args, ATOMICS_OP_EXCHANGE); }
static EJS_NATIVE_FUNC(_ejs_Atomics_load)            { return _ejs_atomics_rmw (argc, args, ATOMICS_OP_LOAD); }
static EJS_NATIVE_FUNC(_ejs_Atomics_or)              { return _ejs_atomics_rmw (argc, args, ATOMICS_OP_OR); }
static EJS_NATIVE_FUNC(_ejs_Atomics_store)           { return _ejs_atomics_rmw (argc, args, ATOMICS_OP_STORE); }
static EJS_NATIVE_FUNC(_ejs_Atomics_sub)             { return _ejs_atomics_rmw (argc, args, ATOMICS_OP_SUB); }
static EJS_NATIVE_FUNC(_ejs_Atomics_xor)             { return _ejs_atomics_rmw (argc, args, ATOMICS_OP_XOR); }

// 25.4.9 Atomics.isLockFree ( size )
static EJS_NATIVE_FUNC(_ejs_Atomics_isLockFree) {
    double n = ToIntegerOrInfinity (argc > 0 ? args[0] : _ejs_undefined);
    return BOOLEAN_TO_EJSVAL(n == 1 || n == 2 || n == 4 || n == 8);
}

// 25.4.3.14 DoWait: the shared validation/conversion prefix of
// wait/waitAsync.  Runs through the timeout conversion; the caller
// decides what waiting means.
static EJSTypedArray*
_ejs_atomics_do_wait_prefix (uint32_t argc, ejsval* args, uint32_t* idx_out, uint64_t* value_bits_out)
{
    ejsval tav     = argc > 0 ? args[0] : _ejs_undefined;
    ejsval index   = argc > 1 ? args[1] : _ejs_undefined;
    ejsval value   = argc > 2 ? args[2] : _ejs_undefined;
    ejsval timeout = argc > 3 ? args[3] : _ejs_undefined;

    EJSTypedArray* ta = ValidateIntegerTypedArray (tav, EJS_TRUE);
    if (!EJSVAL_IS_SHAREDARRAYBUFFER(ta->buffer))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Atomics wait requires a typed array over a SharedArrayBuffer");

    *idx_out = ValidateAtomicAccess (ta, index);

    if (ta->element_type == EJS_TYPEDARRAY_BIGINT64)
        *value_bits_out = BigIntValueToUint64Bits (AtomicsToBigInt (value));
    else
        *value_bits_out = IntegerDoubleToUint32Bits (ToIntegerOrInfinity (value));

    // ToNumber(timeout) runs for its side effects; the timeout itself
    // never matters here (nothing can notify a single-agent runtime)
    ToNumber (timeout);

    return ta;
}

// 25.4.13 Atomics.wait: this agent cannot suspend
static EJS_NATIVE_FUNC(_ejs_Atomics_wait) {
    uint32_t idx;
    uint64_t value_bits;
    _ejs_atomics_do_wait_prefix (argc, args, &idx, &value_bits);
    _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Atomics.wait cannot block: this agent cannot suspend");
    return _ejs_undefined; // not reached
}

// 25.4.14 Atomics.waitAsync: resolves synchronously -- "not-equal" on
// a failed comparison, otherwise "timed-out" (no other agent exists to
// notify, so every wait times out)
static EJS_NATIVE_FUNC(_ejs_Atomics_waitAsync) {
    uint32_t idx;
    uint64_t value_bits;
    EJSTypedArray* ta = _ejs_atomics_do_wait_prefix (argc, args, &idx, &value_bits);

    uint64_t current;
    void* p = _ejs_atomics_elem_ptr (ta, idx);
    if (ta->element_type == EJS_TYPEDARRAY_BIGINT64)
        current = (uint64_t)*(int64_t*)p;
    else
        current = (uint32_t)*(int32_t*)p;

    ejsval result = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_setprop (result, _ejs_atom_async, _ejs_false);
    _ejs_object_setprop (result, _ejs_atom_value, current != value_bits ? _ejs_atom_not_equal : _ejs_atom_timed_out);
    return result;
}

// 25.4.15 Atomics.notify: validation and count conversion run for
// their effects; no agent can be waiting, so zero waiters wake
static EJS_NATIVE_FUNC(_ejs_Atomics_notify) {
    ejsval tav   = argc > 0 ? args[0] : _ejs_undefined;
    ejsval index = argc > 1 ? args[1] : _ejs_undefined;
    ejsval count = argc > 2 ? args[2] : _ejs_undefined;

    EJSTypedArray* ta = ValidateIntegerTypedArray (tav, EJS_TRUE);
    ValidateAtomicAccess (ta, index);

    if (!EJSVAL_IS_UNDEFINED(count))
        ToIntegerOrInfinity (count);

    return NUMBER_TO_EJSVAL(0);
}

// Atomics.pause ( [ N ] ): a scheduling hint; validation is its only
// observable behavior
static EJS_NATIVE_FUNC(_ejs_Atomics_pause) {
    if (argc > 0 && !EJSVAL_IS_UNDEFINED(args[0])) {
        ejsval N = args[0];
        if (!EJSVAL_IS_NUMBER(N))
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Atomics.pause argument must be an integer");
        double d = EJSVAL_TO_NUMBER(N);
        if (isnan(d) || !isfinite(d) || trunc(d) != d)
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Atomics.pause argument must be an integer");
    }
    return _ejs_undefined;
}

// ------------------------------------------------------------------------
// init
// ------------------------------------------------------------------------

void
_ejs_atomics_init (ejsval global)
{
    // SharedArrayBuffer
    _ejs_gc_add_root (&_ejs_SharedArrayBuffer);
    _ejs_SharedArrayBuffer = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_SharedArrayBuffer, _ejs_SharedArrayBuffer_impl);
    _ejs_object_define_value_property (global, _ejs_atom_SharedArrayBuffer, _ejs_SharedArrayBuffer,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

    _ejs_gc_add_root (&_ejs_SharedArrayBuffer_prototype);
    _ejs_SharedArrayBuffer_prototype = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (_ejs_SharedArrayBuffer, _ejs_atom_prototype, _ejs_SharedArrayBuffer_prototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_SharedArrayBuffer_prototype, _ejs_atom_constructor, _ejs_SharedArrayBuffer,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define PROTO_GETTER(x) EJS_MACRO_START                                 \
    ejsval getter = _ejs_function_new_native (_ejs_null, _ejs_atom_##x, _ejs_SharedArrayBuffer_prototype_get_##x); \
    _ejs_object_define_accessor_property (_ejs_SharedArrayBuffer_prototype, _ejs_atom_##x, getter, _ejs_undefined, \
                                          EJS_PROP_FLAGS_GETTER_SET | EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE); \
    EJS_MACRO_END

    PROTO_GETTER(byteLength);
    PROTO_GETTER(growable);
    PROTO_GETTER(maxByteLength);

#undef PROTO_GETTER

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_SharedArrayBuffer_prototype, x, _ejs_SharedArrayBuffer_prototype_##x, \
                                                        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    PROTO_METHOD(grow);
    PROTO_METHOD(slice);

#undef PROTO_METHOD

    _ejs_object_define_value_property (_ejs_SharedArrayBuffer_prototype, _ejs_Symbol_toStringTag, _ejs_atom_SharedArrayBuffer,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    // Atomics: a plain namespace object, like Math
    _ejs_gc_add_root (&_ejs_Atomics);
    _ejs_Atomics = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (global, _ejs_atom_Atomics, _ejs_Atomics,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_Atomics, x, _ejs_Atomics_##x, \
                                                      EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    OBJ_METHOD(add);
    OBJ_METHOD(and);
    OBJ_METHOD(compareExchange);
    OBJ_METHOD(exchange);
    OBJ_METHOD(isLockFree);
    OBJ_METHOD(load);
    OBJ_METHOD(notify);
    OBJ_METHOD(or);
    OBJ_METHOD(pause);
    OBJ_METHOD(store);
    OBJ_METHOD(sub);
    OBJ_METHOD(wait);
    OBJ_METHOD(waitAsync);
    OBJ_METHOD(xor);

#undef OBJ_METHOD

    _ejs_object_define_value_property (_ejs_Atomics, _ejs_Symbol_toStringTag, _ejs_atom_Atomics,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
}
