#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "type.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* type specific data */
  llvm::StructType *type;
} EJSLLVMStructType;


EJSObject* _ejs_llvm_StructType_alloc_instance()
{
  return (EJSObject*)calloc(1, sizeof (EJSLLVMStructType));
}

EJSValue* _ejs_llvm_StructType_proto;
EJSValue* _ejs_llvm_StructType;
static EJSValue*
_ejs_llvm_StructType_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
}

EJSValue*
_ejs_llvm_StructType_create(EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
    REQ_UTF8_ARG (0, name);
    REQ_ARRAY_ARG (1, elementTypes)

    std::vector<llvm::Type*> element_types;
    for (int i = 0; i < EJS_ARRAY_LEN(elementTypes); i ++) {
      element_types.push_back (_ejs_llvm_Type_getLLVMObj(EJS_ARRAY_ELEMENTS(elementTypes)[i]));
    }

    EJSObject* result = _ejs_llvm_StructType_alloc_instance();
    _ejs_init_object (result, _ejs_llvm_StructType_proto);
    ((EJSLLVMStructType*)result)->type = llvm::StructType::create(llvm::getGlobalContext(), element_types, name);
    return (EJSValue*)result;
}

EJSValue*
_ejs_llvm_StructType_prototype_toString(EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  std::string str;
  llvm::raw_string_ostream str_ostream(str);
  ((EJSLLVMStructType*)_this)->type->print(str_ostream);

  return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

EJSValue*
_ejs_llvm_StructType_prototype_dump(EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  ((EJSLLVMStructType*)_this)->type->dump();
  return _ejs_undefined;
}


void
_ejs_llvm_StructType_init (EJSValue* exports)
{
  _ejs_llvm_StructType = _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_StructType_impl);
  _ejs_llvm_StructType_proto = _ejs_object_new(_ejs_llvm_Type_get_prototype());

  _ejs_object_setprop_utf8 (_ejs_llvm_StructType,       "prototype",  _ejs_llvm_StructType_proto);

  _ejs_object_setprop_utf8 (_ejs_llvm_StructType,       "create",  _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_StructType_create));

  _ejs_object_setprop_utf8 (_ejs_llvm_StructType_proto, "dump",   _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_StructType_prototype_dump));
  _ejs_object_setprop_utf8 (_ejs_llvm_StructType_proto, "toString",   _ejs_function_new (NULL, (EJSClosureFunc)_ejs_llvm_StructType_prototype_toString));

  _ejs_object_setprop_utf8 (exports,              "StructType", _ejs_llvm_StructType);
}
