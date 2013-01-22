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


/// landingpad instructions

typedef struct {
    /* object header */
    EJSObject obj;

    /* landingpad specific data */
    llvm::LandingPadInst *llvm_landing_pad;
} EJSLLVMLandingPad;

static EJSSpecOps _ejs_llvm_landingpad_specops;

static EJSObject* _ejs_llvm_LandingPad_allocate()
{
    return (EJSObject*)_ejs_gc_new(EJSLLVMLandingPad);
}


ejsval _ejs_llvm_LandingPad_proto;
ejsval _ejs_llvm_LandingPad;
static ejsval
_ejs_llvm_LandingPad_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
  EJS_NOT_IMPLEMENTED();
}

ejsval
_ejs_llvm_LandingPad_new(llvm::LandingPadInst* llvm_landingpad)
{
    ejsval result = _ejs_object_new (_ejs_llvm_LandingPad_proto, &_ejs_llvm_landingpad_specops);
    ((EJSLLVMLandingPad*)EJSVAL_TO_OBJECT(result))->llvm_landing_pad = llvm_landingpad;
    return result;
}

ejsval
_ejs_llvm_LandingPad_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((EJSLLVMLandingPad*)EJSVAL_TO_OBJECT(_this))->llvm_landing_pad->print(str_ostream, NULL);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_LandingPad_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
    ((EJSLLVMLandingPad*)EJSVAL_TO_OBJECT(_this))->llvm_landing_pad->dump();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_LandingPad_prototype_setCleanup(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMLandingPad *landing_pad = ((EJSLLVMLandingPad*)EJSVAL_TO_OBJECT(_this));
    REQ_BOOL_ARG(0, flag);
    landing_pad->llvm_landing_pad->setCleanup(flag);
    return _ejs_undefined;
}

ejsval
_ejs_llvm_LandingPad_prototype_addClause(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMLandingPad *landing_pad = ((EJSLLVMLandingPad*)EJSVAL_TO_OBJECT(_this));
    REQ_LLVM_VAL_ARG(0, clause_val);
    landing_pad->llvm_landing_pad->addClause(clause_val);
    return _ejs_undefined;
}

llvm::LandingPadInst*
_ejs_llvm_LandingPad_GetLLVMObj(ejsval val)
{
    if (EJSVAL_IS_NULL(val)) return NULL;
    return ((EJSLLVMLandingPad*)EJSVAL_TO_OBJECT(val))->llvm_landing_pad;
}

void
_ejs_llvm_LandingPad_init (ejsval exports)
{
    _ejs_llvm_landingpad_specops = _ejs_object_specops;
    _ejs_llvm_landingpad_specops.class_name = "LLVMLandingPad";
    _ejs_llvm_landingpad_specops.allocate = _ejs_llvm_LandingPad_allocate;

    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_llvm_LandingPad_proto);
    _ejs_llvm_LandingPad_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_llvm_landingpad_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMLandingPad", (EJSClosureFunc)_ejs_llvm_LandingPad_impl));
    _ejs_llvm_LandingPad = tmpobj;


#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_LandingPad_proto, EJS_STRINGIFY(x), _ejs_llvm_LandingPad_prototype_##x)

    _ejs_object_setprop (_ejs_llvm_LandingPad,       _ejs_atom_prototype,  _ejs_llvm_LandingPad_proto);

    PROTO_METHOD(dump);
    PROTO_METHOD(toString);
    PROTO_METHOD(setCleanup);
    PROTO_METHOD(addClause);

#undef PROTO_METHOD

    _ejs_object_setprop_utf8 (exports,              "LandingPad", _ejs_llvm_LandingPad);

    END_SHADOW_STACK_FRAME;
}

