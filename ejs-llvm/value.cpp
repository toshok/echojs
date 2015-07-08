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

#include "value.h"

namespace ejsllvm {

typedef struct {
    /* object header */
    EJSObject obj;

    /* value specific data */
    llvm::Value *llvm_val;
} Value;

static ejsval _ejs_Value_prototype EJSVAL_ALIGNMENT;
static ejsval _ejs_Value EJSVAL_ALIGNMENT;

EJSObject *Value_alloc_instance() { return (EJSObject *)_ejs_gc_new(Value); }

static ejsval Value_impl(ejsval env, ejsval _this, int argc, ejsval *args) {
    EJS_NOT_IMPLEMENTED();
}

ejsval Value_new(llvm::Value *llvm_val) {
    EJSObject *result = Value_alloc_instance();
    _ejs_init_object(result, _ejs_Value_prototype, NULL);
    ((Value *)result)->llvm_val = llvm_val;
    return OBJECT_TO_EJSVAL(result);
}

ejsval Value_prototype_toString(ejsval env, ejsval _this, int argc,
                                ejsval *args) {
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((Value *)EJSVAL_TO_OBJECT(_this))->llvm_val->print(str_ostream);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval Value_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args) {
    ((Value *)EJSVAL_TO_OBJECT(_this))->llvm_val->dump();
    return _ejs_undefined;
}

ejsval Value_prototype_setName(ejsval env, ejsval _this, int argc,
                               ejsval *args) {
    Value *val = ((Value *)EJSVAL_TO_OBJECT(_this));
    REQ_UTF8_ARG(0, name);

    val->llvm_val->setName(name);
    return _ejs_undefined;
}

llvm::Value *Value_GetLLVMObj(ejsval val) {
    if (EJSVAL_IS_NULL(val))
        return NULL;
    return ((Value *)EJSVAL_TO_OBJECT(val))->llvm_val;
}

void Value_init(ejsval exports) {
    _ejs_gc_add_root(&_ejs_Value_prototype);
    _ejs_Value_prototype =
        _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);

    _ejs_Value = _ejs_function_new_utf8_with_proto(_ejs_null, "LLVMValue",
                                                   (EJSClosureFunc)Value_impl,
                                                   _ejs_Value_prototype);

    _ejs_object_setprop_utf8(exports, "Value", _ejs_Value);

#define PROTO_METHOD(x)                                                        \
    EJS_INSTALL_ATOM_FUNCTION(_ejs_Value_prototype, x, Value_prototype_##x)

    _ejs_object_setprop(_ejs_Value, _ejs_atom_prototype, _ejs_Value_prototype);

    PROTO_METHOD(setName);
    PROTO_METHOD(dump);
    PROTO_METHOD(toString);

#undef PROTO_METHOD
}
};
