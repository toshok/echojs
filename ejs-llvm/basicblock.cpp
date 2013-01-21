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
  llvm::BasicBlock *llvm_bb;
} EJSLLVMBasicBlock;

static EJSSpecOps _ejs_llvm_basicblock_specops;

static EJSObject* _ejs_llvm_BasicBlock_allocate()
{
    return (EJSObject*)_ejs_gc_new(EJSLLVMBasicBlock);
}



ejsval _ejs_llvm_BasicBlock_proto;
ejsval _ejs_llvm_BasicBlock;
static ejsval
_ejs_llvm_BasicBlock_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        // called as a function
        EJS_NOT_IMPLEMENTED();
    }
    else {
        EJSLLVMBasicBlock* bb = ((EJSLLVMBasicBlock*)EJSVAL_TO_OBJECT(_this));
        REQ_UTF8_ARG(0, name);
        REQ_LLVM_FUN_ARG(1, fun);
        bb->llvm_bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), name, fun);
        return _this;
    }
}

ejsval
_ejs_llvm_BasicBlock_new(llvm::BasicBlock* llvm_bb)
{
    ejsval result = _ejs_object_new (_ejs_llvm_BasicBlock_proto, &_ejs_llvm_basicblock_specops);
    ((EJSLLVMBasicBlock*)EJSVAL_TO_OBJECT(result))->llvm_bb = llvm_bb;
    return result;
}

ejsval
_ejs_llvm_BasicBlock_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((EJSLLVMBasicBlock*)EJSVAL_TO_OBJECT(_this))->llvm_bb->print(str_ostream);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_BasicBlock_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
    ((EJSLLVMBasicBlock*)EJSVAL_TO_OBJECT(_this))->llvm_bb->dump();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_BasicBlock_prototype_getName(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMBasicBlock* bb = ((EJSLLVMBasicBlock*)EJSVAL_TO_OBJECT(_this));
    return _ejs_string_new_utf8(bb->llvm_bb->getName().data());
}

ejsval
_ejs_llvm_BasicBlock_prototype_getParent(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMBasicBlock* bb = ((EJSLLVMBasicBlock*)EJSVAL_TO_OBJECT(_this));
    return _ejs_llvm_Function_new (bb->llvm_bb->getParent());
}

llvm::BasicBlock*
_ejs_llvm_BasicBlock_GetLLVMObj(ejsval val)
{
    if (EJSVAL_IS_NULL(val)) return NULL;
    return ((EJSLLVMBasicBlock*)EJSVAL_TO_OBJECT(val))->llvm_bb;
}

void
_ejs_llvm_BasicBlock_init (ejsval exports)
{
    _ejs_llvm_basicblock_specops = _ejs_object_specops;
    _ejs_llvm_basicblock_specops.class_name = "LLVMBasicBlock";
    _ejs_llvm_basicblock_specops.allocate = _ejs_llvm_BasicBlock_allocate;

    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_llvm_BasicBlock_proto);
    _ejs_llvm_BasicBlock_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_llvm_basicblock_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMBasicBlock", (EJSClosureFunc)_ejs_llvm_BasicBlock_impl));
    _ejs_llvm_BasicBlock = tmpobj;


#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_BasicBlock_proto, EJS_STRINGIFY(x), _ejs_llvm_BasicBlock_prototype_##x)
#define PROTO_ACCESSOR(x, y) EJS_INSTALL_GETTER(_ejs_llvm_BasicBlock_proto, x, _ejs_llvm_BasicBlock_prototype_##y)

    _ejs_object_setprop (_ejs_llvm_BasicBlock,       _ejs_atom_prototype,  _ejs_llvm_BasicBlock_proto);

    PROTO_ACCESSOR("name", getName);
    PROTO_ACCESSOR("parent", getParent);

    PROTO_METHOD(dump);
    PROTO_METHOD(toString);

#undef PROTO_METHOD
#undef PROTO_ACCESSOR

    _ejs_object_setprop_utf8 (exports,              "BasicBlock", _ejs_llvm_BasicBlock);

    END_SHADOW_STACK_FRAME;
}
