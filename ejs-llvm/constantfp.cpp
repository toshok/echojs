/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-array.h"
#include "ejs-string.h"

#include "constantfp.h"
#include "type.h"
#include "value.h"

namespace ejsllvm {
    static ejsval _ejs_ConstantFP_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_ConstantFP EJSVAL_ALIGNMENT;

    static EJS_NATIVE_FUNC(ConstantFP_impl) {
        EJS_NOT_IMPLEMENTED();
    }

    static EJS_NATIVE_FUNC(ConstantFP_getDouble) {
        REQ_DOUBLE_ARG(0, v);
        return Value_new (llvm::ConstantFP::get(TheContext, llvm::APFloat(v)));
    }

    void
    ConstantFP_init (ejsval exports)
    {
        _ejs_gc_add_root (&_ejs_ConstantFP_prototype);
        _ejs_ConstantFP_prototype = _ejs_object_create(_ejs_Object_prototype);

        _ejs_ConstantFP = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMConstantFP", (EJSClosureFunc)ConstantFP_impl, _ejs_ConstantFP_prototype);

        _ejs_object_setprop_utf8 (exports,              "ConstantFP", _ejs_ConstantFP);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_ConstantFP, x, ConstantFP_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_ConstantFP_prototype, x, ConstantFP_prototype_##x)

        OBJ_METHOD(getDouble);

#undef PROTO_METHOD
#undef OBJ_METHOD
    }

};
