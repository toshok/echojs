#include <stdio.h>

#include "type.h"
#include "ejs-object.h"
#include "ejs-function.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* type specific data */
  llvm::Type *type;
} EJSLLVMType;


EJSObject* _ejs_llvm_Type_alloc_instance()
{
  return (EJSObject*)_ejs_gc_new(EJSLLVMType);
}

static ejsval _ejs_llvm_Type_proto;
ejsval
_ejs_llvm_Type_get_prototype()
{
  return _ejs_llvm_Type_proto;
}

ejsval _ejs_llvm_Type;
static ejsval
_ejs_llvm_Type_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
  EJS_NOT_IMPLEMENTED();
}

#define LLVM_TYPE_METHOD_PROXY(name) LLVM_TYPE_METHOD(name,name)
#define LLVM_TYPE_METHOD(name,llvm_ty)					\
  ejsval								\
  _ejs_llvm_Type_##name(ejsval env, ejsval _this, int argc, ejsval *args) \
  {									\
    return _ejs_llvm_Type_new(llvm::Type::llvm_ty(llvm::getGlobalContext())); \
  }

LLVM_TYPE_METHOD_PROXY(getDoubleTy)
LLVM_TYPE_METHOD_PROXY(getInt64Ty)
LLVM_TYPE_METHOD_PROXY(getInt32Ty)
LLVM_TYPE_METHOD_PROXY(getInt8Ty)
LLVM_TYPE_METHOD_PROXY(getVoidTy)

#undef LLVM_TYPE_METHOD_PROXY
#undef LLVM_TYPE_METHOD

ejsval
_ejs_llvm_Type_prototype_pointerTo(ejsval env, ejsval _this, int argc, ejsval *args)
{
  return _ejs_llvm_Type_new(((EJSLLVMType*)EJSVAL_TO_OBJECT(_this))->type->getPointerTo());
}

ejsval
_ejs_llvm_Type_prototype_isVoid(ejsval env, ejsval _this, int argc, ejsval *args)
{
  return ((EJSLLVMType*)EJSVAL_TO_OBJECT(_this))->type->isVoidTy() ? _ejs_true : _ejs_false;
}

ejsval
_ejs_llvm_Type_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
  std::string str;
  llvm::raw_string_ostream str_ostream(str);
  ((EJSLLVMType*)EJSVAL_TO_OBJECT(_this))->type->print(str_ostream);

  return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_Type_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
  ((EJSLLVMType*)EJSVAL_TO_OBJECT(_this))->type->dump();
  return _ejs_undefined;
}

ejsval
_ejs_llvm_Type_new(llvm::Type* llvm_ty)
{
  EJSObject* result = _ejs_llvm_Type_alloc_instance();
  _ejs_init_object (result, _ejs_llvm_Type_proto, NULL);
  ((EJSLLVMType*)result)->type = llvm_ty;
  return OBJECT_TO_EJSVAL(result);
}

llvm::Type*
_ejs_llvm_Type_getLLVMObj(ejsval val)
{
  return ((EJSLLVMType*)EJSVAL_TO_OBJECT(val))->type;
}

void
_ejs_llvm_Type_init (ejsval exports)
{
  _ejs_llvm_Type = _ejs_function_new_utf8 (_ejs_null, "LLVMType", (EJSClosureFunc)_ejs_llvm_Type_impl);
  _ejs_llvm_Type_proto = _ejs_object_create(_ejs_Object_prototype);

  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "prototype",    _ejs_llvm_Type_proto);
  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "getDoubleTy",  _ejs_function_new_utf8 (_ejs_null, "getDoubleTy", (EJSClosureFunc)_ejs_llvm_Type_getDoubleTy));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "getInt64Ty",   _ejs_function_new_utf8 (_ejs_null, "getInt64Ty", (EJSClosureFunc)_ejs_llvm_Type_getInt64Ty));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "getInt32Ty",   _ejs_function_new_utf8 (_ejs_null, "getInt32Ty", (EJSClosureFunc)_ejs_llvm_Type_getInt32Ty));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "getInt8Ty",    _ejs_function_new_utf8 (_ejs_null, "getInt8Ty", (EJSClosureFunc)_ejs_llvm_Type_getInt8Ty));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "getVoidTy",    _ejs_function_new_utf8 (_ejs_null, "getVoidTy", (EJSClosureFunc)_ejs_llvm_Type_getVoidTy));

  _ejs_object_setprop_utf8 (_ejs_llvm_Type_proto, "pointerTo",    _ejs_function_new_utf8 (_ejs_null, "pointerTo", (EJSClosureFunc)_ejs_llvm_Type_prototype_pointerTo));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type_proto, "isVoid",       _ejs_function_new_utf8 (_ejs_null, "isVoid", (EJSClosureFunc)_ejs_llvm_Type_prototype_isVoid));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type_proto, "dump",         _ejs_function_new_utf8 (_ejs_null, "dump", (EJSClosureFunc)_ejs_llvm_Type_prototype_dump));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type_proto, "toString",     _ejs_function_new_utf8 (_ejs_null, "toString", (EJSClosureFunc)_ejs_llvm_Type_prototype_toString));

  _ejs_object_setprop_utf8 (exports,              "Type", _ejs_llvm_Type);
}
