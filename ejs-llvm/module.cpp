/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdio.h>
#include <fstream>

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-array.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-proxy.h"
#include "ejs-symbol.h"

#include "function.h"
#include "type.h"
#include "globalvariable.h"
#include "module.h"
#include "value.h"

namespace ejsllvm {

    typedef struct {
        /* object header */
        EJSObject obj;

        /* module specific data */
        llvm::Module *llvm_module;
    } Module;

    static EJSSpecOps _ejs_Module_specops;
    static ejsval _ejs_Module_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_Module EJSVAL_ALIGNMENT;

    static EJSObject* Module_allocate()
    {
        return (EJSObject*)_ejs_gc_new(Module);
    }

    static void Module_finalize(EJSObject* obj)
    {
        Module* module = (Module*)obj;
        if (module->llvm_module)
            delete module->llvm_module;
        _ejs_Object_specops.finalize(obj);
    }

    static ejsval
    Module_create (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ejsval F = _this;
        if (!EJSVAL_IS_CONSTRUCTOR(F)) 
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in Module[Symbol.create] is not a constructor");
        EJSObject* F_ = EJSVAL_TO_OBJECT(F);
        // 2. Let obj be the result of calling OrdinaryCreateFromConstructor(F, "%DatePrototype%", ([[DateData]]) ). 
        ejsval proto = OP(F_,get)(F, _ejs_atom_prototype, F);
        if (EJSVAL_IS_UNDEFINED(proto))
            proto = _ejs_Module_prototype;

        EJSObject* obj = (EJSObject*)_ejs_gc_new (Module);
        _ejs_init_object (obj, proto, &_ejs_Module_specops);
        return OBJECT_TO_EJSVAL(obj);
    }

