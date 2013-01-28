/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "irbuilder.h"
#include "type.h"
#include "value.h"
#include "landingpad.h"
#include "switch.h"
#include "callinvoke.h"
#include "basicblock.h"

namespace ejsllvm {

    static llvm::IRBuilder<> _llvm_builder(llvm::getGlobalContext());

    static ejsval _ejs_IRBuilder_proto;
    static ejsval _ejs_IRBuilder;
    static ejsval
    IRBuilder_impl (ejsval env, ejsval _this, int argc, ejsval *args)
    {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    IRBuilder_new(llvm::IRBuilder<>* llvm_fun)
    {
        return _ejs_object_create (_ejs_null);
    }


    ejsval
    IRBuilder_setInsertPoint(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_BB_ARG(0, bb);
        if (bb != NULL)
            _llvm_builder.SetInsertPoint (bb);
        return _ejs_undefined;
    }

    ejsval
    IRBuilder_setInsertPointStartBB(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_BB_ARG(0, bb);
        if (bb != NULL)
            _llvm_builder.SetInsertPoint (bb, bb->getFirstInsertionPt());
        return _ejs_undefined;
    }

    ejsval
    IRBuilder_getInsertBlock(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        llvm::BasicBlock *llvm_bb = _llvm_builder.GetInsertBlock();
        if (llvm_bb)
            return BasicBlock_new (llvm_bb);
        return _ejs_null;
    }

    ejsval
    IRBuilder_createRet(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0,val);
        return Value_new(_llvm_builder.CreateRet(val));
    }

    ejsval
    IRBuilder_createRetVoid(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        return Value_new(_llvm_builder.CreateRetVoid());
    }

    ejsval
    IRBuilder_createPointerCast(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0,val);
        REQ_LLVM_TYPE_ARG(1,ty);
        REQ_UTF8_ARG(2,name);

