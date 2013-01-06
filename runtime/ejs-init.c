/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <math.h>

#include "ejs.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-arguments.h"
#include "ejs-array.h"
#include "ejs-console.h"
#include "ejs-date.h"
#include "ejs-function.h"
#include "ejs-json.h"
#include "ejs-math.h"
#include "ejs-number.h"
#include "ejs-object.h"
#include "ejs-process.h"
#include "ejs-regexp.h"
#include "ejs-require.h"
#include "ejs-string.h"
#include "ejs-uri.h"
#include "ejs-value.h"

const ejsval _ejs_undefined = STATIC_BUILD_EJSVAL(EJSVAL_TAG_UNDEFINED, 0);
const ejsval _ejs_nan = STATIC_BUILD_EJSVAL(0, 0x40be360000000000);
const ejsval _ejs_null = STATIC_BUILD_EJSVAL(EJSVAL_TAG_NULL, 0);
const ejsval _ejs_true = STATIC_BUILD_BOOLEAN_EJSVAL(EJS_TRUE);
const ejsval _ejs_false = STATIC_BUILD_BOOLEAN_EJSVAL(EJS_FALSE);
const ejsval _ejs_zero = STATIC_BUILD_DOUBLE_EJSVAL(0);
const ejsval _ejs_one = STATIC_BUILD_DOUBLE_EJSVAL(1);

ejsval _ejs_global;


/* useful strings literals */
#define EJS_ATOM(atom) static const EJSPrimString _ejs_string_##atom = { .gc_header = (EJS_STRING_FLAT<<EJS_GC_USER_FLAGS_SHIFT), .length = sizeof(EJS_STRINGIFY(atom))-1, .data = { .flat = EJS_STRINGIFY(atom) }}; ejsval _ejs_atom_##atom; const char* _ejs_cstring_##atom = EJS_STRINGIFY(atom);
#include "ejs-atoms.h"
#undef EJS_ATOM
static const EJSPrimString _ejs_string_empty = { .gc_header = (EJS_STRING_FLAT<<EJS_GC_USER_FLAGS_SHIFT), .length = 0, .data = { .flat = "" }};
ejsval _ejs_atom_empty;

static void
_ejs_init_static_strings()
{
#define EJS_ATOM(atom) _ejs_atom_##atom = STRING_TO_EJSVAL((EJSPrimString*)&_ejs_string_##atom); _ejs_gc_add_named_root (_ejs_atom_##atom); //STATIC_BUILD_EJSVAL(EJSVAL_TAG_STRING, (uintptr_t)&_ejs_string_##atom);
#include "ejs-atoms.h"
#undef EJS_ATOM
    _ejs_atom_empty = STRING_TO_EJSVAL((EJSPrimString*)&_ejs_string_empty); _ejs_gc_add_named_root (_ejs_atom_empty); //STATIC_BUILD_EJSVAL(EJSVAL_TAG_STRING, (uintptr_t)&_ejs_string_empty);
}

void
_ejs_init(int argc, char** argv)
{
    // initialize our constants before anything else
    START_SHADOW_STACK_FRAME;

    _ejs_init_static_strings();

    _ejs_gc_init();

    _ejs_gc_add_named_root (_ejs_global);

    _ejs_global = OBJECT_TO_EJSVAL(_ejs_object_alloc_instance());
    EJSObject *global = EJSVAL_TO_OBJECT(_ejs_global);
    _ejs_init_object (global, _ejs_null, &_ejs_object_specops);

    _ejs_object_setprop_utf8 (_ejs_global, "undefined", _ejs_undefined);
    _ejs_object_setprop_utf8 (_ejs_global, "NaN", _ejs_nan);
    _ejs_object_setprop_utf8 (_ejs_global, "__ejs", _ejs_true);

    _ejs_object_init_proto();

    _ejs_function_init(_ejs_global);
    _ejs_object_init(_ejs_global);
    _ejs_arguments_init(_ejs_global);
    _ejs_array_init(_ejs_global);
    _ejs_string_init(_ejs_global);
    _ejs_number_init(_ejs_global);
    _ejs_regexp_init(_ejs_global);
    _ejs_date_init(_ejs_global);
    _ejs_require_init(_ejs_global);
    _ejs_console_init(_ejs_global);
    _ejs_process_init(_ejs_global, argc, argv);

    _ejs_json_init(_ejs_global);
    _ejs_math_init(_ejs_global);

#define GLOBAL_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_global, EJS_STRINGIFY(x), _ejs_##x)

    GLOBAL_METHOD(isNaN);
    GLOBAL_METHOD(isFinite);
    GLOBAL_METHOD(parseInt);
    GLOBAL_METHOD(parseFloat);

    GLOBAL_METHOD(decodeURI);
    GLOBAL_METHOD(decodeURIComponent);
    GLOBAL_METHOD(encodeURI);
    GLOBAL_METHOD(encodeURIComponent);

#undef GLOBAL_METHOD
    END_SHADOW_STACK_FRAME;
}