    static ejsval
    Module_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        if (EJSVAL_IS_UNDEFINED(_this)) {
            // called as a function
            EJS_NOT_IMPLEMENTED();
        }
        else {
            Module* module = ((Module*)EJSVAL_TO_OBJECT(_this));
            REQ_UTF8_ARG(0, name);
            module->llvm_module = new llvm::Module(name, llvm::getGlobalContext());
            free (name);
            return _this;
        }
    }

    ejsval
    Module_prototype_getOrInsertIntrinsic(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Module* module = ((Module*)EJSVAL_TO_OBJECT(_this));

        REQ_UTF8_ARG(0, id);
#if false
        REQ_ARRAY_ARG(1, paramTypes);

        std::vector< llvm::Type*> param_types;
        for (int i = 0; i < paramTypes->Length(); i ++) {
            param_types.push_back (Type_getLLVMObj(EJSARRAY_ELEMENTS(argTypes)[i]));
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

        free (id);

        return Function_new (f);
    }

    ejsval
    Module_prototype_getOrInsertFunction(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Module *module = ((Module*)EJSVAL_TO_OBJECT(_this));

        REQ_UTF8_ARG(0, name);
        REQ_LLVM_TYPE_ARG(1, returnType);
        REQ_ARRAY_ARG(2, paramTypes);

        std::vector< llvm::Type*> param_types;
        for (int i = 0; i < EJSARRAY_LEN(paramTypes); i ++) {
            param_types.push_back (Type_GetLLVMObj(EJSDENSEARRAY_ELEMENTS(paramTypes)[i]));
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

        free (name);

        return Function_new (f);
    }

    ejsval
    Module_prototype_getOrInsertGlobal(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Module *module = ((Module*)EJSVAL_TO_OBJECT(_this));
        REQ_UTF8_ARG(0, name);
        REQ_LLVM_TYPE_ARG(1, type);

        ejsval rv = GlobalVariable_new (static_cast<llvm::GlobalVariable*>(module->llvm_module->getOrInsertGlobal(name, type)));
        free (name);
        return rv;
    }

    ejsval
    Module_prototype_getOrInsertExternalFunction(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Module *module = ((Module*)EJSVAL_TO_OBJECT(_this));

        REQ_UTF8_ARG(0, name);
        REQ_LLVM_TYPE_ARG(1, returnType);
        REQ_ARRAY_ARG(2, paramTypes);

        std::vector< llvm::Type*> param_types;
        for (int i = 0; i < EJSARRAY_LEN(paramTypes); i ++) {
            param_types.push_back (Type_GetLLVMObj(EJSDENSEARRAY_ELEMENTS(paramTypes)[i]));
        }

        llvm::FunctionType *FT = llvm::FunctionType::get(returnType, param_types, false);

        llvm::Function* f = static_cast< llvm::Function*>(module->llvm_module->getOrInsertFunction(name, FT));
        f->setLinkage (llvm::Function::ExternalLinkage);

        free (name);

        return Function_new (f);
    }

    ejsval
    Module_prototype_getFunction(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Module *module = ((Module*)EJSVAL_TO_OBJECT(_this));

        REQ_UTF8_ARG(0, name);
	
        llvm::Function* f = static_cast< llvm::Function*>(module->llvm_module->getFunction(name));

        free (name);

        if (f)
            return Function_new (f);
        return _ejs_null;
    }

    ejsval
    Module_prototype_getGlobalVariable(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Module *module = ((Module*)EJSVAL_TO_OBJECT(_this));

        REQ_UTF8_ARG(0, name);
        REQ_BOOL_ARG(1, allowInternal);

        ejsval rv = GlobalVariable_new (module->llvm_module->getGlobalVariable(name, allowInternal));

        free (name);

        return rv;
    }

    ejsval
    Module_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((Module*)EJSVAL_TO_OBJECT(_this))->llvm_module->print(str_ostream, NULL);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    ejsval
    Module_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ((Module*)EJSVAL_TO_OBJECT(_this))->llvm_module->dump();
        return _ejs_undefined;
    }

    ejsval
    Module_prototype_writeToFile(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Module *module = ((Module*)EJSVAL_TO_OBJECT(_this));

        REQ_UTF8_ARG(0, path);

        std::ofstream output_file(path);
        llvm::raw_os_ostream raw_stream(output_file);
        module->llvm_module->print(raw_stream, NULL);
        output_file.close();

        free (path);

        return _ejs_undefined;
    }

    ejsval
    Module_prototype_writeBitcodeToFile(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Module *module = ((Module*)EJSVAL_TO_OBJECT(_this));

        REQ_UTF8_ARG(0, path);

        std::string error;
        llvm::raw_fd_ostream OS(path, error, llvm::sys::fs::F_Binary);
        // check error

        llvm::WriteBitcodeToFile (module->llvm_module, OS);

        free (path);

        return _ejs_undefined;
    }

    ejsval
    Module_prototype_setDataLayout(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Module *module = ((Module*)EJSVAL_TO_OBJECT(_this));

        REQ_UTF8_ARG(0, dataLayout);

        module->llvm_module->setDataLayout (dataLayout);
        return _ejs_undefined;
    }

    ejsval
    Module_prototype_setTriple(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Module *module = ((Module*)EJSVAL_TO_OBJECT(_this));

        REQ_UTF8_ARG(0, triple);

        module->llvm_module->setTargetTriple (triple);
        return _ejs_undefined;
    }

    llvm::Module*
    Module_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((Module*)EJSVAL_TO_OBJECT(val))->llvm_module;
    }

    void
    Module_init (ejsval exports)
    {
        _ejs_Module_specops = _ejs_Object_specops;
        _ejs_Module_specops.class_name = "LLVMModule";
        _ejs_Module_specops.allocate = Module_allocate;
        _ejs_Module_specops.finalize = Module_finalize;

        _ejs_gc_add_root (&_ejs_Module_prototype);
        _ejs_Module_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_Module_specops);

        _ejs_Module = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMModule", (EJSClosureFunc)Module_impl, _ejs_Module_prototype);

        _ejs_object_setprop_utf8 (exports,              "Module", _ejs_Module);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Module_prototype, x, Module_prototype_##x)

        PROTO_METHOD(getGlobalVariable);
        PROTO_METHOD(getOrInsertIntrinsic);
        PROTO_METHOD(getOrInsertFunction);
        PROTO_METHOD(getOrInsertGlobal);
        PROTO_METHOD(getOrInsertExternalFunction);
        PROTO_METHOD(getFunction);
        PROTO_METHOD(dump);
        PROTO_METHOD(toString);
        PROTO_METHOD(writeToFile);
        PROTO_METHOD(writeBitcodeToFile);
        PROTO_METHOD(setDataLayout);
        PROTO_METHOD(setTriple);

#undef PROTO_METHOD

        EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_Module, create, Module_create, EJS_PROP_NOT_ENUMERABLE);
    }
};
