#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "type.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* type specific data */
  llvm::Type *type;
} EJSLLVMType;


EJSObject* _ejs_llvm_Type_alloc_instance()
{
  return (EJSObject*)calloc(1, sizeof (EJSLLVMType));
}

static EJSValue* _ejs_llvm_Type_proto;
EJSValue*
_ejs_llvm_Type_get_prototype()
{
  return _ejs_llvm_Type_proto;
}

EJSValue* _ejs_llvm_Type;
static EJSValue*
_ejs_llvm_Type_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
}

#define LLVM_TYPE_METHOD_PROXY(name) LLVM_TYPE_METHOD(name,name)
#define LLVM_TYPE_METHOD(name,llvm_ty)					\
  EJSValue*								\
  _ejs_llvm_Type_##name(EJSValue* env, EJSValue* _this, int argc, EJSValue **args) \
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

EJSValue*
_ejs_llvm_Type_prototype_pointerTo(EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  return _ejs_llvm_Type_new(((EJSLLVMType*)_this)->type->getPointerTo());
}

EJSValue*
_ejs_llvm_Type_prototype_isVoid(EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  return ((EJSLLVMType*)_this)->type->isVoidTy() ? _ejs_true : _ejs_false;
}

EJSValue*
_ejs_llvm_Type_prototype_toString(EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  std::string str;
  llvm::raw_string_ostream str_ostream(str);
  ((EJSLLVMType*)_this)->type->print(str_ostream);

  return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

EJSValue*
_ejs_llvm_Type_prototype_dump(EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  ((EJSLLVMType*)_this)->type->dump();
  return _ejs_undefined;
}

EJSValue*
_ejs_llvm_Type_new(llvm::Type* llvm_ty)
{
  EJSObject* result = _ejs_llvm_Type_alloc_instance();
  _ejs_init_object (result, _ejs_llvm_Type_proto);
  ((EJSLLVMType*)result)->type = llvm_ty;
  return (EJSValue*)result;
}

llvm::Type*
_ejs_llvm_Type_getLLVMObj(EJSValue* val)
{
  return ((EJSLLVMType*)val)->type;
}

void
_ejs_llvm_Type_init (EJSValue* exports)
{
  _ejs_llvm_Type = _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_Type_impl);
  _ejs_llvm_Type_proto = _ejs_object_new(NULL);

  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "prototype",  _ejs_llvm_Type_proto);
  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "getDoubleTy",  _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_Type_getDoubleTy));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "getInt64Ty",  _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_Type_getInt64Ty));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "getInt32Ty",  _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_Type_getInt32Ty));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "getInt8Ty",  _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_Type_getInt8Ty));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type,       "getVoidTy",  _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_Type_getVoidTy));

  _ejs_object_setprop_utf8 (_ejs_llvm_Type_proto, "pointerTo",   _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_Type_prototype_pointerTo));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type_proto, "isVoid",   _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_Type_prototype_isVoid));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type_proto, "dump",   _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_Type_prototype_dump));
  _ejs_object_setprop_utf8 (_ejs_llvm_Type_proto, "toString",   _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_Type_prototype_toString));

  _ejs_object_setprop_utf8 (exports,              "Type", _ejs_llvm_Type);
}
