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

/// landingpad instructions

typedef struct {
    /* object header */
    EJSObject obj;

    /* landingpad specific data */
    llvm::LandingPadInst *llvm_landing_pad;
} LandingPad;

static EJSSpecOps _ejs_LandingPad_specops;
static ejsval _ejs_LandingPad_prototype EJSVAL_ALIGNMENT;
static ejsval _ejs_LandingPad EJSVAL_ALIGNMENT;

static EJSObject *LandingPad_allocate() {
    return (EJSObject *)_ejs_gc_new(LandingPad);
}

static ejsval LandingPad_impl(ejsval env, ejsval _this, int argc,
                              ejsval *args) {
    EJS_NOT_IMPLEMENTED();
}

ejsval LandingPad_new(llvm::LandingPadInst *llvm_landingpad) {
    ejsval result =
        _ejs_object_new(_ejs_LandingPad_prototype, &_ejs_LandingPad_specops);
    ((LandingPad *)EJSVAL_TO_OBJECT(result))->llvm_landing_pad =
        llvm_landingpad;
    return result;
}

ejsval LandingPad_prototype_toString(ejsval env, ejsval _this, int argc,
                                     ejsval *args) {
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((LandingPad *)EJSVAL_TO_OBJECT(_this))
        ->llvm_landing_pad->print(str_ostream);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval LandingPad_prototype_dump(ejsval env, ejsval _this, int argc,
                                 ejsval *args) {
    ((LandingPad *)EJSVAL_TO_OBJECT(_this))->llvm_landing_pad->dump();
    return _ejs_undefined;
}

ejsval LandingPad_prototype_setCleanup(ejsval env, ejsval _this, int argc,
                                       ejsval *args) {
    LandingPad *landing_pad = ((LandingPad *)EJSVAL_TO_OBJECT(_this));
    REQ_BOOL_ARG(0, flag);
    landing_pad->llvm_landing_pad->setCleanup(flag);
    return _ejs_undefined;
}

ejsval LandingPad_prototype_addClause(ejsval env, ejsval _this, int argc,
                                      ejsval *args) {
    LandingPad *landing_pad = ((LandingPad *)EJSVAL_TO_OBJECT(_this));
    REQ_LLVM_VAL_ARG(0, clause_val);
    landing_pad->llvm_landing_pad->addClause(
        llvm::cast<llvm::Constant>(clause_val));
    return _ejs_undefined;
}

llvm::LandingPadInst *LandingPad_GetLLVMObj(ejsval val) {
    if (EJSVAL_IS_NULL(val))
        return NULL;
    return ((LandingPad *)EJSVAL_TO_OBJECT(val))->llvm_landing_pad;
}

void LandingPad_init(ejsval exports) {
    _ejs_LandingPad_specops = _ejs_Object_specops;
    _ejs_LandingPad_specops.class_name = "LLVMLandingPad";
    _ejs_LandingPad_specops.Allocate = LandingPad_allocate;

    _ejs_gc_add_root(&_ejs_LandingPad_prototype);
    _ejs_LandingPad_prototype =
        _ejs_object_new(_ejs_Object_prototype, &_ejs_LandingPad_specops);

    _ejs_LandingPad = _ejs_function_new_utf8_with_proto(
        _ejs_null, "LLVMLandingPad", (EJSClosureFunc)LandingPad_impl,
        _ejs_LandingPad_prototype);

    _ejs_object_setprop_utf8(exports, "LandingPad", _ejs_LandingPad);

#define PROTO_METHOD(x)                                                        \
    EJS_INSTALL_ATOM_FUNCTION(_ejs_LandingPad_prototype, x,                    \
                              LandingPad_prototype_##x)

    PROTO_METHOD(dump);
    PROTO_METHOD(toString);
    PROTO_METHOD(setCleanup);
    PROTO_METHOD(addClause);

#undef PROTO_METHOD
}
};
