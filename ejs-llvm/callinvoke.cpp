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

    static EJSSpecOps call_specops;

    static EJSObject* Call_allocate()
    {
        return (EJSObject*)_ejs_gc_new(Call);
    }


    static ejsval _ejs_Call_proto;
    static ejsval _ejs_Call;
    static ejsval
    Call_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    Call_new(llvm::CallInst* llvm_call)
    {
        ejsval result = _ejs_object_new (_ejs_Call_proto, &call_specops);
        ((Call*)EJSVAL_TO_OBJECT(result))->llvm_call = llvm_call;
        return result;
    }

    ejsval
    Call_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((Call*)EJSVAL_TO_OBJECT(_this))->llvm_call->print(str_ostream, NULL);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    ejsval
    Call_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ((Call*)EJSVAL_TO_OBJECT(_this))->llvm_call->dump();
        return _ejs_undefined;
    }

    ejsval
    Call_prototype_setOnlyReadsMemory(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Call* call = ((Call*)EJSVAL_TO_OBJECT(_this));
        call->llvm_call->setOnlyReadsMemory();
        return _ejs_undefined;
    }

    ejsval
    Call_prototype_setDoesNotAccessMemory(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Call* call = ((Call*)EJSVAL_TO_OBJECT(_this));
        call->llvm_call->setDoesNotAccessMemory();
        return _ejs_undefined;
    }

    ejsval
    Call_prototype_setDoesNotThrow(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Call* call = ((Call*)EJSVAL_TO_OBJECT(_this));
        call->llvm_call->setDoesNotThrow();
        return _ejs_undefined;
    }

    ejsval
    Call_prototype_setStructRet(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Call* call = ((Call*)EJSVAL_TO_OBJECT(_this));
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
        call_specops = _ejs_object_specops;
        call_specops.class_name = "LLVMCall";
        call_specops.allocate = Call_allocate;

        _ejs_gc_add_root (&_ejs_Call_proto);
        _ejs_Call_proto = _ejs_object_new(_ejs_Object_prototype, &call_specops);

        ejsval tmpobj = _ejs_function_new_utf8 (_ejs_null, "LLVMCall", (EJSClosureFunc)Call_impl);
        _ejs_Call = tmpobj;


#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Call_proto, x, Call_prototype_##x)

        _ejs_object_setprop (_ejs_Call,       _ejs_atom_prototype,  _ejs_Call_proto);

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

    static EJSSpecOps invoke_specops;

    static EJSObject* Invoke_allocate()
    {
        return (EJSObject*)_ejs_gc_new(Invoke);
    }


    static ejsval _ejs_Invoke_proto;
    static ejsval _ejs_Invoke;
    static ejsval
    Invoke_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    Invoke_new(llvm::InvokeInst* llvm_invoke)
    {
        ejsval result = _ejs_object_new (_ejs_Invoke_proto, &invoke_specops);
        ((Invoke*)EJSVAL_TO_OBJECT(result))->llvm_invoke = llvm_invoke;
        return result;
    }

    ejsval
    Invoke_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((Invoke*)EJSVAL_TO_OBJECT(_this))->llvm_invoke->print(str_ostream, NULL);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    ejsval
    Invoke_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ((Invoke*)EJSVAL_TO_OBJECT(_this))->llvm_invoke->dump();
        return _ejs_undefined;
    }

    ejsval
    Invoke_prototype_setOnlyReadsMemory(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Invoke* invoke = ((Invoke*)EJSVAL_TO_OBJECT(_this));
        invoke->llvm_invoke->setOnlyReadsMemory();
        return _ejs_undefined;
    }

    ejsval
    Invoke_prototype_setDoesNotAccessMemory(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Invoke* invoke = ((Invoke*)EJSVAL_TO_OBJECT(_this));
        invoke->llvm_invoke->setDoesNotAccessMemory();
        return _ejs_undefined;
    }

    ejsval
    Invoke_prototype_setDoesNotThrow(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Invoke* invoke = ((Invoke*)EJSVAL_TO_OBJECT(_this));
        invoke->llvm_invoke->setDoesNotThrow();
        return _ejs_undefined;
    }

    ejsval
    Invoke_prototype_setStructRet(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Invoke* invoke = ((Invoke*)EJSVAL_TO_OBJECT(_this));
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
        invoke_specops = _ejs_object_specops;
        invoke_specops.class_name = "LLVMInvoke";
        invoke_specops.allocate = Invoke_allocate;

        _ejs_gc_add_root (&_ejs_Invoke_proto);
        _ejs_Invoke_proto = _ejs_object_new(_ejs_Object_prototype, &invoke_specops);

        _ejs_Invoke = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMInvoke", (EJSClosureFunc)Invoke_impl, _ejs_Invoke_proto);

        _ejs_object_setprop_utf8 (exports,              "Invoke", _ejs_Invoke);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Invoke_proto, x, Invoke_prototype_##x)

        PROTO_METHOD(setOnlyReadsMemory);
        PROTO_METHOD(setDoesNotAccessMemory);
        PROTO_METHOD(setDoesNotThrow);
        PROTO_METHOD(setStructRet);
        PROTO_METHOD(dump);
        PROTO_METHOD(toString);

#undef PROTO_METHOD

    }

};
