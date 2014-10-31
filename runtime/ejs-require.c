/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <string.h>

#include "ejs.h"
#include "ejs-array.h"
#include "ejs-object.h"
#include "ejs-require.h"
#include "ejs-function.h"
#include "ejs-value.h"
#include "ejs-string.h"
#include "ejs-module.h"
#if IOS || OSX
#include "ejs-objc.h"
#endif

extern EJSModule* _ejs_modules[];
extern EJSClosureFunc _ejs_module_toplevels[];
extern int _ejs_num_modules;

extern EJSExternalModule _ejs_external_modules[];
extern int _ejs_num_external_modules;

static EJSExternalModule builtin_modules[] = {
#if IOS || OSX
    { "objc_internal", _ejs_objc_module_func }
#endif
};
static int num_builtin_modules = sizeof(builtin_modules) / sizeof(builtin_modules[0]);

ejsval _ejs_require EJSVAL_ALIGNMENT;

EJSBool
require_builtin_module (const char* name, ejsval *module)
{
    for (int i = 0; i < num_builtin_modules; i ++) {
        if (!strcmp (builtin_modules[i].name, name)) {
            if (EJSVAL_IS_NULL(builtin_modules[i].cached_module_obj)) {
                //	printf ("require'ing %s.\n", EJSVAL_TO_FLAT_STRING(arg));
                _ejs_gc_add_root (&builtin_modules[i].cached_module_obj);
                builtin_modules[i].cached_module_obj = _ejs_object_new(_ejs_null, &_ejs_Object_specops);
                builtin_modules[i].func(builtin_modules[i].cached_module_obj);
            }
            *module = builtin_modules[i].cached_module_obj;
            return EJS_TRUE;
        }
    }
    return EJS_FALSE;
}

EJSBool
require_external_module (const char* name, ejsval *module)
{
    for (int i = 0; i < _ejs_num_external_modules; i ++) {
        if (!strcmp (_ejs_external_modules[i].name, name)) {
            if (EJSVAL_IS_NULL(_ejs_external_modules[i].cached_module_obj)) {
                _ejs_gc_add_root (&_ejs_external_modules[i].cached_module_obj);
                _ejs_external_modules[i].cached_module_obj = _ejs_object_new(_ejs_null, &_ejs_Object_specops);
                _ejs_external_modules[i].func(_ejs_external_modules[i].cached_module_obj);
            }
            *module = _ejs_external_modules[i].cached_module_obj;
            return EJS_TRUE;
        }
    }
    return EJS_FALSE;
}

EJSBool
require_user_module (const char* name, ejsval *module)
{
    for (int i = 0; i < _ejs_num_modules; i ++) {
        EJSModule* mod = _ejs_modules[i];
        if (mod->module_name && !strcmp (mod->module_name, name)) {
            _ejs_module_toplevels[i](_ejs_null, OBJECT_TO_EJSVAL(mod), 0, NULL);
            *module = OBJECT_TO_EJSVAL(mod);
            return EJS_TRUE;
        }
    }
    return EJS_FALSE;
}

void
_ejs_module_resolve(EJSModule* mod)
{
    for (int i = 0; i < _ejs_num_modules; i ++) {
        if (_ejs_modules[i] == mod) {
            _ejs_module_toplevels[i](_ejs_null, OBJECT_TO_EJSVAL(mod), 0, NULL);
            return;
        }
    }
    EJS_NOT_REACHED();
}

ejsval
_ejs_module_get (ejsval arg)
{
    char* arg_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(arg));

    ejsval module EJSVAL_ALIGNMENT;
    if (require_builtin_module (arg_utf8, &module)) {
        free (arg_utf8);
        return module;
    }
    if (require_external_module (arg_utf8, &module)) {
        free (arg_utf8);
        return module;
    }
    if (require_user_module (arg_utf8, &module)) {
        free (arg_utf8);
        return module;
    }
    _ejs_log ("require('%s') failed: module not included in build.\n", arg_utf8);
    free (arg_utf8);
    return _ejs_null;
}

static ejsval
_ejs_require_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (argc < 1) {
        return _ejs_undefined;
    }

    ejsval arg = args[0];

    if (!EJSVAL_IS_STRING(arg)) {
        _ejs_log ("required called with non-string\n");
        return _ejs_null;
    }

    return _ejs_module_get(arg);
}

void
_ejs_require_init(ejsval global)
{
    _ejs_require = _ejs_function_new_native (_ejs_null, _ejs_atom_require, _ejs_require_impl);
    _ejs_object_setprop (global, _ejs_atom_require, _ejs_require);
  
    int i;
    for (i = 0; i < num_builtin_modules; i ++) {
        builtin_modules[i].cached_module_obj = _ejs_null;
    }

    for (i = 0; i < _ejs_num_external_modules; i ++) {
        _ejs_external_modules[i].cached_module_obj = _ejs_null;
    }

    for (i = 0; i < _ejs_num_modules; i ++) {
        _ejs_init_object ((EJSObject*)_ejs_modules[i], _ejs_Object_prototype, &_ejs_Module_specops);
    }
}
