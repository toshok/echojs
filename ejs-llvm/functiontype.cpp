#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-function.h"
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
  return (EJSObject*)_ejs_gc_new(EJSLLVMFunctionType);
}

ejsval _ejs_llvm_FunctionType_proto;
ejsval _ejs_llvm_FunctionType;
static ejsval
_ejs_llvm_FunctionType_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
  EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_llvm_FunctionType_get (ejsval env, ejsval _this, int argc, ejsval *args)
{
    REQ_LLVM_TYPE_ARG(0, returnType);
    REQ_ARRAY_ARG(1, argTypes);

    std::vector<llvm::Type*> arg_types;
    for (int i = 0; i < EJSARRAY_LEN(argTypes); i ++) {
      arg_types.push_back (_ejs_llvm_Type_getLLVMObj(EJSARRAY_ELEMENTS(argTypes)[i]));
    }

    llvm::FunctionType *FT = llvm::FunctionType::get(returnType,
						     arg_types, false);

    EJSObject* result = _ejs_llvm_FunctionType_alloc_instance();
    _ejs_init_object (result, _ejs_llvm_FunctionType_proto, NULL);
    ((EJSLLVMFunctionType*)result)->type = FT;
    return OBJECT_TO_EJSVAL(result);
}

static ejsval
_ejs_llvm_FunctionType_prototype_getReturnType (ejsval env, ejsval _this, int argc, ejsval *args)
{
  return _ejs_llvm_Type_new (((EJSLLVMFunctionType*)EJSVAL_TO_OBJECT(_this))->type->getReturnType());
}

static ejsval
_ejs_llvm_FunctionType_prototype_getParamType (ejsval env, ejsval _this, int argc, ejsval *args)
{
    REQ_INT_ARG(0, i);

    return _ejs_llvm_Type_new (((EJSLLVMFunctionType*)EJSVAL_TO_OBJECT(_this))->type->getParamType(i));
}

ejsval
_ejs_llvm_FunctionType_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
  std::string str;
  llvm::raw_string_ostream str_ostream(str);
  ((EJSLLVMFunctionType*)EJSVAL_TO_OBJECT(_this))->type->print(str_ostream);

  return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_FunctionType_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
  ((EJSLLVMFunctionType*)EJSVAL_TO_OBJECT(_this))->type->dump();
  return _ejs_undefined;
}

void
_ejs_llvm_FunctionType_init (ejsval exports)
{
  _ejs_llvm_FunctionType = _ejs_function_new_utf8 (_ejs_null, "LLVMFunction", (EJSClosureFunc)_ejs_llvm_FunctionType_impl);
  _ejs_llvm_FunctionType_proto = _ejs_object_create (_ejs_llvm_Type_get_prototype());

  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType,       "prototype",  _ejs_llvm_FunctionType_proto);
  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType,       "get",        _ejs_function_new_utf8 (_ejs_null, "get", (EJSClosureFunc)_ejs_llvm_FunctionType_get));

  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType_proto, "getReturnType",   _ejs_function_new_utf8 (_ejs_null, "getReturnType", (EJSClosureFunc)_ejs_llvm_FunctionType_prototype_getReturnType));
  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType_proto, "getParamType",   _ejs_function_new_utf8 (_ejs_null, "getParamType", (EJSClosureFunc)_ejs_llvm_FunctionType_prototype_getParamType));

  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType_proto, "dump",   _ejs_function_new_utf8 (_ejs_null, "dump", (EJSClosureFunc)_ejs_llvm_FunctionType_prototype_dump));
  _ejs_object_setprop_utf8 (_ejs_llvm_FunctionType_proto, "toString",   _ejs_function_new_utf8 (_ejs_null, "toString", (EJSClosureFunc)_ejs_llvm_FunctionType_prototype_toString));

  _ejs_object_setprop_utf8 (exports,              "FunctionType", _ejs_llvm_FunctionType);
}

