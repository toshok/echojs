/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "function.h"
#include "functiontype.h"
#include "type.h"
#include "value.h"

namespace ejsllvm {

    typedef struct {
        /* object header */
        EJSObject obj;

        /* type specific data */
        llvm::Function *llvm_fun;
    } Function;

    static EJSSpecOps function_specops;

    static EJSObject* Function_allocate()
    {
        return (EJSObject*)_ejs_gc_new(Function);
    }



    static ejsval _ejs_Function_proto;
    static ejsval _ejs_Function;
    static ejsval
    Function_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    Function_new(llvm::Function* llvm_fun)
    {
        ejsval result = _ejs_object_new (_ejs_Function_proto, &function_specops);
        ((Function*)EJSVAL_TO_OBJECT(result))->llvm_fun = llvm_fun;
        return result;
    }

    ejsval
    Function_prototype_toString(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        std::string str;
        llvm::raw_string_ostream str_ostream(str);
        ((Function*)EJSVAL_TO_OBJECT(_this))->llvm_fun->print(str_ostream);

        return _ejs_string_new_utf8(trim(str_ostream.str()).c_str());
    }

    ejsval
    Function_prototype_dump(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        ((Function*)EJSVAL_TO_OBJECT(_this))->llvm_fun->dump();
        return _ejs_undefined;
    }

    ejsval
    Function_prototype_setOnlyReadsMemory(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        fun->llvm_fun->setOnlyReadsMemory();
        return _ejs_undefined;
    }

    ejsval
    Function_prototype_setDoesNotAccessMemory(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        fun->llvm_fun->setDoesNotAccessMemory();
        return _ejs_undefined;
    }

    ejsval
    Function_prototype_setDoesNotThrow(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        fun->llvm_fun->setDoesNotThrow();
        return _ejs_undefined;
    }

    ejsval
    Function_prototype_setGC(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        REQ_UTF8_ARG(0, name);
        fun->llvm_fun->setGC(name);
        free(name);
        return _ejs_undefined;
    }

    ejsval
    Function_prototype_setExternalLinkage(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        fun->llvm_fun->setLinkage (llvm::Function::ExternalLinkage);
        return _ejs_undefined;
    }

    ejsval
    Function_prototype_setInternalLinkage(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        fun->llvm_fun->setLinkage (llvm::Function::InternalLinkage);
        return _ejs_undefined;
    }

    ejsval
    Function_prototype_get_args(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        int size = fun->llvm_fun->arg_size();
        ejsval result = _ejs_array_new(0, EJS_FALSE);

        unsigned Idx = 0;
        for (llvm::Function::arg_iterator AI = fun->llvm_fun->arg_begin(); Idx != size;
             ++AI, ++Idx) {
            ejsval val = Value_new(AI);
            _ejs_array_push_dense (result, 1, &val);
        }
        return result;
    }

    ejsval
    Function_prototype_get_argSize(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        return NUMBER_TO_EJSVAL (fun->llvm_fun->arg_size());
    }

    ejsval
    Function_prototype_get_returnType(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        return Type_new (fun->llvm_fun->getReturnType());
    }

    ejsval
    Function_prototype_get_type(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        return FunctionType_new (fun->llvm_fun->getFunctionType());
    }

    ejsval
    Function_prototype_get_doesNotThrow(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        return BOOLEAN_TO_EJSVAL(fun->llvm_fun->doesNotThrow());
    }

    ejsval
    Function_prototype_get_doesNotAccessMemory(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        return BOOLEAN_TO_EJSVAL(fun->llvm_fun->doesNotAccessMemory());
    }

    ejsval
    Function_prototype_get_onlyReadsMemory(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        Function* fun = ((Function*)EJSVAL_TO_OBJECT(_this));
        return BOOLEAN_TO_EJSVAL(fun->llvm_fun->onlyReadsMemory());
    }

    llvm::Function*
    Function_GetLLVMObj(ejsval val)
    {
        if (EJSVAL_IS_NULL(val)) return NULL;
        return ((Function*)EJSVAL_TO_OBJECT(val))->llvm_fun;
    }

    void
    Function_init (ejsval exports)
    {
        function_specops = _ejs_object_specops;
        function_specops.class_name = "LLVMFunction";
        function_specops.allocate = Function_allocate;

        _ejs_gc_add_root (&_ejs_Function_proto);
        _ejs_Function_proto = _ejs_object_new(_ejs_Object_prototype, &function_specops);

        _ejs_Function = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMFunction", (EJSClosureFunc)Function_impl, _ejs_Function_proto);

        _ejs_object_setprop_utf8 (exports,              "Function", _ejs_Function);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_Function_proto, x, Function_prototype_##x)
#define PROTO_ACCESSOR(x) EJS_INSTALL_ATOM_GETTER(_ejs_Function_proto, x, Function_prototype_get_##x)

        PROTO_ACCESSOR(args);
        PROTO_ACCESSOR(argSize);
        PROTO_ACCESSOR(returnType);
        PROTO_ACCESSOR(type);
        PROTO_ACCESSOR(doesNotThrow);
        PROTO_ACCESSOR(onlyReadsMemory);
        PROTO_ACCESSOR(doesNotAccessMemory);

        PROTO_METHOD(dump);
        PROTO_METHOD(setOnlyReadsMemory);
        PROTO_METHOD(setDoesNotAccessMemory);
        PROTO_METHOD(setDoesNotThrow);
        PROTO_METHOD(setGC);
        PROTO_METHOD(setExternalLinkage);
        PROTO_METHOD(setInternalLinkage);
        PROTO_METHOD(toString);

#undef PROTO_METHOD
#undef PROTO_ACCESSOR
    }

};
