/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */


#include "ejs-llvm.h"
#include "ejs-function.h"
#include "ejs-object.h"
#include "ejs-array.h"
#include "ejs-string.h"

#include "type.h"

namespace ejsllvm {

    typedef struct {
        /* object header */
        EJSObject obj;

        /* type specific data */
        llvm::StructType *type;
    } StructType;

    static ejsval _ejs_StructType_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_StructType EJSVAL_ALIGNMENT;

    EJSObject* StructType_alloc_instance()
    {
        return (EJSObject*)_ejs_gc_new(StructType);
    }

    static EJS_NATIVE_FUNC(StructType_impl) {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    StructType_new(llvm::StructType* llvm_ty)
    {
        EJSObject* result = StructType_alloc_instance();
        _ejs_init_object (result, _ejs_StructType_prototype, NULL);
        ((StructType*)result)->type = llvm_ty;
        return OBJECT_TO_EJSVAL(result);
    }

    static EJS_NATIVE_FUNC(StructType_create) {
        REQ_UTF8_ARG (0, name);
        REQ_ARRAY_ARG (1, elementTypes);

        std::vector<llvm::Type*> element_types;
        for (int i = 0; i < EJSARRAY_LEN(elementTypes); i ++) {
            element_types.push_back (Type_GetLLVMObj(EJSDENSEARRAY_ELEMENTS(elementTypes)[i]));
        }

        return StructType_new(llvm::StructType::create(TheContext, element_types, name));
    }

    static EJS_NATIVE_FUNC(StructType_prototype_toString) {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((StructType*)EJSVAL_TO_OBJECT(*_this))->type->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    static EJS_NATIVE_FUNC(StructType_prototype_dump) {
        // ((StructType*)EJSVAL_TO_OBJECT(*_this))->type->dump();
        return _ejs_undefined;
    }


    llvm::StructType*
    StructType_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((StructType*)EJSVAL_TO_OBJECT(val))->type;
    }

    void
    StructType_init (ejsval exports)
    {
        _ejs_StructType_prototype = _ejs_object_create (Type_get_prototype());
        _ejs_StructType = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMStructType", (EJSClosureFunc)StructType_impl, _ejs_StructType_prototype);

        _ejs_object_setprop_utf8 (exports,              "StructType", _ejs_StructType);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_StructType, x, StructType_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_StructType_prototype, x, StructType_prototype_##x)

        OBJ_METHOD(create);

        PROTO_METHOD(dump);
        PROTO_METHOD(toString);

#undef OBJ_METHOD
#undef PROTO_METHOD
    }
};
