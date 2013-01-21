/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "function.h"
#include "functiontype.h"
#include "type.h"
#include "value.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* type specific data */
  llvm::Function *llvm_fun;
} EJSLLVMFunction;

static EJSSpecOps _ejs_llvm_function_specops;

static EJSObject* _ejs_llvm_Function_allocate()
{
    return (EJSObject*)_ejs_gc_new(EJSLLVMFunction);
}



ejsval _ejs_llvm_Function_proto;
ejsval _ejs_llvm_Function;
static ejsval
_ejs_llvm_Function_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_llvm_Function_new(llvm::Function* llvm_fun)
{
    ejsval result = _ejs_object_new (_ejs_llvm_Function_proto, &_ejs_llvm_function_specops);
    ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(result))->llvm_fun = llvm_fun;
    return result;
}

ejsval
_ejs_llvm_Function_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this))->llvm_fun->print(str_ostream);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_Function_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
    ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this))->llvm_fun->dump();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Function_prototype_setOnlyReadsMemory(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMFunction* fun = ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this));
    fun->llvm_fun->setOnlyReadsMemory();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Function_prototype_setDoesNotAccessMemory(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMFunction* fun = ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this));
    fun->llvm_fun->setDoesNotAccessMemory();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Function_prototype_setDoesNotThrow(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMFunction* fun = ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this));
    fun->llvm_fun->setDoesNotThrow();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Function_prototype_setGC(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMFunction* fun = ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this));
    REQ_UTF8_ARG(0, name);
    fun->llvm_fun->setGC(name);
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Function_prototype_getArgs(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMFunction* fun = ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this));
    int size = fun->llvm_fun->arg_size();
    ejsval result = _ejs_array_new(0);

    unsigned Idx = 0;
    for (llvm::Function::arg_iterator AI = fun->llvm_fun->arg_begin(); Idx != size;
         ++AI, ++Idx) {
        ejsval val = _ejs_llvm_Value_new(AI);
        _ejs_array_push_dense (result, 1, &val);
    }
    return result;
}

ejsval
_ejs_llvm_Function_prototype_getArgSize(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMFunction* fun = ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this));
    return NUMBER_TO_EJSVAL (fun->llvm_fun->arg_size());
}

ejsval
_ejs_llvm_Function_prototype_getReturnType(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMFunction* fun = ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this));
    return _ejs_llvm_Type_new (fun->llvm_fun->getReturnType());
}

ejsval
_ejs_llvm_Function_prototype_getType(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMFunction* fun = ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this));
    return _ejs_llvm_FunctionType_new (fun->llvm_fun->getFunctionType());
}

ejsval
_ejs_llvm_Function_prototype_getDoesNotThrow(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMFunction* fun = ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this));
    return BOOLEAN_TO_EJSVAL(fun->llvm_fun->doesNotThrow());
}

ejsval
_ejs_llvm_Function_prototype_getDoesNotAccessMemory(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMFunction* fun = ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this));
    return BOOLEAN_TO_EJSVAL(fun->llvm_fun->doesNotAccessMemory());
}

ejsval
_ejs_llvm_Function_prototype_getOnlyReadsMemory(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMFunction* fun = ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(_this));
    return BOOLEAN_TO_EJSVAL(fun->llvm_fun->onlyReadsMemory());
}

llvm::Function*
_ejs_llvm_Function_GetLLVMObj(ejsval val)
{
    if (EJSVAL_IS_NULL(val)) return NULL;
    return ((EJSLLVMFunction*)EJSVAL_TO_OBJECT(val))->llvm_fun;
}

void
_ejs_llvm_Function_init (ejsval exports)
{
    _ejs_llvm_function_specops = _ejs_object_specops;
    _ejs_llvm_function_specops.class_name = "LLVMFunction";
    _ejs_llvm_function_specops.allocate = _ejs_llvm_Function_allocate;

    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_llvm_Function_proto);
    _ejs_llvm_Function_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_llvm_function_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMFunction", (EJSClosureFunc)_ejs_llvm_Function_impl));
    _ejs_llvm_Function = tmpobj;


#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_Function_proto, EJS_STRINGIFY(x), _ejs_llvm_Function_prototype_##x)
#define PROTO_ACCESSOR(x, y) EJS_INSTALL_GETTER(_ejs_llvm_Function_proto, x, _ejs_llvm_Function_prototype_##y)

    _ejs_object_setprop (_ejs_llvm_Function,       _ejs_atom_prototype,  _ejs_llvm_Function_proto);

    PROTO_ACCESSOR("args", getArgs);
    PROTO_ACCESSOR("argSize", getArgSize);
    PROTO_ACCESSOR("returnType", getReturnType);
    PROTO_ACCESSOR("type", getType);
    PROTO_ACCESSOR("doesNotThrow", getDoesNotThrow);
    PROTO_ACCESSOR("onlyReadsMemory", getOnlyReadsMemory);
    PROTO_ACCESSOR("doesNotAccessMemory", getDoesNotAccessMemory);

    PROTO_METHOD(dump);
    PROTO_METHOD(setOnlyReadsMemory);
    PROTO_METHOD(setDoesNotAccessMemory);
    PROTO_METHOD(setDoesNotThrow);
    PROTO_METHOD(setGC);
    PROTO_METHOD(toString);

#undef PROTO_METHOD
#undef PROTO_ACCESSOR

    _ejs_object_setprop_utf8 (exports,              "Function", _ejs_llvm_Function);

    END_SHADOW_STACK_FRAME;
}
