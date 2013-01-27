/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include <fstream>

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "ejs-string.h"

#include "value.h"
#include "module.h"
#include "type.h"
#include "globalvariable.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* value specific data */
    llvm::GlobalVariable *llvm_global;
} EJSLLVMGlobalVariable;

EJSObject* _ejs_llvm_GlobalVariable_alloc_instance()
{
    return (EJSObject*)_ejs_gc_new(EJSLLVMGlobalVariable);
}



ejsval _ejs_llvm_GlobalVariable_proto;
ejsval _ejs_llvm_GlobalVariable;
static ejsval
_ejs_llvm_GlobalVariable_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_llvm_GlobalVariable_new(llvm::GlobalVariable* llvm_global)
{
  EJSObject* result = _ejs_llvm_GlobalVariable_alloc_instance();
  _ejs_init_object (result, _ejs_llvm_GlobalVariable_proto, NULL);
  ((EJSLLVMGlobalVariable*)result)->llvm_global = llvm_global;
  return OBJECT_TO_EJSVAL(result);
}

ejsval
_ejs_llvm_GlobalVariable_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((EJSLLVMGlobalVariable*)EJSVAL_TO_OBJECT(_this))->llvm_global->print(str_ostream);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_GlobalVariable_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
    ((EJSLLVMGlobalVariable*)EJSVAL_TO_OBJECT(_this))->llvm_global->dump();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_GlobalVariable_prototype_setInitializer(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMGlobalVariable* global = (EJSLLVMGlobalVariable*)EJSVAL_TO_OBJECT(_this);

    REQ_LLVM_CONST_ARG (0, init);

    global->llvm_global->setInitializer (init);

    return _ejs_undefined;
}

llvm::GlobalVariable*
_ejs_llvm_GlobalVariable_GetLLVMObj(ejsval val)
{
    if (EJSVAL_IS_NULL(val)) return NULL;
    return ((EJSLLVMGlobalVariable*)EJSVAL_TO_OBJECT(val))->llvm_global;
}

void
_ejs_llvm_GlobalVariable_init (ejsval exports)
{
    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_llvm_GlobalVariable_proto);
    _ejs_llvm_GlobalVariable_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_object_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMGlobalVariable", (EJSClosureFunc)_ejs_llvm_GlobalVariable_impl));
    _ejs_llvm_GlobalVariable = tmpobj;


#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_GlobalVariable_proto, EJS_STRINGIFY(x), _ejs_llvm_GlobalVariable_prototype_##x)

    _ejs_object_setprop (_ejs_llvm_GlobalVariable,       _ejs_atom_prototype,  _ejs_llvm_GlobalVariable_proto);

    PROTO_METHOD(setInitializer);
    PROTO_METHOD(dump);
    PROTO_METHOD(toString);

#undef PROTO_METHOD

    _ejs_object_setprop_utf8 (exports,              "GlobalVariable", _ejs_llvm_GlobalVariable);

    END_SHADOW_STACK_FRAME;
}
