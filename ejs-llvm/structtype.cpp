#include "ejs-llvm.h"
#include "ejs-function.h"
#include "ejs-object.h"
#include "ejs-array.h"
#include "type.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* type specific data */
  llvm::StructType *type;
} EJSLLVMStructType;


EJSObject* _ejs_llvm_StructType_alloc_instance()
{
  return (EJSObject*)_ejs_gc_new(EJSLLVMStructType);
}

ejsval _ejs_llvm_StructType_proto;
ejsval _ejs_llvm_StructType;
static ejsval
_ejs_llvm_StructType_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
  EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_llvm_StructType_new(llvm::StructType* llvm_ty)
{
  EJSObject* result = _ejs_llvm_StructType_alloc_instance();
  _ejs_init_object (result, _ejs_llvm_StructType_proto, NULL);
  ((EJSLLVMStructType*)result)->type = llvm_ty;
  return OBJECT_TO_EJSVAL(result);
}

ejsval
_ejs_llvm_StructType_create(ejsval env, ejsval _this, int argc, ejsval *args)
{
  REQ_UTF8_ARG (0, name);
  REQ_ARRAY_ARG (1, elementTypes)

    std::vector<llvm::Type*> element_types;
  for (int i = 0; i < EJSARRAY_LEN(elementTypes); i ++) {
    element_types.push_back (_ejs_llvm_Type_GetLLVMObj(EJSARRAY_ELEMENTS(elementTypes)[i]));
  }

  return _ejs_llvm_StructType_new(llvm::StructType::create(llvm::getGlobalContext(), element_types, name));
}

ejsval
_ejs_llvm_StructType_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
  std::string str;
  llvm::raw_string_ostream str_ostream(str);
  ((EJSLLVMStructType*)EJSVAL_TO_OBJECT(_this))->type->print(str_ostream);

  return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_StructType_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
  ((EJSLLVMStructType*)EJSVAL_TO_OBJECT(_this))->type->dump();
  return _ejs_undefined;
}


llvm::StructType*
_ejs_llvm_StructType_GetLLVMObj(ejsval val)
{
  if (EJSVAL_IS_NULL(val)) return NULL;
  return ((EJSLLVMStructType*)EJSVAL_TO_OBJECT(val))->type;
}

void
_ejs_llvm_StructType_init (ejsval exports)
{
  _ejs_llvm_StructType = _ejs_function_new_utf8 (_ejs_null, "LLVMStructType", (EJSClosureFunc)_ejs_llvm_StructType_impl);
  _ejs_llvm_StructType_proto = _ejs_object_create (_ejs_llvm_Type_get_prototype());

  _ejs_object_setprop_utf8 (_ejs_llvm_StructType,       "prototype",  _ejs_llvm_StructType_proto);

  _ejs_object_setprop_utf8 (_ejs_llvm_StructType,       "create",  _ejs_function_new_utf8 (_ejs_null, "create", (EJSClosureFunc)_ejs_llvm_StructType_create));

  _ejs_object_setprop_utf8 (_ejs_llvm_StructType_proto, "dump",   _ejs_function_new_utf8 (_ejs_null, "dump", (EJSClosureFunc)_ejs_llvm_StructType_prototype_dump));
  _ejs_object_setprop_utf8 (_ejs_llvm_StructType_proto, "toString",   _ejs_function_new_utf8 (_ejs_null, "toString", (EJSClosureFunc)_ejs_llvm_StructType_prototype_toString));

  _ejs_object_setprop_utf8 (exports,              "StructType", _ejs_llvm_StructType);
}
