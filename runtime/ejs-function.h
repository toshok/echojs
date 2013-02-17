/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_function_h_
#define _ejs_function_h_

#include "ejs.h"
#include "ejs-object.h"

// for now we just build environments out of EJS objects
typedef ejsval EJSClosureEnv;

typedef ejsval (*EJSClosureFunc) (EJSClosureEnv env, ejsval _this, uint32_t argc, ejsval* args);



typedef struct {
    /* object header */
    EJSObject obj;

    ejsval name;
    EJSClosureFunc func;
    EJSClosureEnv env;

    EJSBool bound;
    ejsval bound_this;
    uint32_t bound_argc;
    ejsval *bound_args;
} EJSFunction;


EJS_BEGIN_DECLS

#define EJS_INSTALL_FUNCTION(o,n,f) EJS_MACRO_START                     \
    ADD_STACK_ROOT(ejsval, funcname, _ejs_string_new_utf8(n));          \
    ADD_STACK_ROOT(ejsval, tmpfunc, _ejs_function_new_native (_ejs_null, funcname, (EJSClosureFunc)f)); \
    _ejs_object_setprop (o, funcname, tmpfunc);                         \
    EJS_MACRO_END

#define EJS_INSTALL_GETTER(o,n,f) EJS_MACRO_START                     \
    ADD_STACK_ROOT(ejsval, key, _ejs_string_new_utf8(n));          \
    ADD_STACK_ROOT(ejsval, tmpfunc, _ejs_function_new (_ejs_null, key, (EJSClosureFunc)f)); \
    _ejs_object_define_accessor_property (o, key, tmpfunc, _ejs_undefined, EJS_PROP_FLAGS_GETTER_SET); \
    EJS_MACRO_END

ejsval  _ejs_invoke_closure (ejsval closure, ejsval _this, uint32_t argc, ejsval* args);
EJSBool _ejs_decompose_closure (ejsval closure, EJSClosureFunc* func, EJSClosureEnv* env, ejsval *_this);

ejsval _ejs_invoke_closure_0 (ejsval closure, ejsval _this, uint32_t argc);
ejsval _ejs_invoke_closure_1 (ejsval closure, ejsval _this, uint32_t argc, ejsval arg1);
ejsval _ejs_invoke_closure_2 (ejsval closure, ejsval _this, uint32_t argc, ejsval arg1, ejsval arg2);

extern ejsval _ejs_function_new (EJSClosureEnv env, ejsval name, EJSClosureFunc func);
extern ejsval _ejs_function_new_native (EJSClosureEnv env, ejsval name, EJSClosureFunc func);
extern ejsval _ejs_function_new_anon (EJSClosureEnv env, EJSClosureFunc func);
extern ejsval _ejs_function_new_utf8 (EJSClosureEnv env, const char* name, EJSClosureFunc func);

extern ejsval _ejs_Function;
extern ejsval _ejs_Function__proto__;
extern EJSSpecOps _ejs_function_specops;

extern void _ejs_function_init(ejsval global);

// used as the __proto__ for a number of builtin objects
ejsval _ejs_Function_empty (ejsval env, ejsval _this, uint32_t argc, ejsval *args);

EJS_END_DECLS

#endif
