/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-function.h"

namespace ejsllvm {
    /// alloca instructions

    typedef struct {
        /* object header */
        EJSObject obj;

        /* alloca specific data */
        llvm::AllocaInst *llvm_alloca;
    } AllocaInst;

    static EJSSpecOps allocainst_specops;

    static EJSObject* AllocaInst_allocate()
    {
        return (EJSObject*)_ejs_gc_new(AllocaInst);
    }


    static ejsval _ejs_AllocaInst_proto;
    static ejsval _ejs_AllocaInst;
    static ejsval
    AllocaInst_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    AllocaInst_new(llvm::AllocaInst* llvm_alloca)
    {
        ejsval result = _ejs_object_new (_ejs_AllocaInst_proto, &allocainst_specops);
        ((AllocaInst*)EJSVAL_TO_OBJECT(result))->llvm_alloca = llvm_alloca;
        return result;
    }

    ejsval
    AllocaInst_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((AllocaInst*)EJSVAL_TO_OBJECT(_this))->llvm_alloca->print(str_ostream, NULL);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    ejsval
    AllocaInst_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ((AllocaInst*)EJSVAL_TO_OBJECT(_this))->llvm_alloca->dump();
        return _ejs_undefined;
    }

    ejsval
    AllocaInst_prototype_setAlignment(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        AllocaInst *allocainst = ((AllocaInst*)EJSVAL_TO_OBJECT(_this));
        REQ_INT_ARG(0, alignment);
	allocainst->llvm_alloca->setAlignment(alignment);
        return _ejs_undefined;
    }

    llvm::AllocaInst*
    AllocaInst_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((AllocaInst*)EJSVAL_TO_OBJECT(val))->llvm_alloca;
    }

    void
    AllocaInst_init (ejsval exports)
    {
        allocainst_specops = _ejs_Object_specops;
        allocainst_specops.class_name = "LLVMAllocaInst";
        allocainst_specops.allocate = AllocaInst_allocate;

        _ejs_gc_add_root (&_ejs_AllocaInst_proto);
        _ejs_AllocaInst_proto = _ejs_object_new(_ejs_Object_prototype, &allocainst_specops);

        _ejs_AllocaInst = _ejs_function_new_utf8_with_proto  (_ejs_null, "LLVMAllocaInst", (EJSClosureFunc)AllocaInst_impl, _ejs_AllocaInst_proto);

        _ejs_object_setprop_utf8 (exports,              "AllocaInst", _ejs_AllocaInst);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_AllocaInst_proto, x, AllocaInst_prototype_##x)

        PROTO_METHOD(dump);
        PROTO_METHOD(toString);
        PROTO_METHOD(setAlignment);

#undef PROTO_METHOD
    }
}
