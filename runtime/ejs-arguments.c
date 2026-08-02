/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>
#include <string.h>

#include "ejs.h"
#include "ejs-ops.h"
#include "ejs-array.h"
#include "ejs-arguments.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-symbol.h"

ejsval _ejs_Arguments__proto__ EJSVAL_ALIGNMENT;

// singletons shared by every arguments object: the %ThrowTypeError%
// poison pill and %ArrayProto_values% (`arguments[Symbol.iterator]`
// must be the same function object as `Array.prototype.values`).
// Both are created lazily — _ejs_arguments_init runs before
// _ejs_array_init, so Array.prototype isn't populated yet at init time.
static ejsval _ejs_arguments_thrower EJSVAL_ALIGNMENT;
static ejsval _ejs_arguments_iterator_fn EJSVAL_ALIGNMENT;

static ejsval
ThrowTypeError(ejsval env, ejsval *_this, uint32_t argc, ejsval* args, ejsval newTarget)
{
    // XXX should really list the property
    _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "property not available in ejs");
}

static ejsval
arguments_thrower (void)
{
    if (EJSVAL_IS_UNDEFINED(_ejs_arguments_thrower))
        _ejs_arguments_thrower = _ejs_function_new_native (_ejs_null, _ejs_undefined, ThrowTypeError);
    return _ejs_arguments_thrower;
}

static ejsval
arguments_iterator_fn (void)
{
    if (EJSVAL_IS_UNDEFINED(_ejs_arguments_iterator_fn))
        _ejs_arguments_iterator_fn = _ejs_object_getprop (_ejs_Array_prototype, _ejs_atom_values);
    return _ejs_arguments_iterator_fn;
}

// the canonical array-index reading of a property key: a nonnegative
// integral number, or the canonical decimal string for one ("0", "17" —
// not " 0", "0.0", or "00", which name distinct ordinary properties).
// Returns -1 for every other key.
static int64_t
arguments_index (ejsval P)
{
    if (EJSVAL_IS_NUMBER(P)) {
        double n = EJSVAL_TO_NUMBER(P);
        if (n >= 0 && n <= INT32_MAX && floor(n) == n)
            return (int64_t)n;
        return -1;
    }
    if (EJSVAL_IS_STRING(P)) {
        uint32_t len = EJSVAL_TO_STRLEN(P);
        if (len < 1 || len > 9) // longer strings can't index a real argc
            return -1;
        jschar* chars = EJSVAL_TO_FLAT_STRING(P);
        if (len > 1 && chars[0] == '0')
            return -1;
        int64_t v = 0;
        for (uint32_t i = 0; i < len; i ++) {
            jschar c = chars[i];
            if (c < '0' || c > '9')
                return -1;
            v = v * 10 + (c - '0');
        }
        return v;
    }
    return -1;
}

