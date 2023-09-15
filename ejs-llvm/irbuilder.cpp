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
#include "dibuilder.h"
#include "type.h"
#include "value.h"
#include "landingpad.h"
#include "switch.h"
#include "callinvoke.h"
#include "basicblock.h"
#include "allocainst.h"
#include "loadinst.h"

namespace ejsllvm {

    static llvm::IRBuilder<> _llvm_builder(TheContext);

    static ejsval _ejs_IRBuilder_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_IRBuilder EJSVAL_ALIGNMENT;

    static EJS_NATIVE_FUNC(IRBuilder_impl) {
        EJS_NOT_IMPLEMENTED();
    }

    ejsval
    IRBuilder_new(llvm::IRBuilder<>* llvm_fun)
    {
        return _ejs_object_create (_ejs_null);
    }


    static EJS_NATIVE_FUNC(IRBuilder_setInsertPoint) {
        REQ_LLVM_BB_ARG(0, bb);
        if (bb != NULL)
            _llvm_builder.SetInsertPoint (bb);
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(IRBuilder_setInsertPointStartBB) {
        REQ_LLVM_BB_ARG(0, bb);
        if (bb != NULL)
            _llvm_builder.SetInsertPoint (bb, bb->getFirstInsertionPt());
        return _ejs_undefined;
    }

    static EJS_NATIVE_FUNC(IRBuilder_getInsertBlock) {
        llvm::BasicBlock *llvm_bb = _llvm_builder.GetInsertBlock();
        if (llvm_bb)
            return BasicBlock_new (llvm_bb);
        return _ejs_null;
    }

    static EJS_NATIVE_FUNC(IRBuilder_createRet) {
        REQ_LLVM_VAL_ARG(0,val);
        return Value_new(_llvm_builder.CreateRet(val));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createRetVoid) {
        return Value_new(_llvm_builder.CreateRetVoid());
    }

    static EJS_NATIVE_FUNC(IRBuilder_createPointerCast) {
        REQ_LLVM_VAL_ARG(0,val);
        REQ_LLVM_TYPE_ARG(1,ty);
        FALLBACK_EMPTY_UTF8_ARG(2,name);

        return Value_new(_llvm_builder.CreatePointerCast(val, ty, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createFPCast) {
        REQ_LLVM_VAL_ARG(0,val);
        REQ_LLVM_TYPE_ARG(1,ty);
        FALLBACK_EMPTY_UTF8_ARG(2,name);

        return Value_new (_llvm_builder.CreateFPCast(val, ty, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createCall) {
        REQ_LLVM_TYPE_ARG(0, type);
        REQ_LLVM_VAL_ARG(1, callee);
        REQ_ARRAY_ARG(2, argv);
        FALLBACK_EMPTY_UTF8_ARG(3, name);

        std::vector<llvm::Value*> ArgsV;
        for (unsigned i = 0, e = EJSARRAY_LEN(argv); i != e; ++i) {
            ArgsV.push_back (Value_GetLLVMObj(EJSDENSEARRAY_ELEMENTS(argv)[i]));
            EJS_ASSERT(ArgsV.back() != 0); // XXX throw an exception here
        }

        auto FT = static_cast<llvm::FunctionType*>(type); // XXX need to make this safe...
        return Call_new (_llvm_builder.CreateCall(FT, callee, ArgsV, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createInvoke) {
        REQ_LLVM_TYPE_ARG(0, type);
        REQ_LLVM_VAL_ARG(1, callee);
        REQ_ARRAY_ARG(2, argv);
        REQ_LLVM_BB_ARG(3, normal_dest);
        REQ_LLVM_BB_ARG(4, unwind_dest);
        FALLBACK_EMPTY_UTF8_ARG(5, name);

        std::vector<llvm::Value*> ArgsV;
        for (unsigned i = 0, e = EJSARRAY_LEN(argv); i != e; ++i) {
            ArgsV.push_back (Value_GetLLVMObj(EJSDENSEARRAY_ELEMENTS(argv)[i]));
            EJS_ASSERT(ArgsV.back() != 0); // XXX throw an exception here
        }

        auto FT = static_cast<llvm::FunctionType*>(type); // XXX need to make this safe...
        return Invoke_new (_llvm_builder.CreateInvoke(FT, callee, normal_dest, unwind_dest, ArgsV, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createFAdd) {
        REQ_LLVM_VAL_ARG(0, left);
        REQ_LLVM_VAL_ARG(1, right);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return Value_new (_llvm_builder.CreateFAdd(left, right, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createAlloca) {
        REQ_LLVM_TYPE_ARG(0, ty);
        FALLBACK_EMPTY_UTF8_ARG(1, name);

        return AllocaInst_new (_llvm_builder.CreateAlloca(ty, 0, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createLoad) {
        REQ_LLVM_TYPE_ARG(0, ty);
        REQ_LLVM_VAL_ARG(1, val);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return LoadInst_new (_llvm_builder.CreateLoad(ty, val, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createStore) {
        REQ_LLVM_VAL_ARG(0, val);
        REQ_LLVM_VAL_ARG(1, ptr);

        return Value_new (_llvm_builder.CreateStore(val, ptr));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createExtractElement) {
        REQ_LLVM_VAL_ARG(0, val);
        REQ_LLVM_VAL_ARG(1, idx);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return Value_new (_llvm_builder.CreateExtractElement(val, idx, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createExtractValue) {
        REQ_LLVM_VAL_ARG(0, val);
        REQ_INT_ARG(1, idx);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return Value_new (_llvm_builder.CreateExtractValue(val, idx, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createGetElementPointer) {
        REQ_LLVM_TYPE_ARG(0, ty);
        REQ_LLVM_VAL_ARG(1, val);
        REQ_ARRAY_ARG(2, idxv);
        FALLBACK_EMPTY_UTF8_ARG(3, name);

        std::vector<llvm::Value*> IdxV;
        for (unsigned i = 0, e = EJSARRAY_LEN(idxv); i != e; ++i) {
            IdxV.push_back (Value_GetLLVMObj(EJSDENSEARRAY_ELEMENTS(idxv)[i]));
            EJS_ASSERT(IdxV.back() != 0); // XXX throw an exception here
        }

        return Value_new (_llvm_builder.CreateGEP(ty, val, IdxV, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createInBoundsGetElementPointer) {
        REQ_LLVM_TYPE_ARG(0, ty);
        REQ_LLVM_VAL_ARG(1, val);
        REQ_ARRAY_ARG(2, idxv);
        FALLBACK_EMPTY_UTF8_ARG(3, name);

        std::vector<llvm::Value*> IdxV;
        for (unsigned i = 0, e = EJSARRAY_LEN(idxv); i != e; ++i) {
            IdxV.push_back (Value_GetLLVMObj(EJSDENSEARRAY_ELEMENTS(idxv)[i]));
            EJS_ASSERT(IdxV.back() != 0); // XXX throw an exception here
        }

        return Value_new (_llvm_builder.CreateInBoundsGEP(ty, val, IdxV, name));
    }

    /*
    static EJS_NATIVE_FUNC(IRBuilder_createStructGetElementPointer) {
        REQ_LLVM_VAL_ARG(0, val);
        REQ_INT_ARG(1, idx);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return Value_new (_llvm_builder.CreateStructGEP(val, idx, name));
    }
    */

    static EJS_NATIVE_FUNC(IRBuilder_createICmpEq) {
        REQ_LLVM_VAL_ARG(0, left);
        REQ_LLVM_VAL_ARG(1, right);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return Value_new (_llvm_builder.CreateICmpEQ(left, right, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createICmpSGt) {
        REQ_LLVM_VAL_ARG(0, left);
        REQ_LLVM_VAL_ARG(1, right);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return Value_new (_llvm_builder.CreateICmpSGT(left, right, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createICmpUGt) {
        REQ_LLVM_VAL_ARG(0, left);
        REQ_LLVM_VAL_ARG(1, right);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return Value_new (_llvm_builder.CreateICmpUGT(left, right, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createICmpUGE) {
        REQ_LLVM_VAL_ARG(0, left);
        REQ_LLVM_VAL_ARG(1, right);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return Value_new (_llvm_builder.CreateICmpUGE(left, right, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createICmpULt) {
        REQ_LLVM_VAL_ARG(0, left);
        REQ_LLVM_VAL_ARG(1, right);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return Value_new (_llvm_builder.CreateICmpULT(left, right, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createBr) {
        REQ_LLVM_BB_ARG(0, dest);

        return Value_new (_llvm_builder.CreateBr(dest));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createCondBr) {
        REQ_LLVM_VAL_ARG(0, cond);
        REQ_LLVM_BB_ARG(1, thenPart);
        REQ_LLVM_BB_ARG(2, elsePart);

        return Value_new (_llvm_builder.CreateCondBr(cond, thenPart, elsePart));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createPhi) {
        EJS_NOT_IMPLEMENTED();
#if notyet
        REQ_LLVM_TYPE_ARG(0, ty);
        REQ_INT_ARG(1, incoming_values);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        ejsval rv = Value_new (_llvm_builder.CreatePHI(ty, incoming_values, name));
        free (name);
        return rv;
#endif
    }

    static EJS_NATIVE_FUNC(IRBuilder_createGlobalStringPtr) {
        REQ_UTF8_ARG(0, val);
        FALLBACK_EMPTY_UTF8_ARG(1, name);

        return Value_new (_llvm_builder.CreateGlobalStringPtr(val, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createUnreachable) {
        return Value_new (_llvm_builder.CreateUnreachable());
    }

    static EJS_NATIVE_FUNC(IRBuilder_createAnd) {
        REQ_LLVM_VAL_ARG(0, lhs);
        REQ_LLVM_VAL_ARG(1, rhs);
        FALLBACK_EMPTY_UTF8_ARG(2, name);
        return Value_new (_llvm_builder.CreateAnd(lhs, rhs, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createOr) {
        REQ_LLVM_VAL_ARG(0, lhs);
        REQ_LLVM_VAL_ARG(1, rhs);
        FALLBACK_EMPTY_UTF8_ARG(2, name);
        return Value_new (_llvm_builder.CreateOr(lhs, rhs, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createTrunc) {
        REQ_LLVM_VAL_ARG(0, V);
        REQ_LLVM_TYPE_ARG(1, dest_ty);
        FALLBACK_EMPTY_UTF8_ARG(2, name);
        return Value_new (_llvm_builder.CreateTrunc(V, dest_ty, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createZExt) {
        REQ_LLVM_VAL_ARG(0, V);
        REQ_LLVM_TYPE_ARG(1, dest_ty);
        FALLBACK_EMPTY_UTF8_ARG(2, name);
        return Value_new (_llvm_builder.CreateZExt(V, dest_ty, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createIntToPtr) {
        REQ_LLVM_VAL_ARG(0, V);
        REQ_LLVM_TYPE_ARG(1, dest_ty);
        FALLBACK_EMPTY_UTF8_ARG(2, name);
        return Value_new (_llvm_builder.CreateIntToPtr(V, dest_ty, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createPtrToInt) {
        REQ_LLVM_VAL_ARG(0, V);
        REQ_LLVM_TYPE_ARG(1, dest_ty);
        FALLBACK_EMPTY_UTF8_ARG(2, name);
        return Value_new (_llvm_builder.CreatePtrToInt(V, dest_ty, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createBitCast) {
        REQ_LLVM_VAL_ARG(0, V);
        REQ_LLVM_TYPE_ARG(1, dest_ty);
        FALLBACK_EMPTY_UTF8_ARG(2, name);
        return Value_new (_llvm_builder.CreateBitCast(V, dest_ty, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createSwitch) {
        REQ_LLVM_VAL_ARG(0, V);
        REQ_LLVM_BB_ARG(1, Dest);
        REQ_INT_ARG(2, num_cases);

        return Switch_new (_llvm_builder.CreateSwitch(V, Dest, num_cases));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createSelect) {
        REQ_LLVM_VAL_ARG(0, C);
        REQ_LLVM_VAL_ARG(1, True);
        REQ_LLVM_VAL_ARG(2, False);
        FALLBACK_EMPTY_UTF8_ARG(3, name);

        return Value_new(_llvm_builder.CreateSelect(C, True, False, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createNswSub) {
        REQ_LLVM_VAL_ARG(0, lhs);
        REQ_LLVM_VAL_ARG(1, rhs);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return Value_new(_llvm_builder.CreateNSWSub(lhs, rhs, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createLandingPad) {
        REQ_LLVM_TYPE_ARG(0, ty);
        REQ_INT_ARG(1, num_clauses);
        FALLBACK_EMPTY_UTF8_ARG(2, name);

        return LandingPad_new (_llvm_builder.CreateLandingPad(ty,
                                                              num_clauses, name));
    }

    static EJS_NATIVE_FUNC(IRBuilder_createResume) {
        REQ_LLVM_VAL_ARG(0, val);

        return Value_new (_llvm_builder.CreateResume(val));
    }

    static EJS_NATIVE_FUNC(IRBuilder_getCurrentDebugLocation) {
#if 0
        return DebugLoc_new(_llvm_builder.getCurrentDebugLocation());
#else
        return _ejs_undefined;
#endif
    }

    static EJS_NATIVE_FUNC(IRBuilder_setCurrentDebugLocation) {
#if 0
        REQ_LLVM_DEBUGLOC_ARG(0, debugloc);
        _llvm_builder.SetCurrentDebugLocation(debugloc);
#endif
        return _ejs_undefined;
    }

    void
    IRBuilder_init (ejsval exports)
    {
        _ejs_gc_add_root (&_ejs_IRBuilder_prototype);
        _ejs_IRBuilder_prototype = _ejs_object_create(_ejs_Object_prototype);

        _ejs_IRBuilder = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMIRBuilder", (EJSClosureFunc)IRBuilder_impl, _ejs_IRBuilder_prototype);

        _ejs_object_setprop_utf8 (exports,              "IRBuilder", _ejs_IRBuilder);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_IRBuilder, x, IRBuilder_##x)

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
        OBJ_METHOD(createExtractValue);
        OBJ_METHOD(createGetElementPointer);
        OBJ_METHOD(createInBoundsGetElementPointer);
        //OBJ_METHOD(createStructGetElementPointer);
        OBJ_METHOD(createICmpEq);
        OBJ_METHOD(createICmpSGt);
        OBJ_METHOD(createICmpUGE);
        OBJ_METHOD(createICmpUGt);
        OBJ_METHOD(createICmpULt);
        OBJ_METHOD(createBr);
        OBJ_METHOD(createCondBr);
        OBJ_METHOD(createPhi);
        OBJ_METHOD(createGlobalStringPtr);
        OBJ_METHOD(createUnreachable);
        OBJ_METHOD(createAnd);
        OBJ_METHOD(createOr);
        OBJ_METHOD(createTrunc);
        OBJ_METHOD(createZExt);
        OBJ_METHOD(createIntToPtr);
        OBJ_METHOD(createPtrToInt);
        OBJ_METHOD(createBitCast);

        OBJ_METHOD(createSwitch);
        OBJ_METHOD(createSelect);

        OBJ_METHOD(createNswSub);
    
        OBJ_METHOD(createLandingPad);
        OBJ_METHOD(createResume);

        OBJ_METHOD(setCurrentDebugLocation);

#undef OBJ_METHOD
    }
};
