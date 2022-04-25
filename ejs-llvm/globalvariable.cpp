/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include <fstream>

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-ops.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-proxy.h"
#include "ejs-symbol.h"
#include "ejs-error.h"

#include "value.h"
#include "module.h"
#include "type.h"
#include "globalvariable.h"

namespace ejsllvm {

    typedef struct {
        /* object header */
        EJSObject obj;

        /* value specific data */
        llvm::GlobalVariable *llvm_global;
    } GlobalVariable;

    static EJSSpecOps _ejs_GlobalVariable_specops;
    static ejsval _ejs_GlobalVariable_prototype;
    static ejsval _ejs_GlobalVariable;

    EJSObject* GlobalVariable_allocate()
    {
        return (EJSObject*)_ejs_gc_new(GlobalVariable);
    }

    static EJS_NATIVE_FUNC(GlobalVariable_impl) {
        if (EJSVAL_IS_UNDEFINED(newTarget)) {
            // called as a function
            EJS_NOT_IMPLEMENTED();
        }
        else {
            ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_GlobalVariable_prototype, &_ejs_GlobalVariable_specops);
            *_this = O;

            GlobalVariable* gv = (GlobalVariable*)EJSVAL_TO_OBJECT(O);

            REQ_LLVM_MODULE_ARG(0, module);
            REQ_LLVM_TYPE_ARG(1, type);
            REQ_UTF8_ARG(2, name);
            REQ_NULLABLE_LLVM_CONST_ARG(3, init);
            REQ_BOOL_ARG(4, visible);

            gv->llvm_global = new ::llvm::GlobalVariable(*module, type, false, visible ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::InternalLinkage, init, name);
            return *_this;
        }
    }

    ejsval
    GlobalVariable_new(llvm::GlobalVariable* llvm_global)
    {
        EJSObject* result = GlobalVariable_allocate();
        _ejs_init_object (result, _ejs_GlobalVariable_prototype, NULL);
        ((GlobalVariable*)result)->llvm_global = llvm_global;
        return OBJECT_TO_EJSVAL(result);
    }

    static EJS_NATIVE_FUNC(GlobalVariable_prototype_toString) {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((GlobalVariable*)EJSVAL_TO_OBJECT(*_this))->llvm_global->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    static EJS_NATIVE_FUNC(GlobalVariable_prototype_dump) {
        // ((GlobalVariable*)EJSVAL_TO_OBJECT(*_this))->llvm_global->dump();
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(GlobalVariable_prototype_setInitializer) {
        GlobalVariable* global = (GlobalVariable*)EJSVAL_TO_OBJECT(*_this);

        REQ_LLVM_CONST_ARG (0, init);

        global->llvm_global->setInitializer (init);

        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(GlobalVariable_prototype_setAlignment) {
        GlobalVariable* global = (GlobalVariable*)EJSVAL_TO_OBJECT(*_this);

        REQ_INT_ARG (0, alignment);

        global->llvm_global->setAlignment (llvm::Align(alignment));

        return _ejs_undefined;
    }

    llvm::GlobalVariable*
    GlobalVariable_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((GlobalVariable*)EJSVAL_TO_OBJECT(val))->llvm_global;
    }

    void
    GlobalVariable_init (ejsval exports)
    {
        _ejs_GlobalVariable_specops = _ejs_Object_specops;
        _ejs_GlobalVariable_specops.class_name = "LLVMGlobalVariable";
        _ejs_GlobalVariable_specops.Allocate = GlobalVariable_allocate;

        _ejs_gc_add_root (&_ejs_GlobalVariable_prototype);
        _ejs_GlobalVariable_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);

        _ejs_GlobalVariable = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMGlobalVariable", (EJSClosureFunc)GlobalVariable_impl, _ejs_GlobalVariable_prototype);

        _ejs_object_setprop_utf8 (exports,              "GlobalVariable", _ejs_GlobalVariable);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_GlobalVariable_prototype, x, GlobalVariable_prototype_##x)

        PROTO_METHOD(setAlignment);
        PROTO_METHOD(setInitializer);
        PROTO_METHOD(dump);
        PROTO_METHOD(toString);

#undef PROTO_METHOD
    }
};
