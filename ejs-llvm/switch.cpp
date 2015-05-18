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

namespace ejsllvm {

    /// switch instructions

    typedef struct {
        /* object header */
        EJSObject obj;

        /* switch specific data */
        llvm::SwitchInst *llvm_switch;
    } Switch;

    static EJSSpecOps _ejs_Switch_specops;
    static ejsval _ejs_Switch_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_Switch EJSVAL_ALIGNMENT;

    static EJSObject* Switch_allocate()
    {
        return (EJSObject*)_ejs_gc_new(Switch);
    }


    static ejsval
    Switch_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    Switch_new(llvm::SwitchInst* llvm_switch)
    {
        ejsval result = _ejs_object_new (_ejs_Switch_prototype, &_ejs_Switch_specops);
        ((Switch*)EJSVAL_TO_OBJECT(result))->llvm_switch = llvm_switch;
        return result;
    }

    ejsval
    Switch_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((Switch*)EJSVAL_TO_OBJECT(_this))->llvm_switch->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    ejsval
    Switch_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ((Switch*)EJSVAL_TO_OBJECT(_this))->llvm_switch->dump();
        return _ejs_undefined;
    }

    ejsval
    Switch_prototype_addCase(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Switch *_switch = ((Switch*)EJSVAL_TO_OBJECT(_this));
        REQ_LLVM_VAL_ARG(0, OnVal);
        REQ_LLVM_BB_ARG(1, Dest);
        _switch->llvm_switch->addCase(static_cast<llvm::ConstantInt*>(OnVal), Dest);
        return _ejs_undefined;
    }

    llvm::SwitchInst*
    Switch_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((Switch*)EJSVAL_TO_OBJECT(val))->llvm_switch;
    }

    void
    Switch_init (ejsval exports)
    {
        _ejs_Switch_specops = _ejs_Object_specops;
        _ejs_Switch_specops.class_name = "LLVMSwitch";
        _ejs_Switch_specops.Allocate = Switch_allocate;

        _ejs_gc_add_root (&_ejs_Switch_prototype);
        _ejs_Switch_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_Switch_specops);

        _ejs_Switch = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMSwitch", (EJSClosureFunc)Switch_impl, _ejs_Switch_prototype);
        _ejs_object_setprop_utf8 (exports,              "Switch", _ejs_Switch);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Switch_prototype, x, Switch_prototype_##x)

        PROTO_METHOD(dump);
        PROTO_METHOD(toString);
        PROTO_METHOD(addCase);

#undef PROTO_METHOD
    }
};
