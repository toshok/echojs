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

typedef struct {
    /* object header */
    EJSObject obj;

    /* value specific data */
    llvm::Value *llvm_val;
} EJSLLVMValue;

EJSObject* _ejs_llvm_Value_alloc_instance()
{
    return (EJSObject*)_ejs_gc_new(EJSLLVMValue);
}



ejsval _ejs_llvm_Value_proto;
ejsval _ejs_llvm_Value;
static ejsval
_ejs_llvm_Value_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_llvm_Value_new(llvm::Value* llvm_val)
{
  EJSObject* result = _ejs_llvm_Value_alloc_instance();
  _ejs_init_object (result, _ejs_llvm_Value_proto, NULL);
  ((EJSLLVMValue*)result)->llvm_val = llvm_val;
  return OBJECT_TO_EJSVAL(result);
}

ejsval
_ejs_llvm_Value_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((EJSLLVMValue*)EJSVAL_TO_OBJECT(_this))->llvm_val->print(str_ostream);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_Value_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
    ((EJSLLVMValue*)EJSVAL_TO_OBJECT(_this))->llvm_val->dump();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Value_prototype_setName(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMValue* val = ((EJSLLVMValue*)EJSVAL_TO_OBJECT(_this));
    REQ_UTF8_ARG (0, name);

    val->llvm_val->setName(name);
    free(name);
    return _ejs_undefined;
}

llvm::Value*
_ejs_llvm_Value_GetLLVMObj(ejsval val)
{
    if (EJSVAL_IS_NULL(val)) return NULL;
    return ((EJSLLVMValue*)EJSVAL_TO_OBJECT(val))->llvm_val;
}

void
_ejs_llvm_Value_init (ejsval exports)
{
    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_llvm_Value_proto);
    _ejs_llvm_Value_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_object_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMValue", (EJSClosureFunc)_ejs_llvm_Value_impl));
    _ejs_llvm_Value = tmpobj;


#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_Value_proto, EJS_STRINGIFY(x), _ejs_llvm_Value_prototype_##x)

    _ejs_object_setprop (_ejs_llvm_Value,       _ejs_atom_prototype,  _ejs_llvm_Value_proto);

    PROTO_METHOD(setName);
    PROTO_METHOD(dump);
    PROTO_METHOD(toString);

#undef PROTO_METHOD

    _ejs_object_setprop_utf8 (exports,              "Value", _ejs_llvm_Value);

    END_SHADOW_STACK_FRAME;
}
