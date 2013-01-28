/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-array.h"
#include "ejs-string.h"
#include "constantarray.h"
#include "type.h"
#include "value.h"

namespace ejsllvm {

  static ejsval _ejs_ConstantArray_proto;
  static ejsval _ejs_ConstantArray;
  static ejsval
  ConstantArray_impl (ejsval env, ejsval _this, int argc, ejsval *args)
  {
    EJS_NOT_IMPLEMENTED();
  }

  static ejsval
  ConstantArray_get (ejsval env, ejsval _this, int argc, ejsval *args)
  {
    REQ_LLVM_TYPE_ARG(0, array_type);
    REQ_ARRAY_ARG(1, elements);

    std::vector< llvm::Constant*> element_constants;
    for (int i = 0; i < EJSARRAY_LEN(elements); i ++) {
      element_constants.push_back (static_cast<llvm::Constant*>(Value_GetLLVMObj(EJSARRAY_ELEMENTS(elements)[i])));
    }

    return Value_new (llvm::ConstantArray::get(static_cast<llvm::ArrayType*>(array_type), element_constants));
  }

  void
  ConstantArray_init (ejsval exports)
  {
    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_ConstantArray_proto);
    _ejs_ConstantArray_proto = _ejs_object_create(_ejs_Object_prototype);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMConstantArray", (EJSClosureFunc)ConstantArray_impl));
    _ejs_ConstantArray = tmpobj;


#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_ConstantArray, EJS_STRINGIFY(x), ConstantArray_##x)
#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_ConstantArray_proto, EJS_STRINGIFY(x), ConstantArray_prototype_##x)

    _ejs_object_setprop (_ejs_ConstantArray,       _ejs_atom_prototype,  _ejs_ConstantArray_proto);

    OBJ_METHOD(get);

#undef PROTO_METHOD
#undef OBJ_METHOD

    _ejs_object_setprop_utf8 (exports,              "ConstantArray", _ejs_ConstantArray);

    END_SHADOW_STACK_FRAME;
  }

};
