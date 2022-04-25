/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-proxy.h"
#include "ejs-error.h"
#include "ejs-symbol.h"
#include "function.h"
#include "functiontype.h"
#include "type.h"
#include "value.h"

namespace ejsllvm {
    typedef struct {
        /* object header */
        EJSObject obj;

        /* type specific data */
        llvm::BasicBlock *llvm_bb;
    } BasicBlock;

    static EJSSpecOps _ejs_BasicBlock_specops;
    static ejsval _ejs_BasicBlock_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_BasicBlock EJSVAL_ALIGNMENT;

    static EJSObject* BasicBlock_allocate()
    {
        return (EJSObject*)_ejs_gc_new(BasicBlock);
    }

    static EJS_NATIVE_FUNC(BasicBlock_impl) {
        if (EJSVAL_IS_UNDEFINED(newTarget)) {
            // called as a function
            EJS_NOT_IMPLEMENTED();
        }
        else {
            ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_BasicBlock_prototype, &_ejs_BasicBlock_specops);
            *_this = O;

            BasicBlock* O_ = (BasicBlock*)EJSVAL_TO_OBJECT(O);

            REQ_UTF8_ARG(0, name);
            REQ_LLVM_FUN_ARG(1, fun);
            O_->llvm_bb = llvm::BasicBlock::Create(TheContext, name, fun);

            return *_this;
        }
    }

    ejsval
    BasicBlock_new(llvm::BasicBlock* llvm_bb)
    {
        ejsval result = _ejs_object_new (_ejs_BasicBlock_prototype, &_ejs_BasicBlock_specops);
        ((BasicBlock*)EJSVAL_TO_OBJECT(result))->llvm_bb = llvm_bb;
        return result;
    }

    static EJS_NATIVE_FUNC(BasicBlock_prototype_toString) {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((BasicBlock*)EJSVAL_TO_OBJECT(*_this))->llvm_bb->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    static EJS_NATIVE_FUNC(BasicBlock_prototype_dump) {
        // ((BasicBlock*)EJSVAL_TO_OBJECT(*_this))->llvm_bb->dump();
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(BasicBlock_prototype_get_name) {
        BasicBlock* bb = ((BasicBlock*)EJSVAL_TO_OBJECT(*_this));
        return _ejs_string_new_utf8(bb->llvm_bb->getName().data());
    }

    static EJS_NATIVE_FUNC(BasicBlock_prototype_get_parent) {
        BasicBlock* bb = ((BasicBlock*)EJSVAL_TO_OBJECT(*_this));
        return Function_new (bb->llvm_bb->getParent());
    }

    llvm::BasicBlock*
    BasicBlock_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((BasicBlock*)EJSVAL_TO_OBJECT(val))->llvm_bb;
    }

    void
    BasicBlock_init (ejsval exports)
    {
        _ejs_BasicBlock_specops = _ejs_Object_specops;
        _ejs_BasicBlock_specops.class_name = "LLVMBasicBlock";
        _ejs_BasicBlock_specops.Allocate = BasicBlock_allocate;

        _ejs_gc_add_root (&_ejs_BasicBlock_prototype);
        _ejs_BasicBlock_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_BasicBlock_specops);

        _ejs_BasicBlock = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMBasicBlock", (EJSClosureFunc)BasicBlock_impl, _ejs_BasicBlock_prototype);

        _ejs_object_setprop_utf8 (exports,              "BasicBlock", _ejs_BasicBlock);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_BasicBlock_prototype, x, BasicBlock_prototype_##x)
#define PROTO_ACCESSOR(x) EJS_INSTALL_ATOM_GETTER(_ejs_BasicBlock_prototype, x, BasicBlock_prototype_get_##x)

        PROTO_ACCESSOR(name);
        PROTO_ACCESSOR(parent);

        PROTO_METHOD(dump);
        PROTO_METHOD(toString);

#undef PROTO_METHOD
#undef PROTO_ACCESSOR
    }
};
