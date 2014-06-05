/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-promise.h"
#include "ejs-error.h"
#include "ejs-string.h"
#include "ejs-proxy.h"
#include "ejs-symbol.h"

#include <string.h>

ejsval _ejs_Promise EJSVAL_ALIGNMENT;
ejsval _ejs_Promise_prototype EJSVAL_ALIGNMENT;

static ejsval
_ejs_Promise_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_Promise_prototype_catch (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_Promise_prototype_then (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_Promise_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_Promise_all (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_Promise_race (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_Promise_reject (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_Promise_resolve (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

void
_ejs_promise_init(ejsval global)
{
    _ejs_Promise = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_Promise, (EJSClosureFunc)_ejs_Promise_impl);
    _ejs_object_setprop (global, _ejs_atom_Promise, _ejs_Promise);

    _ejs_gc_add_root (&_ejs_Promise_prototype);
    _ejs_Promise_prototype = _ejs_object_new(_ejs_null, &_ejs_Object_specops);
    _ejs_object_setprop (_ejs_Promise,       _ejs_atom_prototype,  _ejs_Promise_prototype);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Promise_prototype, x, _ejs_Promise_prototype_##x, EJS_PROP_NOT_ENUMERABLE)
#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_Promise, x, _ejs_Promise_##x, EJS_PROP_NOT_ENUMERABLE)

    PROTO_METHOD(catch);
    PROTO_METHOD(then);

    _ejs_object_define_value_property (_ejs_Promise_prototype, _ejs_Symbol_toStringTag, _ejs_atom_Promise, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

    OBJ_METHOD(all);
    OBJ_METHOD(race);
    OBJ_METHOD(reject);
    OBJ_METHOD(resolve);

#undef PROTO_METHOD

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_Promise, create, _ejs_Promise_create, EJS_PROP_NOT_ENUMERABLE);
}

static EJSObject*
_ejs_promise_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSPromise);
}

EJS_DEFINE_CLASS(Promise,
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
                 _ejs_promise_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 OP_INHERIT  // [[Scan]]
                 )
