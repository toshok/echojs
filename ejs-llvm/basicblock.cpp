/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-llvm.h"
#include "ejs-object.h"
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

    static ejsval
    BasicBlock_create (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ejsval F = _this;
        if (!EJSVAL_IS_CONSTRUCTOR(F)) 
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in BasicBlock[Symbol.create] is not a constructor");
        EJSObject* F_ = EJSVAL_TO_OBJECT(F);
        // 2. Let obj be the result of calling OrdinaryCreateFromConstructor(F, "%DatePrototype%", ([[DateData]]) ). 
        ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
        if (EJSVAL_IS_UNDEFINED(proto))
            proto = _ejs_BasicBlock_prototype;

        EJSObject* obj = (EJSObject*)_ejs_gc_new (BasicBlock);
        _ejs_init_object (obj, proto, &_ejs_BasicBlock_specops);
        return OBJECT_TO_EJSVAL(obj);
    }

    static ejsval
    BasicBlock_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        if (EJSVAL_IS_UNDEFINED(_this)) {
            // called as a function
            EJS_NOT_IMPLEMENTED();
        }
        else {
            BasicBlock* bb = ((BasicBlock*)EJSVAL_TO_OBJECT(_this));
            REQ_UTF8_ARG(0, name);
            REQ_LLVM_FUN_ARG(1, fun);
            bb->llvm_bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), name, fun);
            free (name);
            return _this;
        }
    }

    ejsval
    BasicBlock_new(llvm::BasicBlock* llvm_bb)
    {
        ejsval result = _ejs_object_new (_ejs_BasicBlock_prototype, &_ejs_BasicBlock_specops);
        ((BasicBlock*)EJSVAL_TO_OBJECT(result))->llvm_bb = llvm_bb;
        return result;
    }

    ejsval
    BasicBlock_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((BasicBlock*)EJSVAL_TO_OBJECT(_this))->llvm_bb->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    ejsval
    BasicBlock_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ((BasicBlock*)EJSVAL_TO_OBJECT(_this))->llvm_bb->dump();
        return _ejs_undefined;
    }

    ejsval
    BasicBlock_prototype_get_name(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        BasicBlock* bb = ((BasicBlock*)EJSVAL_TO_OBJECT(_this));
        return _ejs_string_new_utf8(bb->llvm_bb->getName().data());
    }

    ejsval
    BasicBlock_prototype_get_parent(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        BasicBlock* bb = ((BasicBlock*)EJSVAL_TO_OBJECT(_this));
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

        EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_BasicBlock, create, BasicBlock_create, EJS_PROP_NOT_ENUMERABLE);

    }
};
