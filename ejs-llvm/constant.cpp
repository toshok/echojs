/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-array.h"
#include "ejs-string.h"

#include "constant.h"
#include "type.h"
#include "value.h"

namespace ejsllvm {

    static ejsval _ejs_Constant_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_Constant EJSVAL_ALIGNMENT;
    static ejsval
    Constant_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        EJS_NOT_IMPLEMENTED();
    }

    static ejsval
    Constant_getNull (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_TYPE_ARG (0, ty);
        return Value_new (llvm::Constant::getNullValue(ty));
    }

    static ejsval
    Constant_getAggregateZero (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_TYPE_ARG (0, ty);

        return Value_new (llvm::ConstantAggregateZero::get(ty));
    }

    static ejsval
    Constant_getBoolValue (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_BOOL_ARG (0, b);

        return Value_new (llvm::Constant::getIntegerValue(llvm::Type::getInt8Ty(llvm::getGlobalContext()), llvm::APInt(8, b?1:0)));
    }

    static ejsval
    Constant_getIntegerValue (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_TYPE_ARG (0, ty);
        REQ_INT_ARG (1, v);

        if (argc == 2) {
            return Value_new (llvm::Constant::getIntegerValue(ty, llvm::APInt(ty->getPrimitiveSizeInBits(), v)));
        }
        else if (argc == 3 && EJSVAL_IS_NUMBER(args[2]) && ty->getPrimitiveSizeInBits() == 64) {
            int64_t vhi = v;
            int64_t vlo = (int64_t)EJSVAL_TO_NUMBER(args[2]);
            return Value_new (llvm::Constant::getIntegerValue(ty, llvm::APInt(ty->getPrimitiveSizeInBits(), (int64_t)(vhi << 32 | vlo))));
        }
        else
            abort(); // FIXME throw an exception
    }

    void
    Constant_init (ejsval exports)
    {
        _ejs_gc_add_root (&_ejs_Constant_prototype);
        _ejs_Constant_prototype = _ejs_object_create(_ejs_Object_prototype);

        _ejs_Constant = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMConstant", (EJSClosureFunc)Constant_impl, _ejs_Constant_prototype);

        _ejs_object_setprop_utf8 (exports,              "Constant", _ejs_Constant);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Constant, x, Constant_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Constant_prototype, x, Constant_prototype_##x)

        OBJ_METHOD(getNull);
        OBJ_METHOD(getAggregateZero);
        OBJ_METHOD(getBoolValue);
        OBJ_METHOD(getIntegerValue);

#undef PROTO_METHOD
#undef OBJ_METHOD

    }

};
