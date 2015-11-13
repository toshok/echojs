/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <string.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-module.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-error.h"
#include "ejs-symbol.h"
#include "ejs-proxy.h"

ejsval*
_ejs_module_get_slot_ref (EJSModule* module, int slot)
{
    EJS_ASSERT(slot < module->num_exports);
    return &module->exports[slot];
}

void
_ejs_module_add_export_accessors (EJSModule* module, const char *ident, EJSClosureFunc getter, EJSClosureFunc setter)
{
    ejsval M = OBJECT_TO_EJSVAL(module);

    ejsval P = _ejs_string_new_utf8(ident);

    ejsval get = _ejs_function_new_anon (_ejs_undefined, getter);
    ejsval set = _ejs_function_new_anon (_ejs_undefined, getter);
    uint32_t flags = EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_ENUMERABLE; // XXX spec check this

    _ejs_object_define_accessor_property (M, P, get, set, flags);
}

static void
_ejs_module_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSModule *module = (EJSModule*)obj;

    for (int i = 0; i < module->num_exports; i ++)
        scan_func(module->exports[i]);

    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(Module,
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
                 OP_INHERIT, // allocate.  shouldn't ever be used
                 OP_INHERIT, // finalize.  also shouldn't ever be used
                 _ejs_module_specop_scan
                 )
