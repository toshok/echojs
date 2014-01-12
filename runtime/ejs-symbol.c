/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-symbol.h"
#include "ejs-gc.h"
#include "ejs-error.h"
#include "ejs-function.h"

void
_ejs_symbol_init(ejsval global)
{
}


static ejsval
_ejs_symbol_specop_get (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSPropertyDesc*
_ejs_symbol_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSPropertyDesc*
_ejs_symbol_specop_get_property (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static void
_ejs_symbol_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_symbol_specop_can_put (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_symbol_specop_has_property (ejsval obj, ejsval propertyName)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_symbol_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_symbol_specop_default_value (ejsval obj, const char *hint)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
_ejs_symbol_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    EJS_NOT_IMPLEMENTED();
}

static EJSObject*
_ejs_symbol_specop_allocate ()
{
    EJS_NOT_IMPLEMENTED();
}

static void
_ejs_symbol_specop_finalize (EJSObject* obj)
{
    EJS_NOT_IMPLEMENTED();
}

static void
_ejs_symbol_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJS_NOT_IMPLEMENTED();
}

EJS_DEFINE_CLASS(symbol, "Symbol",
                 _ejs_symbol_specop_get,
                 _ejs_symbol_specop_get_own_property,
                 _ejs_symbol_specop_get_property,
                 _ejs_symbol_specop_put,
                 _ejs_symbol_specop_can_put,
                 _ejs_symbol_specop_has_property,
                 _ejs_symbol_specop_delete,
                 _ejs_symbol_specop_default_value,
                 _ejs_symbol_specop_define_own_property,
                 OP_INHERIT, // has_instance
                 _ejs_symbol_specop_allocate,
                 _ejs_symbol_specop_finalize,
                 _ejs_symbol_specop_scan
                 )
