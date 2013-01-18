/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include <fstream>

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-array.h"
#include "ejs-function.h"

#include "function.h"
#include "type.h"
#include "module.h"
#include "value.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* module specific data */
    llvm::Module *llvm_module;
} EJSLLVMModule;

static EJSSpecOps _ejs_module_specops;

static EJSObject* _ejs_llvm_Module_allocate()
{
    return (EJSObject*)_ejs_gc_new(EJSLLVMModule);
}


ejsval _ejs_llvm_Module_proto;
ejsval _ejs_llvm_Module;
static ejsval
_ejs_llvm_Module_impl (ejsval env, ejsval _this, int argc, ejsval *args)
{
    if (EJSVAL_IS_UNDEFINED(_this)) {
        // called as a function
        EJS_NOT_IMPLEMENTED();
    }
    else {
        EJSLLVMModule* module = ((EJSLLVMModule*)EJSVAL_TO_OBJECT(_this));
        REQ_UTF8_ARG(0, name);
        module->llvm_module = new llvm::Module(name, llvm::getGlobalContext());
        return _this;
    }
}

ejsval
_ejs_llvm_Module_prototype_getOrInsertIntrinsic(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMModule* module = ((EJSLLVMModule*)EJSVAL_TO_OBJECT(_this));

    REQ_UTF8_ARG(0, id);
#if false
    REQ_ARRAY_ARG(1, paramTypes);

    std::vector< llvm::Type*> param_types;
    for (int i = 0; i < paramTypes->Length(); i ++) {
        param_types.push_back (_ejs_llvm_Type_getLLVMObj(EJSARRAY_ELEMENTS(argTypes)[i]));
    }
#endif
    llvm::Intrinsic::ID intrinsic_id;

    if (!strcmp (id, "@llvm.gcroot")) {
        intrinsic_id = llvm::Intrinsic::gcroot;
    }
    else {
        abort();
    }

#if false
    llvm::Function* f = llvm::Intrinsic::getDeclaration (module->llvm_module, intrinsic_id, param_types);
#else
    llvm::Function* f = llvm::Intrinsic::getDeclaration (module->llvm_module, intrinsic_id);
#endif

    return _ejs_llvm_Function_new (f);
}

ejsval
_ejs_llvm_Module_prototype_getOrInsertFunction(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMModule *module = ((EJSLLVMModule*)EJSVAL_TO_OBJECT(_this));

    REQ_UTF8_ARG(0, name);
    REQ_LLVM_TYPE_ARG(1, returnType);
    REQ_ARRAY_ARG(2, paramTypes);

    std::vector< llvm::Type*> param_types;
    for (int i = 0; i < EJSARRAY_LEN(paramTypes); i ++) {
        param_types.push_back (_ejs_llvm_Type_getLLVMObj(EJSARRAY_ELEMENTS(paramTypes)[i]));
    }

    llvm::FunctionType *FT = llvm::FunctionType::get(returnType, param_types, false);

    llvm::Function* f = static_cast< llvm::Function*>(module->llvm_module->getOrInsertFunction(name, FT));

    // XXX this needs to come from the js call, since when we hoist anonymous methods we'll need to give them a private linkage.
    f->setLinkage (llvm::Function::ExternalLinkage);

    // XXX the args might not be identifiers but might instead be destructuring expressions.  punt for now.

#if notyet
    // Set names for all arguments.
    unsigned Idx = 0;
    for (Function::arg_iterator AI = F->arg_begin(); Idx != Args.size();
	 ++AI, ++Idx)
      AI->setName(Args[Idx]);
#endif

    return _ejs_llvm_Function_new (f);
}

ejsval
_ejs_llvm_Module_prototype_getOrInsertGlobal(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMModule *module = ((EJSLLVMModule*)EJSVAL_TO_OBJECT(_this));
    REQ_UTF8_ARG(0, name);
    REQ_LLVM_TYPE_ARG(1, type);

    return _ejs_llvm_Value_new (module->llvm_module->getOrInsertGlobal(name, type));
}

ejsval
_ejs_llvm_Module_prototype_getOrInsertExternalFunction(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMModule *module = ((EJSLLVMModule*)EJSVAL_TO_OBJECT(_this));

    REQ_UTF8_ARG(0, name);
    REQ_LLVM_TYPE_ARG(1, type);

    return _ejs_llvm_Value_new (module->llvm_module->getOrInsertGlobal(name, type));
}

ejsval
_ejs_llvm_Module_prototype_getFunction(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMModule *module = ((EJSLLVMModule*)EJSVAL_TO_OBJECT(_this));

    REQ_UTF8_ARG(0, name);
	
    llvm::Function* f = static_cast< llvm::Function*>(module->llvm_module->getFunction(name));

    if (f)
        return _ejs_llvm_Function_new (f);
    return _ejs_null;
}

ejsval
_ejs_llvm_Module_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
{
    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    ((EJSLLVMModule*)EJSVAL_TO_OBJECT(_this))->llvm_module->print(str_ostream, NULL);

    return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
}

ejsval
_ejs_llvm_Module_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
{
    ((EJSLLVMModule*)EJSVAL_TO_OBJECT(_this))->llvm_module->dump();
    return _ejs_undefined;
}

ejsval
_ejs_llvm_Module_prototype_writeToFile(ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSLLVMModule *module = ((EJSLLVMModule*)EJSVAL_TO_OBJECT(_this));

    REQ_UTF8_ARG(0, path);

    std::ofstream output_file(path);
    llvm::raw_os_ostream raw_stream(output_file);
    module->llvm_module->print(raw_stream, NULL);
    output_file.close();

    return _ejs_undefined;
}

void
_ejs_llvm_Module_init (ejsval exports)
{
    _ejs_module_specops = _ejs_object_specops;
    _ejs_module_specops.allocate = _ejs_llvm_Module_allocate;

    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_llvm_Module_proto);
    _ejs_llvm_Module_proto = _ejs_object_new(_ejs_Object_prototype, &_ejs_module_specops);

    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMModule", (EJSClosureFunc)_ejs_llvm_Module_impl));
    _ejs_llvm_Module = tmpobj;


#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_llvm_Module_proto, EJS_STRINGIFY(x), _ejs_llvm_Module_prototype_##x)

    _ejs_object_setprop (_ejs_llvm_Module,       _ejs_atom_prototype,  _ejs_llvm_Module_proto);

    PROTO_METHOD(getOrInsertIntrinsic);
    PROTO_METHOD(getOrInsertFunction);
    PROTO_METHOD(getOrInsertGlobal);
    PROTO_METHOD(getOrInsertExternalFunction);
    PROTO_METHOD(getFunction);
    PROTO_METHOD(dump);
    PROTO_METHOD(toString);
    PROTO_METHOD(writeToFile);

#undef PROTO_METHOD

    _ejs_object_setprop_utf8 (exports,              "Module", _ejs_llvm_Module);

    END_SHADOW_STACK_FRAME;
}
