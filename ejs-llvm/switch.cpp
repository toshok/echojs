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

#include "basicblock.h"
#include "function.h"
#include "type.h"
#include "module.h"
#include "value.h"


/// switch instructions

typedef struct {
    /* object header */
    EJSObject obj;

    /* switch specific data */
    llvm::SwitchInst *llvm_switch;
} EJSLLVMSwitch;

static EJSSpecOps _ejs_llvm_switch_specops;

static EJSObject* _ejs_llvm_Switch_allocate()
{
    return (EJSObject*)_ejs_gc_new(EJSLLVMSwitch);
}


ejsval _ejs_llvm_Switch_proto;
ejsval _ejs_llvm_Switch;
static ejsval
_ejs_llvm_Switch_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
  EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_llvm_Switch_new(llvm::SwitchInst* llvm_switch)
{
    ejsval result = _ejs_object_new (_ejs_llvm_Switch_proto, &_ejs_llvm_switch_specops);
    ((EJSLLVMSwitch*)EJSVAL_TO_OBJECT(result))->llvm_switch = llvm_switch;
    return result;
}

ejsval
_ejs_llvm_Switch_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((EJSLLVMSwitch*)EJSVAL_TO_OBJECT(_this))->llvm_switch->print(str_ostream, NULL);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_Switch_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
    ((EJSLLVMSwitch*)EJSVAL_TO_OBJECT(_this))->llvm_switch->dump();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Switch_prototype_addCase(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMSwitch *_switch = ((EJSLLVMSwitch*)EJSVAL_TO_OBJECT(_this));
    REQ_LLVM_VAL_ARG(0, OnVal);
    REQ_LLVM_BB_ARG(1, Dest);
    _switch->llvm_switch->addCase(static_cast<llvm::ConstantInt*>(OnVal), Dest);
    return _ejs_undefined;
}

llvm::SwitchInst*
_ejs_llvm_Switch_GetLLVMObj(ejsval val)
{
    if (EJSVAL_IS_NULL(val)) return NULL;
    return ((EJSLLVMSwitch*)EJSVAL_TO_OBJECT(val))->llvm_switch;
}

void
_ejs_llvm_Switch_init (ejsval exports)
{
    _ejs_llvm_switch_specops = _ejs_object_specops;
    _ejs_llvm_switch_specops.class_name = "LLVMSwitch";
    _ejs_llvm_switch_specops.allocate = _ejs_llvm_Switch_allocate;

    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_llvm_Switch_proto);
    _ejs_llvm_Switch_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_llvm_switch_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMSwitch", (EJSClosureFunc)_ejs_llvm_Switch_impl));
    _ejs_llvm_Switch = tmpobj;


#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_Switch_proto, EJS_STRINGIFY(x), _ejs_llvm_Switch_prototype_##x)

    _ejs_object_setprop (_ejs_llvm_Switch,       _ejs_atom_prototype,  _ejs_llvm_Switch_proto);

    PROTO_METHOD(dump);
    PROTO_METHOD(toString);
    PROTO_METHOD(addCase);

#undef PROTO_METHOD

    _ejs_object_setprop_utf8 (exports,              "Switch", _ejs_llvm_Switch);

    END_SHADOW_STACK_FRAME;
}

