/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include <fstream>

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "ejs-string.h"

#include "function.h"
#include "type.h"
#include "module.h"
#include "value.h"


/// call instructions

typedef struct {
    /* object header */
    EJSObject obj;

    /* call specific data */
    llvm::CallInst *llvm_call;
} EJSLLVMCall;

static EJSSpecOps _ejs_llvm_call_specops;

static EJSObject* _ejs_llvm_Call_allocate()
{
    return (EJSObject*)_ejs_gc_new(EJSLLVMCall);
}


ejsval _ejs_llvm_Call_proto;
ejsval _ejs_llvm_Call;
static ejsval
_ejs_llvm_Call_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
  EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_llvm_Call_new(llvm::CallInst* llvm_call)
{
    ejsval result = _ejs_object_new (_ejs_llvm_Call_proto, &_ejs_llvm_call_specops);
    ((EJSLLVMCall*)EJSVAL_TO_OBJECT(result))->llvm_call = llvm_call;
    return result;
}

ejsval
_ejs_llvm_Call_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((EJSLLVMCall*)EJSVAL_TO_OBJECT(_this))->llvm_call->print(str_ostream, NULL);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_Call_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
    ((EJSLLVMCall*)EJSVAL_TO_OBJECT(_this))->llvm_call->dump();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Call_prototype_setOnlyReadsMemory(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMCall* call = ((EJSLLVMCall*)EJSVAL_TO_OBJECT(_this));
    call->llvm_call->setOnlyReadsMemory();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Call_prototype_setDoesNotAccessMemory(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMCall* call = ((EJSLLVMCall*)EJSVAL_TO_OBJECT(_this));
    call->llvm_call->setDoesNotAccessMemory();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Call_prototype_setDoesNotThrow(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMCall* call = ((EJSLLVMCall*)EJSVAL_TO_OBJECT(_this));
    call->llvm_call->setDoesNotThrow();
    return _ejs_undefined;
}

llvm::CallInst*
_ejs_llvm_Call_GetLLVMObj(ejsval val)
{
    if (EJSVAL_IS_NULL(val)) return NULL;
    return ((EJSLLVMCall*)EJSVAL_TO_OBJECT(val))->llvm_call;
}

void
_ejs_llvm_Call_init (ejsval exports)
{
    _ejs_llvm_call_specops = _ejs_object_specops;
    _ejs_llvm_call_specops.class_name = "LLVMCall";
    _ejs_llvm_call_specops.allocate = _ejs_llvm_Call_allocate;

    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_llvm_Call_proto);
    _ejs_llvm_Call_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_llvm_call_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMCall", (EJSClosureFunc)_ejs_llvm_Call_impl));
    _ejs_llvm_Call = tmpobj;


#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_Call_proto, EJS_STRINGIFY(x), _ejs_llvm_Call_prototype_##x)

    _ejs_object_setprop (_ejs_llvm_Call,       _ejs_atom_prototype,  _ejs_llvm_Call_proto);

    PROTO_METHOD(setOnlyReadsMemory);
    PROTO_METHOD(setDoesNotAccessMemory);
    PROTO_METHOD(setDoesNotThrow);
    PROTO_METHOD(dump);
    PROTO_METHOD(toString);

#undef PROTO_METHOD

    _ejs_object_setprop_utf8 (exports,              "Call", _ejs_llvm_Call);

    END_SHADOW_STACK_FRAME;
}

/// invoke instructions

typedef struct {
    /* object header */
    EJSObject obj;

    /* invoke specific data */
    llvm::InvokeInst *llvm_invoke;
} EJSLLVMInvoke;

static EJSSpecOps _ejs_llvm_invoke_specops;

static EJSObject* _ejs_llvm_Invoke_allocate()
{
    return (EJSObject*)_ejs_gc_new(EJSLLVMInvoke);
}


ejsval _ejs_llvm_Invoke_proto;
ejsval _ejs_llvm_Invoke;
static ejsval
_ejs_llvm_Invoke_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
  EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_llvm_Invoke_new(llvm::InvokeInst* llvm_invoke)
{
    ejsval result = _ejs_object_new (_ejs_llvm_Invoke_proto, &_ejs_llvm_invoke_specops);
    ((EJSLLVMInvoke*)EJSVAL_TO_OBJECT(result))->llvm_invoke = llvm_invoke;
    return result;
}

ejsval
_ejs_llvm_Invoke_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((EJSLLVMInvoke*)EJSVAL_TO_OBJECT(_this))->llvm_invoke->print(str_ostream, NULL);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_Invoke_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
    ((EJSLLVMInvoke*)EJSVAL_TO_OBJECT(_this))->llvm_invoke->dump();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Invoke_prototype_setOnlyReadsMemory(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMInvoke* invoke = ((EJSLLVMInvoke*)EJSVAL_TO_OBJECT(_this));
    invoke->llvm_invoke->setOnlyReadsMemory();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Invoke_prototype_setDoesNotAccessMemory(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMInvoke* invoke = ((EJSLLVMInvoke*)EJSVAL_TO_OBJECT(_this));
    invoke->llvm_invoke->setDoesNotAccessMemory();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Invoke_prototype_setDoesNotThrow(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMInvoke* invoke = ((EJSLLVMInvoke*)EJSVAL_TO_OBJECT(_this));
    invoke->llvm_invoke->setDoesNotThrow();
    return _ejs_undefined;
}

llvm::InvokeInst*
_ejs_llvm_Invoke_GetLLVMObj(ejsval val)
{
    if (EJSVAL_IS_NULL(val)) return NULL;
    return ((EJSLLVMInvoke*)EJSVAL_TO_OBJECT(val))->llvm_invoke;
}

void
_ejs_llvm_Invoke_init (ejsval exports)
{
    _ejs_llvm_invoke_specops = _ejs_object_specops;
    _ejs_llvm_invoke_specops.class_name = "LLVMInvoke";
    _ejs_llvm_invoke_specops.allocate = _ejs_llvm_Invoke_allocate;

    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_llvm_Invoke_proto);
    _ejs_llvm_Invoke_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_llvm_invoke_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMInvoke", (EJSClosureFunc)_ejs_llvm_Invoke_impl));
    _ejs_llvm_Invoke = tmpobj;


#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_Invoke_proto, EJS_STRINGIFY(x), _ejs_llvm_Invoke_prototype_##x)

    _ejs_object_setprop (_ejs_llvm_Invoke,       _ejs_atom_prototype,  _ejs_llvm_Invoke_proto);

    PROTO_METHOD(setOnlyReadsMemory);
    PROTO_METHOD(setDoesNotAccessMemory);
    PROTO_METHOD(setDoesNotThrow);
    PROTO_METHOD(dump);
    PROTO_METHOD(toString);

#undef PROTO_METHOD

    _ejs_object_setprop_utf8 (exports,              "Invoke", _ejs_llvm_Invoke);

    END_SHADOW_STACK_FRAME;
}
