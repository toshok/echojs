/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_function_h_
#define _ejs_function_h_

#include "ejs.h"
#include "ejs-object.h"
#include "ejs-closureenv.h"

#define EJS_CALL_FLAGS_CALL         0
#define EJS_CALL_FLAGS_CONSTRUCT    (1 << 0) // function being called as a constructor

typedef enum {
    CONSTRUCTOR_KIND_BASE = 1,
    CONSTRUCTOR_KIND_DERIVED
} EJSConstructorKind;

typedef enum { 
    FUNCTION_KIND_NORMAL,
    FUNCTION_KIND_GENERATOR,
    FUNCTION_KIND_CLASS_CONSTRUCTOR,
} EJSFunctionKind;

typedef struct {
    /* object header */
    EJSObject obj;

    EJSClosureFunc func;
    ejsval   env;

    EJSBool  bound;

    EJSFunctionKind function_kind;
    EJSConstructorKind constructor_kind;

} EJSFunction;


EJS_BEGIN_DECLS

#define EJSVAL_IS_BOUND_FUNCTION(o) (EJSVAL_IS_FUNCTION(o) && ((EJSFunction*)EJSVAL_TO_OBJECT(o))->bound)

#define EJS_INSTALL_ATOM_FUNCTION(o,n,f) EJS_MACRO_START                \
    ejsval tmpfunc = _ejs_function_new_native (_ejs_null, _ejs_atom_##n, f); \
    _ejs_object_setprop (o, _ejs_atom_##n, tmpfunc);                    \
    EJS_MACRO_END

#define EJS_INSTALL_ATOM_FUNCTION_VAL(o,n,f) ({                         \
    ejsval tmpfunc = _ejs_function_new_native (_ejs_null, _ejs_atom_##n, f); \
    _ejs_object_setprop (o, _ejs_atom_##n, tmpfunc);            \
    tmpfunc; })

#define EJS_INSTALL_ATOM_FUNCTION_FLAGS(o,n,f,flags) EJS_MACRO_START         \
    _ejs_object_define_value_property (o, _ejs_atom_##n, _ejs_function_new_native (_ejs_null, _ejs_atom_##n, f), flags); \
    EJS_MACRO_END

#define EJS_INSTALL_ATOM_FUNCTION_LEN_FLAGS(o,n,f,l,flags) EJS_MACRO_START  \
    ejsval __f = _ejs_function_new_native (_ejs_null, _ejs_atom_##n, f); \
    _ejs_object_define_value_property (__f, _ejs_atom_length, NUMBER_TO_EJSVAL(l), EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE); \
    _ejs_object_define_value_property (o, _ejs_atom_##n, __f, flags); \
    EJS_MACRO_END

#define EJS_INSTALL_FUNCTION(o,n,f) EJS_MACRO_START                    \
    ejsval funcname = _ejs_string_new_utf8(n);                          \
    ejsval tmpfunc = _ejs_function_new_native (_ejs_null, funcname, f); \
    _ejs_object_setprop (o, funcname, tmpfunc);                         \
    EJS_MACRO_END

#define EJS_INSTALL_FUNCTION_ENV(o,n,f,env) EJS_MACRO_START                \
    ejsval funcname = _ejs_string_new_utf8(n);                          \
    ejsval tmpfunc = _ejs_function_new_native (env, funcname, f); \
    _ejs_object_setprop (o, funcname, tmpfunc);                         \
    EJS_MACRO_END

#define EJS_INSTALL_FUNCTION_FLAGS(o,n,f,flags) EJS_MACRO_START         \
    ejsval funcname = _ejs_string_new_utf8(n);                          \
    ejsval tmpfunc = _ejs_function_new_native (_ejs_null, funcname, f); \
    _ejs_object_define_value_property (o, funcname, tmpfunc, flags);    \
EJS_MACRO_END

#define EJS_INSTALL_SYMBOL_FUNCTION_FLAGS(o,n,f,flags) EJS_MACRO_START         \
    ejsval tmpfunc = _ejs_function_new_native (_ejs_null, _ejs_atom_##n, f); \
    _ejs_object_define_value_property (o, _ejs_Symbol_##n, tmpfunc, flags);    \
EJS_MACRO_END

#define EJS_INSTALL_GETTER(o,n,f) EJS_MACRO_START                     \
    ejsval key = _ejs_string_new_utf8(n);                               \
    ejsval tmpfunc = _ejs_function_new_native (_ejs_null, key, f); \
    _ejs_object_define_accessor_property (o, key, tmpfunc, _ejs_undefined, EJS_PROP_FLAGS_GETTER_SET); \
    EJS_MACRO_END

#define EJS_INSTALL_ATOM_GETTER(o,n,f) EJS_MACRO_START                     \
    ejsval tmpfunc = _ejs_function_new_native (_ejs_null, _ejs_atom_##n, f); \
    _ejs_object_define_accessor_property (o, _ejs_atom_##n, tmpfunc, _ejs_undefined, EJS_PROP_FLAGS_GETTER_SET); \
    EJS_MACRO_END

#define EJS_INSTALL_SYMBOL_GETTER(o,n,f) EJS_MACRO_START                     \
    ejsval tmpfunc = _ejs_function_new_native (_ejs_null, _ejs_atom_##n, f); \
    _ejs_object_define_accessor_property (o, _ejs_Symbol_##n, tmpfunc, _ejs_undefined, EJS_PROP_FLAGS_GETTER_SET); \
    EJS_MACRO_END

#define EJS_INSTALL_ATOM_ACCESSORS(o,n,g,s) EJS_MACRO_START                  \
    ejsval tmpfunc1 = _ejs_function_new_native (_ejs_null, _ejs_atom_##n, g); \
    ejsval tmpfunc2 = _ejs_function_new_native (_ejs_null, _ejs_atom_##n, s); \
    _ejs_object_define_accessor_property (o, _ejs_atom_##n, tmpfunc1, tmpfunc2, EJS_PROP_FLAGS_GETTER_SET | EJS_PROP_FLAGS_SETTER_SET); \
    EJS_MACRO_END

ejsval  _ejs_invoke_closure (ejsval closure, ejsval* _this, uint32_t argc, ejsval* args, EJSCallFlags callFlags, ejsval newTarget);
EJSBool _ejs_invoke_closure_catch (ejsval* retval, ejsval closure, ejsval _this, uint32_t argc, ejsval* args);
EJSBool _ejs_invoke_func_catch (ejsval* retval, ejsval(*func)(void*), void* data);

EJSBool _ejs_decompose_closure (ejsval closure, EJSClosureFunc* func, ejsval* env, ejsval *_this);

ejsval  _ejs_construct_closure (ejsval closure, ejsval* unused_this, uint32_t argc, ejsval* args, EJSCallFlags callFlags, ejsval newTarget);
ejsval  _ejs_construct_closure_apply (ejsval closure, ejsval* unused_this, uint32_t argc, ejsval* args, EJSCallFlags callFlags, ejsval newTarget);

extern ejsval _ejs_function_new (ejsval env, ejsval name, EJSClosureFunc func);
extern ejsval _ejs_function_new_native (ejsval env, ejsval name, EJSClosureFunc func);
extern ejsval _ejs_function_new_anon (ejsval env, EJSClosureFunc func);
extern ejsval _ejs_function_new_utf8 (ejsval env, const char* name, EJSClosureFunc func);

extern ejsval _ejs_function_new_without_env (ejsval name, EJSClosureFunc func);

extern ejsval _ejs_function_new_utf8_with_proto (ejsval env, const char* name, EJSClosureFunc func, ejsval prototype);

// used during initialization so we don't create a prototype only to throw it away again
extern ejsval _ejs_function_new_without_proto (ejsval env, ejsval name, EJSClosureFunc func);

extern void _ejs_function_set_derived_constructor (ejsval F);
extern void _ejs_function_set_base_constructor (ejsval F);

extern ejsval _ejs_Function;
extern ejsval _ejs_Function_prototype;
extern EJSSpecOps _ejs_Function_specops;

extern void _ejs_function_init(ejsval global);

// used as the __proto__ for a number of builtin objects
EJS_NATIVE_FUNC(_ejs_Function_empty);

EJS_END_DECLS

#endif
