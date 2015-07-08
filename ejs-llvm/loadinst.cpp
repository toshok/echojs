/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-function.h"

namespace ejsllvm {
/// load instructions

typedef struct {
    /* object header */
    EJSObject obj;

    /* load specific data */
    llvm::LoadInst *llvm_load;
} LoadInst;

static EJSSpecOps _ejs_LoadInst_specops;
static ejsval _ejs_LoadInst_prototype EJSVAL_ALIGNMENT;
static ejsval _ejs_LoadInst EJSVAL_ALIGNMENT;

static EJSObject *LoadInst_Allocate() {
    return (EJSObject *)_ejs_gc_new(LoadInst);
}

static ejsval LoadInst_impl(ejsval env, ejsval _this, int argc, ejsval *args) {
    EJS_NOT_IMPLEMENTED();
}

ejsval LoadInst_new(llvm::LoadInst *llvm_load) {
    ejsval result =
        _ejs_object_new(_ejs_LoadInst_prototype, &_ejs_LoadInst_specops);
    ((LoadInst *)EJSVAL_TO_OBJECT(result))->llvm_load = llvm_load;
    return result;
}

ejsval LoadInst_prototype_toString(ejsval env, ejsval _this, int argc,
                                   ejsval *args) {
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((LoadInst *)EJSVAL_TO_OBJECT(_this))->llvm_load->print(str_ostream);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval LoadInst_prototype_dump(ejsval env, ejsval _this, int argc,
                               ejsval *args) {
    ((LoadInst *)EJSVAL_TO_OBJECT(_this))->llvm_load->dump();
    return _ejs_undefined;
}

ejsval LoadInst_prototype_setAlignment(ejsval env, ejsval _this, int argc,
                                       ejsval *args) {
    LoadInst *loadinst = ((LoadInst *)EJSVAL_TO_OBJECT(_this));
    REQ_INT_ARG(0, alignment);
    loadinst->llvm_load->setAlignment(alignment);
    return _ejs_undefined;
}

llvm::LoadInst *LoadInst_GetLLVMObj(ejsval val) {
    if (EJSVAL_IS_NULL(val))
        return NULL;
    return ((LoadInst *)EJSVAL_TO_OBJECT(val))->llvm_load;
}

void LoadInst_init(ejsval exports) {
    _ejs_LoadInst_specops = _ejs_Object_specops;
    _ejs_LoadInst_specops.class_name = "LLVMLoadInst";
    _ejs_LoadInst_specops.Allocate = LoadInst_Allocate;

    _ejs_gc_add_root(&_ejs_LoadInst_prototype);
    _ejs_LoadInst_prototype =
        _ejs_object_new(_ejs_Object_prototype, &_ejs_LoadInst_specops);

    _ejs_LoadInst = _ejs_function_new_utf8_with_proto(
        _ejs_null, "LLVMLoadInst", (EJSClosureFunc)LoadInst_impl,
        _ejs_LoadInst_prototype);

    _ejs_object_setprop_utf8(exports, "LoadInst", _ejs_LoadInst);

#define PROTO_METHOD(x)                                                        \
    EJS_INSTALL_ATOM_FUNCTION(_ejs_LoadInst_prototype, x,                      \
                              LoadInst_prototype_##x)

    PROTO_METHOD(dump);
    PROTO_METHOD(toString);
    PROTO_METHOD(setAlignment);

#undef PROTO_METHOD
}
}