ejsval
_ejs_arguments_new (int numElements, ejsval* args)
{
    size_t value_size = sizeof(EJSArguments) + numElements * sizeof(ejsval);
    EJSBool ool_buffer = EJS_FALSE;

    if (value_size > 2048) {
        value_size = sizeof(EJSArguments);
        ool_buffer = EJS_TRUE;
    }

    EJSArguments* arguments = _ejs_gc_new_obj(EJSArguments, value_size);
    _ejs_init_object ((EJSObject*)arguments, _ejs_Arguments__proto__, &_ejs_Arguments_specops);

    // fill the side buffer before anything below can allocate: the scan
    // hook walks argc/args, so they must be coherent once the object is
    // visible to a collection
    arguments->argc = numElements;
    if (ool_buffer) {
        arguments->args = (ejsval*)calloc(numElements, sizeof (ejsval));
        EJS_ARGUMENTS_SET_HAS_OOL_BUFFER(arguments);
    }
    else {
        arguments->args = (ejsval*)((char*)arguments + sizeof(EJSArguments));
    }
    memmove (arguments->args, args, sizeof(ejsval) * numElements);

    ejsval O = OBJECT_TO_EJSVAL((EJSObject*)arguments);

    // 10.4.4.6/7 CreateUnmappedArgumentsObject: the indices are REAL own
    // data properties {[[Writable]]: true, [[Enumerable]]: true,
    // [[Configurable]]: true} — the property map is authoritative for
    // defineProperty/delete/descriptor queries; the side buffer mirrors
    // it for fast indexed reads until a define/delete takes over.
    // Values are re-read from the (GC-scanned) buffer since ToString can
    // collect.  Insertion order = spec key order: indices, then
    // "length"/"callee", symbols last.
    for (int i = 0; i < numElements; i ++) {
        ejsval idx_name = ToString(NUMBER_TO_EJSVAL(i));
        _ejs_object_define_value_property (O, idx_name, arguments->args[i],
                                           EJS_PROP_WRITABLE | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE);
    }

    // 3. Perform DefinePropertyOrThrow(obj, "length", PropertyDescriptor {[[Value]]: len, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true})
    _ejs_object_define_value_property (O, _ejs_atom_length, NUMBER_TO_EJSVAL(numElements),
                                       EJS_PROP_WRITABLE | EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE);

    // Perform DefinePropertyOrThrow(obj, "callee", PropertyDescriptor {[[Get]]: %ThrowTypeError%, [[Set]]: %ThrowTypeError%, [[Enumerable]]: false, [[Configurable]]: false}).
    // this is the UNMAPPED behavior; mapped (sloppy) arguments want
    // callee = the enclosing function, but the compiler doesn't pass
    // the callee to us, so sloppy mode gets the poison too
    ejsval thrower = arguments_thrower();
    _ejs_object_define_accessor_property(O, _ejs_atom_callee, thrower, thrower,
                                         EJS_PROP_FLAGS_GETTER_SET | EJS_PROP_FLAGS_SETTER_SET | EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE);
    // no "caller" property: ES2017+ arguments objects don't have one

    // Perform DefinePropertyOrThrow(obj, @@iterator, PropertyDescriptor {[[Value]]:%ArrayProto_values%, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true})
    _ejs_object_define_value_property (O, _ejs_Symbol_iterator, arguments_iterator_fn(),
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

    // the defines above route through our own [[DefineOwnProperty]],
    // which conservatively taints the object; a freshly built one is
    // clean — buffer and map agree
    EJS_ARGUMENTS_CLEAR_OVERRIDDEN(arguments);

    return O;
}

// the compiler's arg_len op: the length the arguments object (or the
// rest array starting at `index`) would report for a call that arrived
// with `argc` arguments, without materializing either object.  Minted
// by the EIR args sinking (docs/sinking-plan.md) when the
// object's only uses are `.length` reads.
ejsval
_ejs_arg_length (uint32_t argc, uint32_t index)
{
    return NUMBER_TO_EJSVAL(argc > index ? (double)(argc - index) : 0);
}

void
_ejs_arguments_init(ejsval global)
{
    // 10.4.4.6/7: an arguments object's [[Prototype]] is
    // %Object.prototype% itself, not an intermediate object
    _ejs_gc_add_root (&_ejs_Arguments__proto__);
    _ejs_Arguments__proto__ = _ejs_Object_prototype;

    _ejs_arguments_thrower = _ejs_undefined;
    _ejs_gc_add_root (&_ejs_arguments_thrower);
    _ejs_arguments_iterator_fn = _ejs_undefined;
    _ejs_gc_add_root (&_ejs_arguments_iterator_fn);
}

static ejsval
_ejs_arguments_specop_get (ejsval obj, ejsval propertyName, ejsval receiver)
{
    EJSArguments* arguments = EJSVAL_TO_ARGUMENTS(obj);

    // while untainted, in-range indices are plain writable data
    // properties whose values mirror the side buffer — answer without
    // the map lookup
    if (!EJS_ARGUMENTS_IS_OVERRIDDEN(arguments)) {
        int64_t idx = arguments_index(propertyName);
        if (idx >= 0 && idx < (int64_t)arguments->argc)
            return arguments->args[idx];
    }

    return _ejs_Object_specops.Get (obj, propertyName, receiver);
}

static EJSBool
_ejs_arguments_specop_has_property (ejsval obj, ejsval propertyName)
{
    EJSArguments* arguments = EJSVAL_TO_ARGUMENTS(obj);

    if (!EJS_ARGUMENTS_IS_OVERRIDDEN(arguments)) {
        int64_t idx = arguments_index(propertyName);
        if (idx >= 0 && idx < (int64_t)arguments->argc)
            return EJS_TRUE;
        // out-of-range indices and every other key still need the
        // ordinary walk (own map + prototype chain)
    }

    return _ejs_Object_specops.HasProperty (obj, propertyName);
}

static EJSBool
_ejs_arguments_specop_set (ejsval obj, ejsval propertyName, ejsval val, ejsval receiver)
{
    EJSBool ok = _ejs_Object_specops.Set (obj, propertyName, val, receiver);

    // an ordinary set on an untainted object stored into the map's
    // (still plain, writable) index property; mirror it into the side
    // buffer so fast reads stay coherent.  A receiver other than obj
    // stored the property elsewhere, leaving our map untouched.
    EJSArguments* arguments = EJSVAL_TO_ARGUMENTS(obj);
    if (ok && !EJS_ARGUMENTS_IS_OVERRIDDEN(arguments) && EJSVAL_EQ(obj, receiver)) {
        int64_t idx = arguments_index(propertyName);
        if (idx >= 0 && idx < (int64_t)arguments->argc) {
            arguments->args[idx] = val;
            _ejs_gc_remember (arguments, val);
        }
    }
    return ok;
}

static EJSBool
_ejs_arguments_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool _throw)
{
    // a define can change an index's value or attributes out from under
    // the side buffer; from here on the map alone answers.
    // The ordinary define/delete paths reach the property map with the
    // key as passed (unlike Get/Set they don't ToPropertyKey), and the
    // map is string/symbol-keyed — normalize number keys here
    EJS_ARGUMENTS_SET_OVERRIDDEN(EJSVAL_TO_ARGUMENTS(obj));
    return _ejs_Object_specops.DefineOwnProperty (obj, ToPropertyKey(propertyName), propertyDescriptor, _throw);
}

static EJSBool
_ejs_arguments_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    EJS_ARGUMENTS_SET_OVERRIDDEN(EJSVAL_TO_ARGUMENTS(obj));
    return _ejs_Object_specops.Delete (obj, ToPropertyKey(propertyName), flag);
}

static EJSObject*
_ejs_arguments_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSArguments);
}

static void
_ejs_arguments_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSArguments* args = (EJSArguments*)obj;
    for (int i = 0; i < args->argc; i ++)
        scan_func (&(args->args[i]));
    _ejs_Object_specops.Scan (obj, scan_func);
}

static void
_ejs_arguments_specop_finalize (EJSObject* obj)
{
    EJSArguments* args = (EJSArguments*)obj;
    if (EJS_ARGUMENTS_HAS_OOL_BUFFER(args))
        free (args->args);
    _ejs_Object_specops.Finalize (obj);
}

EJS_DEFINE_CLASS(Arguments,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 _ejs_arguments_specop_define_own_property,
                 _ejs_arguments_specop_has_property,
                 _ejs_arguments_specop_get,
                 _ejs_arguments_specop_set,
                 _ejs_arguments_specop_delete,
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 _ejs_arguments_specop_allocate,
                 _ejs_arguments_specop_finalize,
                 _ejs_arguments_specop_scan
                 )
