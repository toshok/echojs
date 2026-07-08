/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-function.h"
#include "ejs-string.h"

#include "phinode.h"
#include "basicblock.h"
#include "value.h"

namespace ejsllvm {

    /// phi nodes

    typedef struct {
        /* object header */
        EJSObject obj;

        /* phi specific data */
        llvm::PHINode *llvm_phi;
    } PhiNode;

    static EJSSpecOps _ejs_PhiNode_specops;
    static ejsval _ejs_PhiNode_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_PhiNode EJSVAL_ALIGNMENT;

    static EJSObject* PhiNode_allocate()
    {
        return (EJSObject*)_ejs_gc_new(PhiNode);
    }

    static EJS_NATIVE_FUNC(PhiNode_impl) {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    PhiNode_new(llvm::PHINode* llvm_phi)
    {
        ejsval result = _ejs_object_new (_ejs_PhiNode_prototype, &_ejs_PhiNode_specops);
        ((PhiNode*)EJSVAL_TO_OBJECT(result))->llvm_phi = llvm_phi;
        return result;
    }

    static EJS_NATIVE_FUNC(PhiNode_prototype_toString) {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((PhiNode*)EJSVAL_TO_OBJECT(*_this))->llvm_phi->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    static EJS_NATIVE_FUNC(PhiNode_prototype_dump) {
        // ((PhiNode*)EJSVAL_TO_OBJECT(*_this))->llvm_phi->dump();
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(PhiNode_prototype_addIncoming) {
        PhiNode *phi = ((PhiNode*)EJSVAL_TO_OBJECT(*_this));
        REQ_LLVM_VAL_ARG(0, incoming_val);
        REQ_LLVM_BB_ARG(1, incoming_bb);
        phi->llvm_phi->addIncoming(incoming_val, incoming_bb);
        return _ejs_undefined;
    }

    llvm::PHINode*
    PhiNode_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((PhiNode*)EJSVAL_TO_OBJECT(val))->llvm_phi;
    }

    void
    PhiNode_init (ejsval exports)
    {
        _ejs_PhiNode_specops = _ejs_Object_specops;
        _ejs_PhiNode_specops.class_name = "LLVMPhiNode";
        _ejs_PhiNode_specops.Allocate = PhiNode_allocate;

        _ejs_gc_add_root (&_ejs_PhiNode_prototype);
        _ejs_PhiNode_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_PhiNode_specops);

        _ejs_PhiNode = _ejs_function_new_utf8_with_proto  (_ejs_null, "LLVMPhiNode", (EJSClosureFunc)PhiNode_impl, _ejs_PhiNode_prototype);

        _ejs_object_setprop_utf8 (exports,              "PhiNode", _ejs_PhiNode);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_PhiNode_prototype, x, PhiNode_prototype_##x)

        PROTO_METHOD(dump);
        PROTO_METHOD(toString);
        PROTO_METHOD(addIncoming);

#undef PROTO_METHOD
    }
};
