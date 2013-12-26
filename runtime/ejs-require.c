/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <string.h>

#include "ejs.h"
#include "ejs-object.h"
#include "ejs-require.h"
#include "ejs-function.h"
#include "ejs-value.h"
#include "ejs-builtin-modules.h"
#include "ejs-string.h"
#include "ejs-objc.h"

extern EJSRequire _ejs_require_map[];
extern EJSExternalModuleRequire _ejs_external_module_require_map[];

static EJSRequire builtin_module_map[] = {
    { "path", _ejs_path_module_func },
    { "fs", _ejs_fs_module_func },
    { "child_process", _ejs_child_process_module_func },
    { "objc_internal", _ejs_objc_module_func }
};
static int num_builtin_modules = sizeof(builtin_module_map) / sizeof(builtin_module_map[0]);

ejsval _ejs_require EJSVAL_ALIGNMENT;

EJSBool
require_builtin_module (const char* name, ejsval *module)
{
    int i;
    for (i = 0; i < num_builtin_modules; i ++) {
        if (!strcmp (builtin_module_map[i].name, name)) {
            if (EJSVAL_IS_NULL(builtin_module_map[i].cached_exports)) {
                //	printf ("require'ing %s.\n", EJSVAL_TO_FLAT_STRING(arg));
                _ejs_gc_add_root (&builtin_module_map[i].cached_exports);
                builtin_module_map[i].cached_exports = _ejs_object_new(_ejs_null, &_ejs_object_specops);
                builtin_module_map[i].func(_ejs_null, _ejs_undefined, 1, &builtin_module_map[i].cached_exports);
            }
            *module = builtin_module_map[i].cached_exports;
            return EJS_TRUE;
        }
    }
    return EJS_FALSE;
}

EJSBool
require_external_module (const char* name, ejsval *module)
{
    int i = 0;
    while (1) {
        if (!_ejs_external_module_require_map[i].name) {
            return EJS_FALSE;
        }

        if (!strcmp (_ejs_external_module_require_map[i].name, name)) {
            if (EJSVAL_IS_NULL(_ejs_external_module_require_map[i].cached_exports)) {
                _ejs_gc_add_root (&_ejs_external_module_require_map[i].cached_exports);
                _ejs_external_module_require_map[i].cached_exports = _ejs_object_new(_ejs_null, &_ejs_object_specops);
                _ejs_external_module_require_map[i].func(_ejs_external_module_require_map[i].cached_exports);
            }
            *module = _ejs_external_module_require_map[i].cached_exports;
            return EJS_TRUE;
        }
        i++;
    }
}

EJSBool
require_user_module (const char* name, ejsval *module)
{
    int i = 0;
    while (1) {
        EJSRequire* map = &_ejs_require_map[i];
        if (!map->name) {
            return EJS_FALSE;
        }
        if (!strcmp (map->name, name)) {
            if (EJSVAL_IS_NULL(map->cached_exports)) {
                _ejs_log ("require'ing %s.\n", name);
                _ejs_gc_add_root (&map->cached_exports);
                map->cached_exports = _ejs_object_new(_ejs_null, &_ejs_object_specops);
                ejsval prev_exports = _ejs_object_getprop (_ejs_global, _ejs_atom_exports);
                _ejs_gc_add_root (&prev_exports);
                _ejs_object_setprop(_ejs_global, _ejs_atom_exports, map->cached_exports);

                ejsval args[1];
                args[0] = map->cached_exports;
                map->func (_ejs_null, _ejs_undefined, 1, args);

                _ejs_object_setprop(_ejs_global, _ejs_atom_exports, prev_exports);
                _ejs_gc_remove_root (&prev_exports);
                _ejs_log ("done require'ing %s.\n", name);
            }
            *module = map->cached_exports;
            return EJS_TRUE;
        }
        i++;
    }
}


static ejsval
_ejs_require_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    _ejs_log ("in _ejs_require_impl");
    if (argc < 1) {
        return _ejs_undefined;
    }

    ejsval arg = args[0];

    if (!EJSVAL_IS_STRING(arg)) {
        _ejs_log ("required called with non-string\n");
        return _ejs_null;
    }

    char* arg_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(arg));

    ejsval module EJSVAL_ALIGNMENT;
    if (require_builtin_module (arg_utf8, &module)) {
        _ejs_log ("returning builtin module for %s", arg_utf8);
        free (arg_utf8);
        return module;
    }
    _ejs_log ("1");
    if (require_external_module (arg_utf8, &module)) {
        _ejs_log ("returning external module for %s", arg_utf8);
        free (arg_utf8);
        return module;
    }
    _ejs_log ("2");
    if (require_user_module (arg_utf8, &module)) {
        _ejs_log ("returning user module for %s", arg_utf8);
        free (arg_utf8);
        return module;
    }
    _ejs_log ("require('%s') failed: module not included in build.\n", arg_utf8);
    free (arg_utf8);
    return _ejs_null;
}

void
_ejs_require_init(ejsval global)
{
    _ejs_require = _ejs_function_new_native (_ejs_null, _ejs_atom_require, _ejs_require_impl);
    _ejs_object_setprop (global, _ejs_atom_require, _ejs_require);
  
    int i;
    for (i = 0; i < num_builtin_modules; i ++) {
        builtin_module_map[i].cached_exports = _ejs_null;
    }

    i = 0;
    while (1) {
        if (!_ejs_external_module_require_map[i].name)
            break;
        _ejs_external_module_require_map[i].cached_exports = _ejs_null;
        i++;
    }

    i = 0;
    while (1) {
        if (!_ejs_require_map[i].name)
            break;
        _ejs_require_map[i].cached_exports = _ejs_null;
        i++;
    }
}
