/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-proxy.h"
#include "ejs-gc.h"
#include "ejs-error.h"
#include "ejs-function.h"

ejsval
_ejs_Proxy_create (ejsval env, ejsval this, uint32_t argc, ejsval* args)
{
    if (argc == 0)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "create requires more than 0 arguments");
    if (!EJSVAL_IS_OBJECT(args[0]))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argument 0 is not a non-null object");

    EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_Proxy_createFunction (ejsval env, ejsval this, uint32_t argc, ejsval* args)
{
    if (argc <= 1)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "create requires more than 1 arguments");
    if (!EJSVAL_IS_OBJECT(args[0]))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argument 0 is not a non-null object");
    if (!EJSVAL_IS_FUNCTION(args[1]))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argument 1 is not a function");
    if (argc > 2 && !EJSVAL_IS_FUNCTION(args[2]))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "argument 2 is not a function");

    EJS_NOT_IMPLEMENTED();
}

void
_ejs_proxy_init(ejsval global)
{
    ejsval _ejs_Proxy = _ejs_object_new (_ejs_null, &_ejs_object_specops);
    _ejs_object_setprop (global, _ejs_atom_Proxy, _ejs_Proxy);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Proxy, x, _ejs_Proxy_##x)

    OBJ_METHOD(create);
    OBJ_METHOD(createFunction);

#undef OBJ_METHOD
}


static ejsval
_ejs_proxy_specop_get (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSPropertyDesc*
_ejs_proxy_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSPropertyDesc*
_ejs_proxy_specop_get_property (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static void
_ejs_proxy_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_proxy_specop_can_put (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_proxy_specop_has_property (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_proxy_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_proxy_specop_default_value (ejsval obj, const char *hint)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_proxy_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSObject*
_ejs_proxy_specop_allocate ()
{
    EJS_NOT_IMPLEMENTED();
}

static void
_ejs_proxy_specop_finalize (EJSObject* obj)
{
    EJS_NOT_IMPLEMENTED();
}

static void
_ejs_proxy_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJS_NOT_IMPLEMENTED();
}

EJSSpecOps _ejs_proxy_specops = {
    "Proxy",
    _ejs_proxy_specop_get,
    _ejs_proxy_specop_get_own_property,
    _ejs_proxy_specop_get_property,
    _ejs_proxy_specop_put,
    _ejs_proxy_specop_can_put,
    _ejs_proxy_specop_has_property,
    _ejs_proxy_specop_delete,
    _ejs_proxy_specop_default_value,
    _ejs_proxy_specop_define_own_property,
    NULL, /* [[HasInstance]] */

    _ejs_proxy_specop_allocate,
    _ejs_proxy_specop_finalize,
    _ejs_proxy_specop_scan
};

