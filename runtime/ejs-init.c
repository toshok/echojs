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
#include "ejs-generator.h"
#include "ejs-json.h"
#include "ejs-math.h"
#include "ejs-number.h"
#include "ejs-boolean.h"
#include "ejs-object.h"
#include "ejs-module.h"
#include "ejs-process.h"
#include "ejs-promise.h"
#include "ejs-regexp.h"
#include "ejs-require.h"
#include "ejs-stream.h"
#include "ejs-string.h"
#include "ejs-symbol.h"
#include "ejs-timers.h"
#include "ejs-typedarrays.h"
#include "ejs-uri.h"
#include "ejs-value.h"
#include "ejs-weakmap.h"
#include "ejs-weakset.h"
#if IOS || OSX
#include "ejs-xhr.h"
#endif
#include "ejs-map.h"
#include "ejs-set.h"
#if IOS
#include "ejs-webgl.h"
#endif
#include "ejs-proxy.h"
#include "ejs-reflect.h"

// lives in ejs-atoms-gen.c
extern void _ejs_init_static_strings();

const ejsval _ejs_undefined EJSVAL_ALIGNMENT = STATIC_BUILD_EJSVAL(EJSVAL_TAG_UNDEFINED, 0);
ejsval _ejs_nan;
const ejsval _ejs_Infinity EJSVAL_ALIGNMENT = STATIC_BUILD_DOUBLE_EJSVAL(HUGE_VAL);
const ejsval _ejs_null EJSVAL_ALIGNMENT = STATIC_BUILD_EJSVAL(EJSVAL_TAG_NULL, 0);
const ejsval _ejs_true EJSVAL_ALIGNMENT = STATIC_BUILD_BOOLEAN_EJSVAL(EJS_TRUE);
const ejsval _ejs_false EJSVAL_ALIGNMENT = STATIC_BUILD_BOOLEAN_EJSVAL(EJS_FALSE);
const ejsval _ejs_zero EJSVAL_ALIGNMENT = STATIC_BUILD_DOUBLE_EJSVAL(0);
const ejsval _ejs_one EJSVAL_ALIGNMENT = STATIC_BUILD_DOUBLE_EJSVAL(1);

ejsval _ejs__ejs EJSVAL_ALIGNMENT;
ejsval _ejs_global EJSVAL_ALIGNMENT;


EJS_NATIVE_FUNC(_ejs_eval) {
  _ejs_throw_nativeerror_utf8 (EJS_ERROR, "EJS doesn't support eval()");
  return _ejs_undefined;
}

EJS_NATIVE_FUNC(_ejs_unhandledException) {
    ejsval exc = _ejs_undefined;

    if (argc > 0)
        exc = args[0];

    printf ("unhandled exception: ");
    
    if (EJSVAL_IS_UNDEFINED(exc)) {
        EJS_NOT_IMPLEMENTED();
    }
    else {
        ejsval str = ToString(exc);

        printf ("%s\n", _ejs_string_to_utf8(_ejs_string_flatten(str)));
    }
    exit(-1);
    return _ejs_undefined;
}

static void
_ejs_init_classes()
{
    _ejs_Class_initialize (&_ejs_Arguments_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Array_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_ArrayIterator_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Boolean_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Date_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Error_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Function_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Map_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_MapIterator_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WeakMap_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WeakSet_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Module_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Proxy_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Promise_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Set_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_SetIterator_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Number_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_RegExp_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_String_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_StringIterator_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Symbol_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_ArrayBuffer_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Timer_specops, &_ejs_Object_specops);

    _ejs_Class_initialize (&_ejs_Int8Array_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Uint8Array_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Uint8ClampedArray_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Int16Array_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Uint16Array_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Int32Array_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Uint32Array_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Float32Array_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_Float64Array_specops, &_ejs_Object_specops);

    _ejs_Class_initialize (&_ejs_DataView_specops, &_ejs_Object_specops);

    _ejs_Class_initialize (&_ejs_Generator_specops, &_ejs_Object_specops);

    _ejs_Class_initialize (&_ejs_IteratorWrapper_specops, &_ejs_Object_specops);

#if IOS
    _ejs_Class_initialize (&_ejs_WebGLRenderingContext_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WebGLBuffer_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WebGLFramebuffer_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WebGLRenderbuffer_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WebGLProgram_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WebGLShader_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WebGLShaderPrecisionFormat_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WebGLTexture_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WebGLActiveInfo_specops, &_ejs_Object_specops);
    _ejs_Class_initialize (&_ejs_WebGLUniformLocation_specops, &_ejs_Object_specops);
#endif
#if IOS || OSX
    _ejs_Class_initialize (&_ejs_XMLHttpRequest_specops, &_ejs_Object_specops);
#endif
}