        ejsval rv = Value_new(_llvm_builder.CreatePointerCast(val, ty, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createFPCast(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0,val);
        REQ_LLVM_TYPE_ARG(1,ty);
        REQ_UTF8_ARG(2,name);

        ejsval rv = Value_new (_llvm_builder.CreateFPCast(val, ty, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createCall(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, callee);
        REQ_ARRAY_ARG(1, argv);
        REQ_UTF8_ARG(2, name);

        std::vector<llvm::Value*> ArgsV;
        for (unsigned i = 0, e = EJSARRAY_LEN(argv); i != e; ++i) {
            ArgsV.push_back (Value_GetLLVMObj(EJSARRAY_ELEMENTS(argv)[i]));
            if (ArgsV.back() == 0) abort(); // XXX throw an exception here
        }

        ejsval rv = Call_new (_llvm_builder.CreateCall(callee, ArgsV, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createInvoke(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, callee);
        REQ_ARRAY_ARG(1, argv);
        REQ_LLVM_BB_ARG(2, normal_dest);
        REQ_LLVM_BB_ARG(3, unwind_dest);
        REQ_UTF8_ARG(4, name);

        std::vector<llvm::Value*> ArgsV;
        for (unsigned i = 0, e = EJSARRAY_LEN(argv); i != e; ++i) {
            ArgsV.push_back (Value_GetLLVMObj(EJSARRAY_ELEMENTS(argv)[i]));
            if (ArgsV.back() == 0) abort(); // XXX throw an exception here
        }

        ejsval rv = Invoke_new (_llvm_builder.CreateInvoke(callee, normal_dest, unwind_dest, ArgsV, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createFAdd(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, left);
        REQ_LLVM_VAL_ARG(1, right);
        REQ_UTF8_ARG(2, name);

        ejsval rv = Value_new (_llvm_builder.CreateFAdd(left, right, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createAlloca(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_TYPE_ARG(0, ty);
        REQ_UTF8_ARG(1, name);

        ejsval rv = Value_new (_llvm_builder.CreateAlloca(ty, 0, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createLoad(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, val);
        REQ_UTF8_ARG(1, name);

        ejsval rv = Value_new (_llvm_builder.CreateLoad(val, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createStore(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, val);
        REQ_LLVM_VAL_ARG(1, ptr);

        return Value_new (_llvm_builder.CreateStore(val, ptr));
    }

    ejsval
    IRBuilder_createExtractElement(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, val);
        REQ_LLVM_VAL_ARG(1, idx);
        REQ_UTF8_ARG(2, name);

        ejsval rv = Value_new (_llvm_builder.CreateExtractElement(val, idx, name));
        free (name);

        return rv;
    }

    ejsval
    IRBuilder_createGetElementPointer(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, val);
        REQ_ARRAY_ARG(1, idxv);
        REQ_UTF8_ARG(2, name);

        std::vector<llvm::Value*> IdxV;
        for (unsigned i = 0, e = EJSARRAY_LEN(idxv); i != e; ++i) {
            IdxV.push_back (Value_GetLLVMObj(EJSARRAY_ELEMENTS(idxv)[i]));
            if (IdxV.back() == 0) abort(); // XXX throw an exception here
        }

        ejsval rv = Value_new (_llvm_builder.CreateGEP(val, IdxV, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createInBoundsGetElementPointer(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, val);
        REQ_ARRAY_ARG(1, idxv);
        REQ_UTF8_ARG(2, name);

        std::vector<llvm::Value*> IdxV;
        for (unsigned i = 0, e = EJSARRAY_LEN(idxv); i != e; ++i) {
            IdxV.push_back (Value_GetLLVMObj(EJSARRAY_ELEMENTS(idxv)[i]));
            if (IdxV.back() == 0) abort(); // XXX throw an exception here
        }

        ejsval rv = Value_new (_llvm_builder.CreateInBoundsGEP(val, IdxV, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createStructGetElementPointer(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, val);
        REQ_INT_ARG(1, idx);
        REQ_UTF8_ARG(2, name);

        ejsval rv = Value_new (_llvm_builder.CreateStructGEP(val, idx, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createICmpEq(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, left);
        REQ_LLVM_VAL_ARG(1, right);
        REQ_UTF8_ARG(2, name);

        ejsval rv = Value_new (_llvm_builder.CreateICmpEQ(left, right, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createICmpSGt(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, left);
        REQ_LLVM_VAL_ARG(1, right);
        REQ_UTF8_ARG(2, name);

        ejsval rv = Value_new (_llvm_builder.CreateICmpSGT(left, right, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createBr(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_BB_ARG(0, dest);

        return Value_new (_llvm_builder.CreateBr(dest));
    }

    ejsval
    IRBuilder_createCondBr(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, cond);
        REQ_LLVM_BB_ARG(1, thenPart);
        REQ_LLVM_BB_ARG(2, elsePart);

        return Value_new (_llvm_builder.CreateCondBr(cond, thenPart, elsePart));
    }

    ejsval
    IRBuilder_createPhi(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        abort();
#if notyet
        REQ_LLVM_TYPE_ARG(0, ty);
        REQ_INT_ARG(1, incoming_values);
        REQ_UTF8_ARG(2, name);

        ejsval rv = Value_new (_llvm_builder.CreatePHI(ty, incoming_values, name));
        free (name);
        return rv;
#endif
    }

    ejsval
    IRBuilder_createGlobalStringPtr(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_UTF8_ARG(0, val);
        REQ_UTF8_ARG(1, name);

        ejsval rv = Value_new (_llvm_builder.CreateGlobalStringPtr(val, name));
        free (val);
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createUnreachable(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        return Value_new (_llvm_builder.CreateUnreachable());
    }

    ejsval
    IRBuilder_createSwitch(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, V);
        REQ_LLVM_BB_ARG(1, Dest);
        REQ_INT_ARG(2, num_cases);

        return Switch_new (_llvm_builder.CreateSwitch(V, Dest, num_cases));
    }

    ejsval
    IRBuilder_createLandingPad(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_TYPE_ARG(0, ty);
        REQ_LLVM_VAL_ARG(1, persFn);
        REQ_INT_ARG(2, num_clauses);
        REQ_UTF8_ARG(3, name);

        ejsval rv = LandingPad_new (_llvm_builder.CreateLandingPad(ty, persFn, num_clauses, name));
        free (name);
        return rv;
    }

    ejsval
    IRBuilder_createResume(ejsval env, ejsval _this, int argc, ejsval *args)
    {
        REQ_LLVM_VAL_ARG(0, val);

        return Value_new (_llvm_builder.CreateResume(val));
    }

    void
    IRBuilder_init (ejsval exports)
    {
        START_SHADOW_STACK_FRAME;

        _ejs_gc_add_named_root (_ejs_IRBuilder_proto);
        _ejs_IRBuilder_proto = _ejs_object_create(_ejs_Object_prototype);

        ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new_utf8 (_ejs_null, "LLVMIRBuilder", (EJSClosureFunc)IRBuilder_impl));
        _ejs_IRBuilder = tmpobj;


#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_IRBuilder, EJS_STRINGIFY(x), IRBuilder_##x)

        _ejs_object_setprop (_ejs_IRBuilder,       _ejs_atom_prototype,  _ejs_IRBuilder_proto);

        OBJ_METHOD(setInsertPoint);
        OBJ_METHOD(setInsertPointStartBB);
        OBJ_METHOD(getInsertBlock);
        OBJ_METHOD(createRet);
        OBJ_METHOD(createRetVoid);
        OBJ_METHOD(createPointerCast);
        OBJ_METHOD(createFPCast);
        OBJ_METHOD(createCall);
        OBJ_METHOD(createInvoke);
        OBJ_METHOD(createFAdd);
        OBJ_METHOD(createAlloca);
        OBJ_METHOD(createLoad);
        OBJ_METHOD(createStore);
        OBJ_METHOD(createExtractElement);
        OBJ_METHOD(createGetElementPointer);
        OBJ_METHOD(createInBoundsGetElementPointer);
        OBJ_METHOD(createStructGetElementPointer);
        OBJ_METHOD(createICmpEq);
        OBJ_METHOD(createICmpSGt);
        OBJ_METHOD(createBr);
        OBJ_METHOD(createCondBr);
        OBJ_METHOD(createPhi);
        OBJ_METHOD(createGlobalStringPtr);
        OBJ_METHOD(createUnreachable);

        OBJ_METHOD(createSwitch);
    
        OBJ_METHOD(createLandingPad);
        OBJ_METHOD(createResume);

#undef OBJ_METHOD

        _ejs_object_setprop_utf8 (exports,              "IRBuilder", _ejs_IRBuilder);

        END_SHADOW_STACK_FRAME;
    }
};
