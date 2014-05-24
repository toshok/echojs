/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset:
 * 4 -*- vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */


#include <stdio.h>

#include "type.h"
#include "ejs-object.h"
#include "ejs-function.h"
#include "ejs-string.h"

namespace ejsllvm {
    typedef struct {
        /* object header */
        EJSObject obj;

        /* type specific data */
        llvm::Type *type;
    } Type;


    static ejsval _ejs_Type_prototype EJSVAL_ALIGNMENT;
    ejsval _ejs_Type EJSVAL_ALIGNMENT;

    EJSObject* Type_alloc_instance()
    {
        return (EJSObject*)_ejs_gc_new(Type);
    }

    ejsval
    Type_get_prototype()
    {
        return _ejs_Type_prototype;
    }

    static ejsval
    Type_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        EJS_NOT_IMPLEMENTED();
    }

#define LLVM_TYPE_METHOD_PROXY(name) LLVM_TYPE_METHOD(name,name)
#define LLVM_TYPE_METHOD(name,llvm_ty)                                  \
    ejsval                                                              \
    Type_##name(ejsval env, ejsval _this, int argc, ejsval *args) \
    {                                                                   \
        return Type_new(llvm::Type::llvm_ty(llvm::getGlobalContext())); \
    }

    LLVM_TYPE_METHOD_PROXY(getDoubleTy)
    LLVM_TYPE_METHOD_PROXY(getInt64Ty)
    LLVM_TYPE_METHOD_PROXY(getInt32Ty)
    LLVM_TYPE_METHOD_PROXY(getInt16Ty)
    LLVM_TYPE_METHOD_PROXY(getInt8Ty)
    LLVM_TYPE_METHOD_PROXY(getVoidTy)

#undef LLVM_TYPE_METHOD_PROXY
#undef LLVM_TYPE_METHOD

    ejsval
    Type_prototype_pointerTo(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        return Type_new(((Type*)EJSVAL_TO_OBJECT(_this))->type->getPointerTo());
    }

    ejsval
    Type_prototype_isVoid(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        return ((Type*)EJSVAL_TO_OBJECT(_this))->type->isVoidTy() ? _ejs_true : _ejs_false;
    }

    ejsval
    Type_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((Type*)EJSVAL_TO_OBJECT(_this))->type->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    ejsval
    Type_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ((Type*)EJSVAL_TO_OBJECT(_this))->type->dump();
        return _ejs_undefined;
    }

    ejsval
    Type_new(llvm::Type* llvm_ty)
    {
        EJSObject* result = Type_alloc_instance();
        _ejs_init_object (result, _ejs_Type_prototype, NULL);
        ((Type*)result)->type = llvm_ty;
        return OBJECT_TO_EJSVAL(result);
    }

    llvm::Type*
    Type_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((Type*)EJSVAL_TO_OBJECT(val))->type;
    }

    void
    Type_init (ejsval exports)
    {
        _ejs_gc_add_root (&_ejs_Type_prototype);
        _ejs_Type_prototype = _ejs_object_create(_ejs_Object_prototype);

        _ejs_Type = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMType", (EJSClosureFunc)Type_impl, _ejs_Type_prototype);

        _ejs_object_setprop_utf8 (exports,              "Type", _ejs_Type);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Type, x, Type_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Type_prototype, x, Type_prototype_##x)

        OBJ_METHOD(getDoubleTy);
        OBJ_METHOD(getInt64Ty);
        OBJ_METHOD(getInt32Ty);
        OBJ_METHOD(getInt16Ty);
        OBJ_METHOD(getInt8Ty);
        OBJ_METHOD(getVoidTy);

        PROTO_METHOD(pointerTo);
        PROTO_METHOD(isVoid);
        PROTO_METHOD(dump);
        PROTO_METHOD(toString);
    }
};
