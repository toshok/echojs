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

typedef struct {
    /* object header */
    EJSObject obj;

    /* type specific data */
    llvm::ArrayType *type;
} EJSLLVMArrayType;

static EJSSpecOps _ejs_llvm_arraytype_specops;

static EJSObject* _ejs_llvm_ArrayType_allocate()
{
    return (EJSObject*)_ejs_gc_new(EJSLLVMArrayType);
}

ejsval _ejs_llvm_ArrayType_proto;
ejsval _ejs_llvm_ArrayType;
static ejsval
_ejs_llvm_ArrayType_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_llvm_ArrayType_new(llvm::ArrayType* llvm_ty)
{
    ejsval result = _ejs_object_new (_ejs_llvm_ArrayType_proto, &_ejs_llvm_arraytype_specops);
    ((EJSLLVMArrayType*)EJSVAL_TO_OBJECT(result))->type = llvm_ty;
    return result;
}

static ejsval
_ejs_llvm_ArrayType_get (ejsval env, ejsval _this, int argc, ejsval *args)
{
    REQ_LLVM_TYPE_ARG (0, elementType);
    REQ_INT_ARG (1, numElements);

    return _ejs_llvm_ArrayType_new (llvm::ArrayType::get(elementType, numElements));
}

ejsval
_ejs_llvm_ArrayType_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((EJSLLVMArrayType*)EJSVAL_TO_OBJECT(_this))->type->print(str_ostream);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_ArrayType_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
    ((EJSLLVMArrayType*)EJSVAL_TO_OBJECT(_this))->type->dump();
    return _ejs_undefined;
}

llvm::ArrayType*
_ejs_llvm_ArrayType_GetLLVMObj(ejsval val)
{
    if (EJSVAL_IS_NULL(val)) return NULL;
    return ((EJSLLVMArrayType*)EJSVAL_TO_OBJECT(val))->type;
}

void
_ejs_llvm_ArrayType_init (ejsval exports)
{
    _ejs_llvm_arraytype_specops = _ejs_object_specops;
    _ejs_llvm_arraytype_specops.class_name = "LLVMArray";
    _ejs_llvm_arraytype_specops.allocate = _ejs_llvm_ArrayType_allocate;

    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_llvm_ArrayType_proto);
    _ejs_llvm_ArrayType_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_llvm_arraytype_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMArrayType", (EJSClosureFunc)_ejs_llvm_ArrayType_impl));
    _ejs_llvm_ArrayType = tmpobj;


#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_ArrayType, EJS_STRINGIFY(x), _ejs_llvm_ArrayType_##x)
#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_ArrayType_proto, EJS_STRINGIFY(x), _ejs_llvm_ArrayType_prototype_##x)

    _ejs_object_setprop (_ejs_llvm_ArrayType,       _ejs_atom_prototype,  _ejs_llvm_ArrayType_proto);

    OBJ_METHOD(get);

    PROTO_METHOD(dump);
    PROTO_METHOD(toString);

#undef PROTO_METHOD
#undef OBJ_METHOD

    _ejs_object_setprop_utf8 (exports,              "ArrayType", _ejs_llvm_ArrayType);

    END_SHADOW_STACK_FRAME;
}

