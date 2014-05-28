/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>
#include <string.h>

#include "ejs.h"
#include "ejs-ops.h"
#include "ejs-arguments.h"
#include "ejs-string.h"

ejsval _ejs_Arguments__proto__ EJSVAL_ALIGNMENT;

ejsval
_ejs_arguments_new (int numElements, ejsval* args)
{
    EJSArguments* arguments = _ejs_gc_new(EJSArguments);
    ejsval rv = OBJECT_TO_EJSVAL(arguments);

    _ejs_init_object ((EJSObject*)arguments, _ejs_Arguments__proto__, &_ejs_Arguments_specops);
    
    arguments->argc = numElements;
    arguments->args = (ejsval*)calloc(numElements, sizeof (ejsval));
    memmove (arguments->args, args, sizeof(ejsval) * numElements);
    return rv;
}

void
_ejs_arguments_init(ejsval global)
{
    _ejs_gc_add_root (&_ejs_Arguments__proto__);
    _ejs_Arguments__proto__ = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);
}

static ejsval
_ejs_arguments_specop_get (ejsval obj, ejsval propertyName, ejsval receiver)
{
    EJSArguments* arguments = EJSVAL_TO_ARGUMENTS(obj);

    // check if propertyName is an integer, or a string that we can convert to an int
    EJSBool is_index = EJS_FALSE;
    ejsval idx_val = ToNumber(propertyName);
    int idx;
    if (EJSVAL_IS_NUMBER(idx_val)) {
        double n = EJSVAL_TO_NUMBER(idx_val);
        if (floor(n) == n) {
            idx = (int)n;
            is_index = EJS_TRUE;
        }
    }

    if (is_index) {
        if (idx < 0 || idx > arguments->argc) {
            printf ("getprop(%d) on an arguments, returning undefined\n", idx);
            return _ejs_undefined;
        }
        return arguments->args[idx];
    }

    // we also handle the length getter here
    if (EJSVAL_IS_STRING(propertyName) && !ucs2_strcmp (_ejs_ucs2_length, EJSVAL_TO_FLAT_STRING(propertyName))) {
        return NUMBER_TO_EJSVAL(arguments->argc);
    }

    // otherwise we fallback to the object implementation
    return _ejs_Object_specops.Get (obj, propertyName, receiver);
}

static EJSBool
_ejs_arguments_specop_has_property (ejsval obj, ejsval propertyName)
{
    EJSArguments* arguments = (EJSArguments*)EJSVAL_TO_OBJECT(obj);
    // check if propertyName is an integer, or a string that we can convert to an int
    ejsval idx_val = ToNumber(propertyName);
    int idx;
    if (EJSVAL_IS_NUMBER(idx_val)) {
        double n = EJSVAL_TO_NUMBER(idx_val);
        if (floor(n) == n) {
            idx = (int)n;
            return idx >= 0 && idx < arguments->argc;
        }
    }

    // if we fail there, we fall back to the object impl below
    return _ejs_Object_specops.HasProperty (obj, propertyName);
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
        scan_func (args->args[i]);
    _ejs_Object_specops.Scan (obj, scan_func);
}

static void
_ejs_arguments_specop_finalize (EJSObject* obj)
{
    EJSArguments* args = (EJSArguments*)obj;
    free (args->args);
    _ejs_Object_specops.Finalize (obj);
}

EJS_DEFINE_CLASS(Arguments,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 _ejs_arguments_specop_has_property,
                 _ejs_arguments_specop_get,
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 _ejs_arguments_specop_allocate,
                 _ejs_arguments_specop_finalize,
                 _ejs_arguments_specop_scan
                 )
