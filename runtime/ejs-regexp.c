/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <assert.h>

#include "ejs-value.h"
#include "ejs-regexp.h"
#include "ejs-function.h"

static ejsval _ejs_regexp_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static EJSPropertyDesc* _ejs_regexp_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_regexp_specop_get_property (ejsval obj, ejsval propertyName);
static void      _ejs_regexp_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool   _ejs_regexp_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_regexp_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_regexp_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval _ejs_regexp_specop_default_value (ejsval obj, const char *hint);
static EJSBool   _ejs_regexp_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static void      _ejs_regexp_specop_finalize (EJSObject* obj);
static void      _ejs_regexp_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

EJSSpecOps _ejs_regexp_specops = {
    "RegExp",
    _ejs_regexp_specop_get,
    _ejs_regexp_specop_get_own_property,
    _ejs_regexp_specop_get_property,
    _ejs_regexp_specop_put,
    _ejs_regexp_specop_can_put,
    _ejs_regexp_specop_has_property,
    _ejs_regexp_specop_delete,
    _ejs_regexp_specop_default_value,
    _ejs_regexp_specop_define_own_property,
    _ejs_regexp_specop_finalize,
    _ejs_regexp_specop_scan
};

EJSObject* _ejs_regexp_alloc_instance()
{
    return (EJSObject*)_ejs_gc_new(EJSRegExp);
}

ejsval
_ejs_regexp_new_utf8 (const char* str)
{
    int str_len = strlen(str);
    size_t value_size = sizeof (EJSRegExp) + str_len;

    EJSRegExp* rv = (EJSRegExp*)_ejs_gc_alloc (value_size, EJS_SCAN_TYPE_OBJECT);

    _ejs_init_object ((EJSObject*)rv, _ejs_RegExp_proto, &_ejs_regexp_specops);
    ((EJSObject*)rv)->ops = &_ejs_regexp_specops;

    rv->pattern_len = str_len;
    rv->pattern = strdup(str);

    return OBJECT_TO_EJSVAL((EJSObject*)rv);
}

ejsval _ejs_RegExp;
ejsval _ejs_RegExp_proto;

static ejsval
_ejs_RegExp_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        // called as a function
        printf ("called RegExp() as a function!\n");
        return _ejs_object_new(_ejs_RegExp_proto);
    }
    else {
        // called as a constructor
        printf ("called RegExp() as a constructor!\n");
        return _this;
    }
}

static ejsval
_ejs_RegExp_prototype_exec (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_RegExp_prototype_match (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_RegExp_prototype_test (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

void
_ejs_regexp_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_RegExp_proto);
    _ejs_RegExp_proto = _ejs_object_new(_ejs_null);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_RegExp, (EJSClosureFunc)_ejs_RegExp_impl));
    _ejs_RegExp = tmpobj;

    _ejs_object_setprop (_ejs_RegExp,       _ejs_atom_prototype,  _ejs_RegExp_proto);

#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_RegExp, EJS_STRINGIFY(x), _ejs_RegExp_##x)
#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_RegExp_proto, EJS_STRINGIFY(x), _ejs_RegExp_prototype_##x)

    PROTO_METHOD(exec);
    PROTO_METHOD(match);
    PROTO_METHOD(test);

#undef OBJ_METHOD
#undef PROTO_METHOD

    _ejs_object_setprop (global, _ejs_atom_RegExp, _ejs_RegExp);

    END_SHADOW_STACK_FRAME;
}


static ejsval
_ejs_regexp_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr)
{
    return _ejs_object_specops.get (obj, propertyName, isCStr);
}

static EJSPropertyDesc*
_ejs_regexp_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSPropertyDesc*
_ejs_regexp_specop_get_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_regexp_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_regexp_specop_can_put (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_regexp_specop_has_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_regexp_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    return _ejs_object_specops._delete (obj, propertyName, flag);
}

static ejsval
_ejs_regexp_specop_default_value (ejsval obj, const char *hint)
{
    return _ejs_object_specops.default_value (obj, hint);
}

static EJSBool
_ejs_regexp_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    return _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static void

_ejs_regexp_specop_finalize (EJSObject* obj)
{
    _ejs_object_specops.finalize (obj);
}

static void
_ejs_regexp_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    _ejs_object_specops.scan (obj, scan_func);
}
