/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-array.h"
#include "constant.h"
#include "type.h"
#include "value.h"

ejsval _ejs_llvm_ConstantFP_proto;
ejsval _ejs_llvm_ConstantFP;
static ejsval
_ejs_llvm_ConstantFP_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_llvm_ConstantFP_getDouble (ejsval env, ejsval _this, int argc, ejsval *args)
{
    REQ_DOUBLE_ARG(0, v);
    return _ejs_llvm_Value_new (llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(v)));
}

void
_ejs_llvm_ConstantFP_init (ejsval exports)
{
    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_llvm_ConstantFP_proto);
    _ejs_llvm_ConstantFP_proto = _ejs_object_create(_ejs_Object_prototype);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMConstantFP", (EJSClosureFunc)_ejs_llvm_ConstantFP_impl));
    _ejs_llvm_ConstantFP = tmpobj;


#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_ConstantFP, EJS_STRINGIFY(x), _ejs_llvm_ConstantFP_##x)
#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_ConstantFP_proto, EJS_STRINGIFY(x), _ejs_llvm_ConstantFP_prototype_##x)

    _ejs_object_setprop (_ejs_llvm_ConstantFP,       _ejs_atom_prototype,  _ejs_llvm_ConstantFP_proto);

    OBJ_METHOD(getDouble);

#undef PROTO_METHOD
#undef OBJ_METHOD

    _ejs_object_setprop_utf8 (exports,              "ConstantFP", _ejs_llvm_ConstantFP);

    END_SHADOW_STACK_FRAME;
}