// root every global ejsval the runtime stores builtins into.  these
// statics live in the data segment, which the collector does not scan;
// relying on each *_init function to root (or connect to the object
// graph) whatever it creates proved fragile -- a missed root means the
// first collection frees an object that is still referenced (see
// _ejs_iterator_init_proto).  registering a root for a still-zeroed
// ejsval is harmless.
static void
_ejs_root_builtin_globals(void)
{
    extern ejsval _ejs_Array;
    extern ejsval _ejs_ArrayBuffer;
    extern ejsval _ejs_ArrayIterator;
    extern ejsval _ejs_Boolean;
    extern ejsval _ejs_DataView;
    extern ejsval _ejs_Date;
    extern ejsval _ejs_Error;
    extern ejsval _ejs_Error_prototype;
    extern ejsval _ejs_EvalError;
    extern ejsval _ejs_EvalError_prototype;
    extern ejsval _ejs_Float32Array;
    extern ejsval _ejs_Float32Array_prototype;
    extern ejsval _ejs_Float64Array;
    extern ejsval _ejs_Float64Array_prototype;
    extern ejsval _ejs_Function;
    extern ejsval _ejs_Int16Array;
    extern ejsval _ejs_Int16Array_prototype;
    extern ejsval _ejs_Int32Array;
    extern ejsval _ejs_Int32Array_prototype;
    extern ejsval _ejs_Int8Array;
    extern ejsval _ejs_Int8Array_prototype;
    extern ejsval _ejs_JSON;
    extern ejsval _ejs_Map;
    extern ejsval _ejs_MapIterator;
    extern ejsval _ejs_Math;
    extern ejsval _ejs_Number;
    extern ejsval _ejs_Object;
    extern ejsval _ejs_Process;
    extern ejsval _ejs_Promise;
    extern ejsval _ejs_Proxy;
    extern ejsval _ejs_RangeError;
    extern ejsval _ejs_RangeError_prototype;
    extern ejsval _ejs_ReferenceError;
    extern ejsval _ejs_ReferenceError_prototype;
    extern ejsval _ejs_Reflect;
    extern ejsval _ejs_RegExp;
    extern ejsval _ejs_SetIterator;
    extern ejsval _ejs_String;
    extern ejsval _ejs_StringIterator;
    extern ejsval _ejs_Symbol;
    extern ejsval _ejs_Symbol_create;
    extern ejsval _ejs_Symbol_hasInstance;
    extern ejsval _ejs_Symbol_isConcatSpreadable;
    extern ejsval _ejs_Symbol_iterator;
    extern ejsval _ejs_Symbol_match;
    extern ejsval _ejs_Symbol_replace;
    extern ejsval _ejs_Symbol_search;
    extern ejsval _ejs_Symbol_species;
    extern ejsval _ejs_Symbol_split;
    extern ejsval _ejs_Symbol_toPrimitive;
    extern ejsval _ejs_Symbol_toStringTag;
    extern ejsval _ejs_Symbol_unscopables;
    extern ejsval _ejs_SyntaxError;
    extern ejsval _ejs_SyntaxError_prototype;
    extern ejsval _ejs_Timer;
    extern ejsval _ejs_TypeError;
    extern ejsval _ejs_TypeError_prototype;
    extern ejsval _ejs_URIError;
    extern ejsval _ejs_URIError_prototype;
    extern ejsval _ejs_Uint16Array;
    extern ejsval _ejs_Uint16Array_prototype;
    extern ejsval _ejs_Uint32Array;
    extern ejsval _ejs_Uint32Array_prototype;
    extern ejsval _ejs_Uint8Array;
    extern ejsval _ejs_Uint8Array_prototype;
    extern ejsval _ejs_Uint8ClampedArray;
    extern ejsval _ejs_Uint8ClampedArray_prototype;
    extern ejsval _ejs_WeakMap;
    extern ejsval _ejs_WeakSet;
    extern ejsval _ejs__ejs;
    extern ejsval _ejs_clearInterval;
    extern ejsval _ejs_clearTimeout;
    extern ejsval _ejs_console;
    extern ejsval _ejs_decodeURI;
    extern ejsval _ejs_decodeURIComponent;
    extern ejsval _ejs_encodeURI;
    extern ejsval _ejs_encodeURIComponent;
    extern ejsval _ejs_isFinite;
    extern ejsval _ejs_isNaN;
    extern ejsval _ejs_parseFloat;
    extern ejsval _ejs_parseInt;
    extern ejsval _ejs_require;
    extern ejsval _ejs_setInterval;
    extern ejsval _ejs_setTimeout;

    _ejs_gc_add_root (&_ejs_Array);
    _ejs_gc_add_root (&_ejs_ArrayBuffer);
    _ejs_gc_add_root (&_ejs_ArrayIterator);
    _ejs_gc_add_root (&_ejs_Boolean);
    _ejs_gc_add_root (&_ejs_DataView);
    _ejs_gc_add_root (&_ejs_Date);
    _ejs_gc_add_root (&_ejs_Error);
    _ejs_gc_add_root (&_ejs_Error_prototype);
    _ejs_gc_add_root (&_ejs_EvalError);
    _ejs_gc_add_root (&_ejs_EvalError_prototype);
    _ejs_gc_add_root (&_ejs_Float32Array);
    _ejs_gc_add_root (&_ejs_Float32Array_prototype);
    _ejs_gc_add_root (&_ejs_Float64Array);
    _ejs_gc_add_root (&_ejs_Float64Array_prototype);
    _ejs_gc_add_root (&_ejs_Function);
    _ejs_gc_add_root (&_ejs_Int16Array);
    _ejs_gc_add_root (&_ejs_Int16Array_prototype);
    _ejs_gc_add_root (&_ejs_Int32Array);
    _ejs_gc_add_root (&_ejs_Int32Array_prototype);
    _ejs_gc_add_root (&_ejs_Int8Array);
    _ejs_gc_add_root (&_ejs_Int8Array_prototype);
    _ejs_gc_add_root (&_ejs_JSON);
    _ejs_gc_add_root (&_ejs_Map);
    _ejs_gc_add_root (&_ejs_MapIterator);
    _ejs_gc_add_root (&_ejs_Math);
    _ejs_gc_add_root (&_ejs_Number);
    _ejs_gc_add_root (&_ejs_Object);
    _ejs_gc_add_root (&_ejs_Process);
    _ejs_gc_add_root (&_ejs_Promise);
    _ejs_gc_add_root (&_ejs_Proxy);
    _ejs_gc_add_root (&_ejs_RangeError);
    _ejs_gc_add_root (&_ejs_RangeError_prototype);
    _ejs_gc_add_root (&_ejs_ReferenceError);
    _ejs_gc_add_root (&_ejs_ReferenceError_prototype);
    _ejs_gc_add_root (&_ejs_Reflect);
    _ejs_gc_add_root (&_ejs_RegExp);
    _ejs_gc_add_root (&_ejs_SetIterator);
    _ejs_gc_add_root (&_ejs_String);
    _ejs_gc_add_root (&_ejs_StringIterator);
    _ejs_gc_add_root (&_ejs_Symbol);
    _ejs_gc_add_root (&_ejs_Symbol_create);
    _ejs_gc_add_root (&_ejs_Symbol_hasInstance);
    _ejs_gc_add_root (&_ejs_Symbol_isConcatSpreadable);
    _ejs_gc_add_root (&_ejs_Symbol_iterator);
    _ejs_gc_add_root (&_ejs_Symbol_match);
    _ejs_gc_add_root (&_ejs_Symbol_replace);
    _ejs_gc_add_root (&_ejs_Symbol_search);
    _ejs_gc_add_root (&_ejs_Symbol_species);
    _ejs_gc_add_root (&_ejs_Symbol_split);
    _ejs_gc_add_root (&_ejs_Symbol_toPrimitive);
    _ejs_gc_add_root (&_ejs_Symbol_toStringTag);
    _ejs_gc_add_root (&_ejs_Symbol_unscopables);
    _ejs_gc_add_root (&_ejs_SyntaxError);
    _ejs_gc_add_root (&_ejs_SyntaxError_prototype);
    _ejs_gc_add_root (&_ejs_Timer);
    _ejs_gc_add_root (&_ejs_TypeError);
    _ejs_gc_add_root (&_ejs_TypeError_prototype);
    _ejs_gc_add_root (&_ejs_URIError);
    _ejs_gc_add_root (&_ejs_URIError_prototype);
    _ejs_gc_add_root (&_ejs_Uint16Array);
    _ejs_gc_add_root (&_ejs_Uint16Array_prototype);
    _ejs_gc_add_root (&_ejs_Uint32Array);
    _ejs_gc_add_root (&_ejs_Uint32Array_prototype);
    _ejs_gc_add_root (&_ejs_Uint8Array);
    _ejs_gc_add_root (&_ejs_Uint8Array_prototype);
    _ejs_gc_add_root (&_ejs_Uint8ClampedArray);
    _ejs_gc_add_root (&_ejs_Uint8ClampedArray_prototype);
    _ejs_gc_add_root (&_ejs_WeakMap);
    _ejs_gc_add_root (&_ejs_WeakSet);
    _ejs_gc_add_root (&_ejs__ejs);
    _ejs_gc_add_root (&_ejs_clearInterval);
    _ejs_gc_add_root (&_ejs_clearTimeout);
    _ejs_gc_add_root (&_ejs_console);
    _ejs_gc_add_root (&_ejs_decodeURI);
    _ejs_gc_add_root (&_ejs_decodeURIComponent);
    _ejs_gc_add_root (&_ejs_encodeURI);
    _ejs_gc_add_root (&_ejs_encodeURIComponent);
    _ejs_gc_add_root (&_ejs_isFinite);
    _ejs_gc_add_root (&_ejs_isNaN);
    _ejs_gc_add_root (&_ejs_parseFloat);
    _ejs_gc_add_root (&_ejs_parseInt);
    _ejs_gc_add_root (&_ejs_require);
    _ejs_gc_add_root (&_ejs_setInterval);
    _ejs_gc_add_root (&_ejs_setTimeout);
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

    _ejs_root_builtin_globals();

    // initialization or ECMA262 builtins
    _ejs_gc_add_root (&_ejs_global);
    _ejs_global = _ejs_object_new (_ejs_null, &_ejs_Object_specops);

    _ejs_nan = NUMBER_TO_EJSVAL(nan("7734"));

    _ejs_object_setprop (_ejs_global, _ejs_atom_undefined, _ejs_undefined);
    _ejs_object_setprop (_ejs_global, _ejs_atom_NaN, _ejs_nan);
    _ejs_object_setprop (_ejs_global, _ejs_atom_Infinity, _ejs_Infinity);
    _ejs_object_setprop (_ejs_global, _ejs_atom_eval, _ejs_function_new_native (_ejs_undefined, _ejs_atom_eval, _ejs_eval));


    _ejs_object_init_proto();

    _ejs_function_init(_ejs_global);

    _ejs_object_init(_ejs_global);

    _ejs_symbol_init(_ejs_global);

    _ejs_iterator_init_proto();
    _ejs_iterator_wrapper_init(_ejs_global);

    _ejs_reflect_init(_ejs_global);
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
    _ejs_uri_init(_ejs_global);
    _ejs_timers_init(_ejs_global);

    // ES6 bits
    _ejs_generator_init(_ejs_global);
    _ejs_promise_init(_ejs_global);
    _ejs_proxy_init(_ejs_global);
    _ejs_map_init(_ejs_global);
    _ejs_set_init(_ejs_global);
    _ejs_weakmap_init(_ejs_global);
    _ejs_weakset_init(_ejs_global);

    _ejs_typedarrays_init(_ejs_global);
#if IOS
    _ejs_webgl_init(_ejs_global);
#endif

#define GLOBAL_METHOD(x) EJS_MACRO_START                                \
    _ejs_##x = _ejs_function_new_native (_ejs_null, _ejs_atom_##x, _ejs_##x##_impl); \
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
    _ejs_stream_init(_ejs_global);
    _ejs_process_init(_ejs_global, argc, argv);

#if IOS || OSX
    _ejs_xmlhttprequest_init(_ejs_global);
#endif

    // a special global (__ejs) under which we can stuff other
    // semi-useful runtime features, like a call to force a GC.  the
    // compiler also uses the presence of __ejs to disable
    // buggy/nonfunctional code (like those that use regexps)
    _ejs__ejs = _ejs_object_new (_ejs_null, &_ejs_Object_specops);
    _ejs_object_setprop (_ejs_global, _ejs_atom___ejs, _ejs__ejs);

    _ejs_GC_init(_ejs__ejs);
    _ejs_gc_allocate_oom_exceptions();

    EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs__ejs, unhandledException, _ejs_unhandledException, 0);
}
