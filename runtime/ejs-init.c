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
#include "ejs-error.h"
#include "ejs-exception.h"
#include "ejs-function.h"
#include "ejs-json.h"
#include "ejs-math.h"
#include "ejs-number.h"
#include "ejs-boolean.h"
#include "ejs-object.h"
#include "ejs-process.h"
#include "ejs-regexp.h"
#include "ejs-require.h"
#include "ejs-string.h"
#include "ejs-symbol.h"
#include "ejs-typedarrays.h"
#include "ejs-uri.h"
#include "ejs-value.h"
#include "ejs-xhr.h"
#include "ejs-map.h"
#if IOS
#include "ejs-webgl.h"
#endif
#include "ejs-proxy.h"

const ejsval _ejs_undefined EJSVAL_ALIGNMENT = STATIC_BUILD_EJSVAL(EJSVAL_TAG_UNDEFINED, 0);
ejsval _ejs_nan;
const ejsval _ejs_null EJSVAL_ALIGNMENT = STATIC_BUILD_EJSVAL(EJSVAL_TAG_NULL, 0);
const ejsval _ejs_true EJSVAL_ALIGNMENT = STATIC_BUILD_BOOLEAN_EJSVAL(EJS_TRUE);
const ejsval _ejs_false EJSVAL_ALIGNMENT = STATIC_BUILD_BOOLEAN_EJSVAL(EJS_FALSE);
const ejsval _ejs_zero EJSVAL_ALIGNMENT = STATIC_BUILD_DOUBLE_EJSVAL(0);
const ejsval _ejs_one EJSVAL_ALIGNMENT = STATIC_BUILD_DOUBLE_EJSVAL(1);

ejsval _ejs_global EJSVAL_ALIGNMENT;

/* useful strings literals */
#include "ejs-atoms-gen.c"

ejsval
_ejs_eval (ejsval env, ejsval _this, uint32_t argc, ejsval* args)
{
  _ejs_throw_nativeerror_utf8 (EJS_ERROR, "EJS doesn't support eval()");
  return _ejs_undefined;
}

static void
_ejs_init_classes()
{
    _ejs_Class_initialize (&_ejs_arguments_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_array_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_boolean_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_date_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_error_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_function_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_map_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_number_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_regexp_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_string_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_symbol_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_arraybuffer_specops, &_ejs_object_specops);
#if IOS
    _ejs_Class_initialize (&_ejs_WebGLRenderingContext_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_WebGLBuffer_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_WebGLFramebuffer_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_WebGLRenderbuffer_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_WebGLProgram_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_WebGLShader_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_WebGLTexture_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_WebGLActiveInfo_specops, &_ejs_object_specops);
    _ejs_Class_initialize (&_ejs_WebGLUniformLocation_specops, &_ejs_object_specops);
#endif
    _ejs_Class_initialize (&_ejs_xmlhttprequest_specops, &_ejs_object_specops);
}

void
_ejs_init(int argc, char** argv)
{
    // process class inheritance
    _ejs_init_classes();

    // initialize our atoms before anything else
    _ejs_init_static_strings();

    _ejs_gc_init();
    _ejs_exception_init();

    // initialization or ECMA262 builtins
    _ejs_gc_add_root (&_ejs_global);
    _ejs_global = _ejs_object_new (_ejs_null, &_ejs_object_specops);

    _ejs_nan = NUMBER_TO_EJSVAL(nan("7734"));

    _ejs_object_setprop (_ejs_global, _ejs_atom_undefined, _ejs_undefined);
    _ejs_object_setprop (_ejs_global, _ejs_atom_NaN, _ejs_nan);
    _ejs_object_setprop (_ejs_global, _ejs_atom_eval, _ejs_function_new_native (_ejs_undefined, _ejs_atom_eval, _ejs_eval));


    _ejs_object_init_proto();

    _ejs_function_init(_ejs_global);

    _ejs_object_init(_ejs_global);

    _ejs_error_init(_ejs_global);
    _ejs_arguments_init(_ejs_global);
    _ejs_array_init(_ejs_global);
    _ejs_boolean_init (_ejs_global);
    _ejs_string_init(_ejs_global);
    _ejs_number_init(_ejs_global);
    _ejs_regexp_init(_ejs_global);
    _ejs_date_init(_ejs_global);
    _ejs_json_init(_ejs_global);
    _ejs_math_init(_ejs_global);

    // ES6 bits
    _ejs_proxy_init(_ejs_global);
    _ejs_symbol_init(_ejs_global);
    _ejs_map_init(_ejs_global);

    _ejs_typedarrays_init(_ejs_global);
#if IOS
    _ejs_webgl_init(_ejs_global);
#endif

#define GLOBAL_METHOD(x) EJS_MACRO_START                                \
    _ejs_##x = _ejs_function_new_native (_ejs_null, _ejs_atom_##x, (EJSClosureFunc)_ejs_##x##_impl); \
    _ejs_object_setprop (_ejs_global, _ejs_atom_##x, _ejs_##x);         \
    EJS_MACRO_END

    GLOBAL_METHOD(isNaN);
    GLOBAL_METHOD(isFinite);
    GLOBAL_METHOD(parseInt);
    GLOBAL_METHOD(parseFloat);

    GLOBAL_METHOD(decodeURI);
    GLOBAL_METHOD(decodeURIComponent);
    GLOBAL_METHOD(encodeURI);
    GLOBAL_METHOD(encodeURIComponent);

#undef GLOBAL_METHOD

    // the node-like api we support in order for our driver to
    // function.  this should really be a separate opt-in .a/.so.
    _ejs_require_init(_ejs_global);
    _ejs_console_init(_ejs_global);
    _ejs_process_init(_ejs_global, argc, argv);

    _ejs_xmlhttprequest_init(_ejs_global);

    // a special global (__ejs) under which we can stuff other
    // semi-useful runtime features, like a call to force a GC.  the
    // compiler also uses the presence of __ejs to disable
    // buggy/nonfunctional code (like those that use regexps)
    ejsval _ejs_ejs_global = _ejs_object_new (_ejs_null, &_ejs_object_specops);
    _ejs_object_setprop (_ejs_global, _ejs_atom___ejs, _ejs_ejs_global);

    _ejs_GC_init(_ejs_ejs_global);
}
