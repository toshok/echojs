/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <assert.h>
#include <math.h>
#include <string.h>

#include "ejs.h"
#include "ejs-ops.h"
#include "ejs-arguments.h"
#include "ejs-string.h"

static ejsval  _ejs_arguments_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static EJSPropertyDesc* _ejs_arguments_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_arguments_specop_get_property (ejsval obj, ejsval propertyName);
static void    _ejs_arguments_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool _ejs_arguments_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool _ejs_arguments_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool _ejs_arguments_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval  _ejs_arguments_specop_default_value (ejsval obj, const char *hint);
static EJSBool _ejs_arguments_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSObject* _ejs_arguments_specop_allocate ();
static void    _ejs_arguments_specop_finalize (EJSObject* obj);
static void    _ejs_arguments_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

EJSSpecOps _ejs_arguments_specops = {
    "Arguments",
    _ejs_arguments_specop_get,
    _ejs_arguments_specop_get_own_property,
    _ejs_arguments_specop_get_property,
    _ejs_arguments_specop_put,
    _ejs_arguments_specop_can_put,
    _ejs_arguments_specop_has_property,
    _ejs_arguments_specop_delete,
    _ejs_arguments_specop_default_value,
    _ejs_arguments_specop_define_own_property,

    _ejs_arguments_specop_allocate,
    _ejs_arguments_specop_finalize,
    _ejs_arguments_specop_scan
};


#define EJSOBJ_IS_ARGUMENTS(obj) (((EJSObject*)obj)->proto == _ejs_Arguments__proto__)

ejsval _ejs_Arguments__proto__;

ejsval
_ejs_arguments_new (int numElements, ejsval* args)
{
    ejsval rv = _ejs_object_new (_ejs_Arguments__proto__, &_ejs_arguments_specops);

    EJSArguments* arguments = (EJSArguments*)EJSVAL_TO_OBJECT(rv);
    arguments->argc = numElements;
    arguments->args = (ejsval*)calloc(numElements, sizeof (ejsval));
    memmove (arguments->args, args, sizeof(ejsval) * numElements);
    return rv;
}

void
_ejs_arguments_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_Arguments__proto__);
    _ejs_Arguments__proto__ = _ejs_object_new(_ejs_Object_prototype, &_ejs_object_specops);

    END_SHADOW_STACK_FRAME;
}

static ejsval
_ejs_arguments_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr)
{
    EJSArguments* arguments = (EJSArguments*)EJSVAL_TO_OBJECT(obj);

    // check if propertyName is an integer, or a string that we can convert to an int
    EJSBool is_index = EJS_FALSE;
    int idx = 0;
    if (!isCStr && EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
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
    if ((isCStr && !strcmp("length", (char*)EJSVAL_TO_PRIVATE_PTR_IMPL(propertyName)))
        || (!isCStr && EJSVAL_IS_STRING(propertyName) && !ucs2_strcmp (_ejs_ucs2_length, EJSVAL_TO_FLAT_STRING(propertyName)))) {
        return NUMBER_TO_EJSVAL(arguments->argc);
    }

    // otherwise we fallback to the object implementation
    return _ejs_object_specops.get (obj, propertyName, isCStr);
}

static EJSPropertyDesc*
_ejs_arguments_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSPropertyDesc*
_ejs_arguments_specop_get_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_arguments_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_arguments_specop_can_put (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_arguments_specop_has_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_arguments_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    return _ejs_object_specops._delete (obj, propertyName, flag);
}

static ejsval
_ejs_arguments_specop_default_value (ejsval obj, const char *hint)
{
    return _ejs_object_specops.default_value (obj, hint);
}

static EJSBool
_ejs_arguments_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    return _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static EJSObject*
_ejs_arguments_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSArguments);
}


static void
_ejs_arguments_specop_finalize (EJSObject* obj)
{
    _ejs_object_specops.finalize (obj);
}

static void
_ejs_arguments_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    _ejs_object_specops.scan (obj, scan_func);
}
