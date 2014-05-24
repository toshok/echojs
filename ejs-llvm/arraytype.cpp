/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-array.h"
#include "ejs-string.h"
#include "arraytype.h"
#include "type.h"

namespace ejsllvm {
    typedef struct {
        /* object header */
        EJSObject obj;

        /* type specific data */
        llvm::ArrayType *type;
    } ArrayType;

    static EJSSpecOps _ejs_ArrayType_specops;
    ejsval _ejs_ArrayType_prototype EJSVAL_ALIGNMENT;
    ejsval _ejs_ArrayType EJSVAL_ALIGNMENT;

    static EJSObject* ArrayType_allocate()
    {
        return (EJSObject*)_ejs_gc_new(ArrayType);
    }

    static ejsval
    ArrayType_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    ArrayType_new(llvm::ArrayType* llvm_ty)
    {
        ejsval result = _ejs_object_new (_ejs_ArrayType_prototype, &_ejs_ArrayType_specops);
        ((ArrayType*)EJSVAL_TO_OBJECT(result))->type = llvm_ty;
        return result;
    }

    static ejsval
    ArrayType_get (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_TYPE_ARG (0, elementType);
        REQ_INT_ARG (1, numElements);

        return ArrayType_new (llvm::ArrayType::get(elementType, numElements));
    }

    ejsval
    ArrayType_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((ArrayType*)EJSVAL_TO_OBJECT(_this))->type->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    ejsval
    ArrayType_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ((ArrayType*)EJSVAL_TO_OBJECT(_this))->type->dump();
        return _ejs_undefined;
    }

    llvm::ArrayType*
    ArrayType_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((ArrayType*)EJSVAL_TO_OBJECT(val))->type;
    }

    void
    ArrayType_init (ejsval exports)
    {
        _ejs_ArrayType_specops = _ejs_Object_specops;
        _ejs_ArrayType_specops.class_name = "LLVMArray";
        _ejs_ArrayType_specops.allocate = ArrayType_allocate;

        _ejs_gc_add_root (&_ejs_ArrayType_prototype);
        _ejs_ArrayType_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_ArrayType_specops);

        _ejs_ArrayType = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMArrayType", (EJSClosureFunc)ArrayType_impl, _ejs_ArrayType_prototype);

        _ejs_object_setprop_utf8 (exports,              "ArrayType", _ejs_ArrayType);


#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_ArrayType, x, ArrayType_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_ArrayType_prototype, x, ArrayType_prototype_##x)

        OBJ_METHOD(get);

        PROTO_METHOD(dump);
        PROTO_METHOD(toString);

#undef PROTO_METHOD
#undef OBJ_METHOD
    }

};
