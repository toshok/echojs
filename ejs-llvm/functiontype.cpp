#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "functiontype.h"
#include "type.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* type specific data */
  llvm::FunctionType *type;
} EJSLLVMFunctionType;


EJSObject* _ejs_llvm_FunctionType_alloc_instance()
{
  return (EJSObject*)calloc(1, sizeof (EJSLLVMFunctionType));
}

EJSValue* _ejs_llvm_FunctionType_proto;
EJSValue* _ejs_llvm_FunctionType;
static EJSValue*
_ejs_llvm_FunctionType_impl (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
}

static EJSValue*
_ejs_llvm_FunctionType_get (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
    REQ_LLVM_TYPE_ARG(0, returnType);
    REQ_ARRAY_ARG(1, argTypes);

    std::vector<llvm::Type*> arg_types;
    for (int i = 0; i < EJS_ARRAY_LEN(argTypes); i ++) {
      arg_types.push_back (_ejs_llvm_Type_getLLVMObj(EJS_ARRAY_ELEMENTS(argTypes)[i]));
    }

    llvm::FunctionType *FT = llvm::FunctionType::get(returnType,
						     arg_types, false);

    EJSObject* result = _ejs_llvm_FunctionType_alloc_instance();
    _ejs_init_object (result, _ejs_llvm_FunctionType_proto);
    ((EJSLLVMFunctionType*)result)->type = FT;
    return (EJSValue*)result;
}

static EJSValue*
_ejs_llvm_FunctionType_prototype_getReturnType (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  return _ejs_llvm_Type_new (((EJSLLVMFunctionType*)_this)->type->getReturnType());
}

static EJSValue*
_ejs_llvm_FunctionType_prototype_getParamType (EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
    REQ_INT_ARG(0, i);

    return _ejs_llvm_Type_new (((EJSLLVMFunctionType*)_this)->type->getParamType(i));
}

EJSValue*
_ejs_llvm_FunctionType_prototype_toString(EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  std::string str;
  llvm::raw_string_ostream str_ostream(str);
  ((EJSLLVMFunctionType*)_this)->type->print(str_ostream);

  return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

EJSValue*
_ejs_llvm_FunctionType_prototype_dump(EJSValue* env, EJSValue* _this, int argc, EJSValue **args)
{
  ((EJSLLVMFunctionType*)_this)->type->dump();
  return _ejs_undefined;
}

void
_ejs_llvm_FunctionType_init (EJSValue* exports)
{
  _ejs_llvm_FunctionType = _ejs_function_new_utf8 (NULL, "LLVMFunction", (EJSClosureFunc)_ejs_llvm_FunctionType_impl);
  _ejs_llvm_FunctionType_proto = _ejs_object_new(_ejs_llvm_Type_get_prototype());

  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType,       "prototype",  _ejs_llvm_FunctionType_proto);
  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType,       "get",        _ejs_function_new_utf8 (NULL, "get", (EJSClosureFunc)_ejs_llvm_FunctionType_get));

  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType_proto, "getReturnType",   _ejs_function_new_utf8 (NULL, "getReturnType", (EJSClosureFunc)_ejs_llvm_FunctionType_prototype_getReturnType));
  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType_proto, "getParamType",   _ejs_function_new_utf8 (NULL, "getParamType", (EJSClosureFunc)_ejs_llvm_FunctionType_prototype_getParamType));

  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType_proto, "dump",   _ejs_function_new_utf8 (NULL, "dump", (EJSClosureFunc)_ejs_llvm_FunctionType_prototype_dump));
  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType_proto, "toString",   _ejs_function_new_utf8 (NULL, "toString", (EJSClosureFunc)_ejs_llvm_FunctionType_prototype_toString));

  _ejs_object_setprop_utf8 (exports,              "FunctionType", _ejs_llvm_FunctionType);
}

