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


    EJSObject* StructType_alloc_instance()
    {
        return (EJSObject*)_ejs_gc_new(StructType);
    }

    static ejsval _ejs_StructType_proto;
    static ejsval _ejs_StructType;
    static ejsval
    StructType_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    StructType_new(llvm::StructType* llvm_ty)
    {
        EJSObject* result = StructType_alloc_instance();
        _ejs_init_object (result, _ejs_StructType_proto, NULL);
        ((StructType*)result)->type = llvm_ty;
        return OBJECT_TO_EJSVAL(result);
    }

    ejsval
    StructType_create(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_UTF8_ARG (0, name);
        REQ_ARRAY_ARG (1, elementTypes);

        std::vector<llvm::Type*> element_types;
        for (int i = 0; i < EJSARRAY_LEN(elementTypes); i ++) {
            element_types.push_back (Type_GetLLVMObj(EJSARRAY_ELEMENTS(elementTypes)[i]));
        }

        ejsval rv = StructType_new(llvm::StructType::create(llvm::getGlobalContext(), element_types, name));
        free (name);
        return rv;
    }

    ejsval
    StructType_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((StructType*)EJSVAL_TO_OBJECT(_this))->type->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    ejsval
    StructType_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ((StructType*)EJSVAL_TO_OBJECT(_this))->type->dump();
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
        START_SHADOW_STACK_FRAME;

        _ejs_StructType = _ejs_function_new_utf8 (_ejs_null, "LLVMStructType", (EJSClosureFunc)StructType_impl);
        _ejs_StructType_proto = _ejs_object_create (Type_get_prototype());

#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_StructType, EJS_STRINGIFY(x), StructType_##x)
#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_StructType_proto, EJS_STRINGIFY(x), StructType_prototype_##x)

        _ejs_object_setprop (_ejs_StructType,       _ejs_atom_prototype,  _ejs_StructType_proto);

        OBJ_METHOD(create);

        PROTO_METHOD(dump);
        PROTO_METHOD(toString);

#undef OBJ_METHOD
#undef PROTO_METHOD

        _ejs_object_setprop_utf8 (exports,              "StructType", _ejs_StructType);

        END_SHADOW_STACK_FRAME;
    }

};
