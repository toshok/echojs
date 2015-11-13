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

namespace ejsllvm {
    /// call instructions

    typedef struct {
        /* object header */
        EJSObject obj;

        /* call specific data */
        llvm::CallInst *llvm_call;
    } Call;

    static EJSSpecOps _ejs_Call_specops;
    static ejsval _ejs_Call_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_Call EJSVAL_ALIGNMENT;

    static EJSObject* Call_allocate()
    {
        return (EJSObject*)_ejs_gc_new(Call);
    }


    static EJS_NATIVE_FUNC(Call_impl) {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    Call_new(llvm::CallInst* llvm_call)
    {
        ejsval result = _ejs_object_new (_ejs_Call_prototype, &_ejs_Call_specops);
        ((Call*)EJSVAL_TO_OBJECT(result))->llvm_call = llvm_call;
        return result;
    }

    static EJS_NATIVE_FUNC(Call_prototype_toString) {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((Call*)EJSVAL_TO_OBJECT(*_this))->llvm_call->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    static EJS_NATIVE_FUNC(Call_prototype_dump) {
        ((Call*)EJSVAL_TO_OBJECT(*_this))->llvm_call->dump();
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(Call_prototype_setOnlyReadsMemory) {
        Call* call = ((Call*)EJSVAL_TO_OBJECT(*_this));
        call->llvm_call->setOnlyReadsMemory();
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(Call_prototype_setDoesNotAccessMemory) {
        Call* call = ((Call*)EJSVAL_TO_OBJECT(*_this));
        call->llvm_call->setDoesNotAccessMemory();
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(Call_prototype_setDoesNotThrow) {
        Call* call = ((Call*)EJSVAL_TO_OBJECT(*_this));
        call->llvm_call->setDoesNotThrow();
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(Call_prototype_setStructRet) {
        Call* call = ((Call*)EJSVAL_TO_OBJECT(*_this));
        call->llvm_call->addAttribute(1 /* first arg */,
                                      llvm::Attribute::StructRet);
        return _ejs_undefined;
    }

    llvm::CallInst*
    Call_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((Call*)EJSVAL_TO_OBJECT(val))->llvm_call;
    }

    void
    Call_init (ejsval exports)
    {
        _ejs_Call_specops = _ejs_Object_specops;
        _ejs_Call_specops.class_name = "LLVMCall";
        _ejs_Call_specops.Allocate = Call_allocate;

        _ejs_gc_add_root (&_ejs_Call_prototype);
        _ejs_Call_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_Call_specops);

        ejsval tmpobj = _ejs_function_new_utf8 (_ejs_null, "LLVMCall", (EJSClosureFunc)Call_impl);
        _ejs_Call = tmpobj;


#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Call_prototype, x, Call_prototype_##x)

        _ejs_object_setprop (_ejs_Call,       _ejs_atom_prototype,  _ejs_Call_prototype);

        PROTO_METHOD(setOnlyReadsMemory);
        PROTO_METHOD(setDoesNotAccessMemory);
        PROTO_METHOD(setDoesNotThrow);
        PROTO_METHOD(setStructRet);
        PROTO_METHOD(dump);
        PROTO_METHOD(toString);

#undef PROTO_METHOD

        _ejs_object_setprop_utf8 (exports,              "Call", _ejs_Call);
    }

    /// invoke instructions

    typedef struct {
        /* object header */
        EJSObject obj;

        /* invoke specific data */
        llvm::InvokeInst *llvm_invoke;
    } Invoke;

    static EJSSpecOps _ejs_Invoke_specops;
    static ejsval _ejs_Invoke_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_Invoke EJSVAL_ALIGNMENT;

    static EJSObject* Invoke_allocate()
    {
        return (EJSObject*)_ejs_gc_new(Invoke);
    }


    static EJS_NATIVE_FUNC(Invoke_impl) {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    Invoke_new(llvm::InvokeInst* llvm_invoke)
    {
        ejsval result = _ejs_object_new (_ejs_Invoke_prototype, &_ejs_Invoke_specops);
        ((Invoke*)EJSVAL_TO_OBJECT(result))->llvm_invoke = llvm_invoke;
        return result;
    }

    static EJS_NATIVE_FUNC(Invoke_prototype_toString) {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((Invoke*)EJSVAL_TO_OBJECT(*_this))->llvm_invoke->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    static EJS_NATIVE_FUNC(Invoke_prototype_dump) {
        ((Invoke*)EJSVAL_TO_OBJECT(*_this))->llvm_invoke->dump();
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(Invoke_prototype_setOnlyReadsMemory) {
        Invoke* invoke = ((Invoke*)EJSVAL_TO_OBJECT(*_this));
        invoke->llvm_invoke->setOnlyReadsMemory();
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(Invoke_prototype_setDoesNotAccessMemory) {
        Invoke* invoke = ((Invoke*)EJSVAL_TO_OBJECT(*_this));
        invoke->llvm_invoke->setDoesNotAccessMemory();
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(Invoke_prototype_setDoesNotThrow) {
        Invoke* invoke = ((Invoke*)EJSVAL_TO_OBJECT(*_this));
        invoke->llvm_invoke->setDoesNotThrow();
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(Invoke_prototype_setStructRet) {
        Invoke* invoke = ((Invoke*)EJSVAL_TO_OBJECT(*_this));
        invoke->llvm_invoke->addAttribute(1 /* first arg */,
                                          llvm::Attribute::StructRet);
        return _ejs_undefined;
    }

    llvm::InvokeInst*
    Invoke_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((Invoke*)EJSVAL_TO_OBJECT(val))->llvm_invoke;
    }

    void
    Invoke_init (ejsval exports)
    {
        _ejs_Invoke_specops = _ejs_Object_specops;
        _ejs_Invoke_specops.class_name = "LLVMInvoke";
        _ejs_Invoke_specops.Allocate = Invoke_allocate;

        _ejs_gc_add_root (&_ejs_Invoke_prototype);
        _ejs_Invoke_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_Invoke_specops);

        _ejs_Invoke = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMInvoke", (EJSClosureFunc)Invoke_impl, _ejs_Invoke_prototype);

        _ejs_object_setprop_utf8 (exports,              "Invoke", _ejs_Invoke);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Invoke_prototype, x, Invoke_prototype_##x)

        PROTO_METHOD(setOnlyReadsMemory);
        PROTO_METHOD(setDoesNotAccessMemory);
        PROTO_METHOD(setDoesNotThrow);
        PROTO_METHOD(setStructRet);
        PROTO_METHOD(dump);
        PROTO_METHOD(toString);

#undef PROTO_METHOD

    }

};
