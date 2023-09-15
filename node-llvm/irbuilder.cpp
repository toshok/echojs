#include "node-llvm.h"
#include "irbuilder.h"
#include "constant.h"
#include "dibuilder.h"
#include "type.h"
#include "value.h"
#include "instruction.h"
#include "landingpad.h"
#include "switch.h"
#include "callinvoke.h"
#include "basicblock.h"
#include "allocainst.h"
#include "loadinst.h"

using namespace node;
using namespace v8;


namespace jsllvm {

  NAN_MODULE_INIT(IRBuilder::Init) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("IRBuilder").ToLocalChecked());

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);

    Nan::SetMethod(ctor_func, "setInsertPoint", IRBuilder::SetInsertPoint);
    Nan::SetMethod(ctor_func, "setInsertPointStartBB", IRBuilder::SetInsertPointStartBB);
    Nan::SetMethod(ctor_func, "getInsertBlock", IRBuilder::GetInsertBlock);
    Nan::SetMethod(ctor_func, "createRet", IRBuilder::CreateRet);
    Nan::SetMethod(ctor_func, "createRetVoid", IRBuilder::CreateRetVoid);
    Nan::SetMethod(ctor_func, "createCall", IRBuilder::CreateCall);
    Nan::SetMethod(ctor_func, "createInvoke", IRBuilder::CreateInvoke);
    Nan::SetMethod(ctor_func, "createFAdd", IRBuilder::CreateFAdd);
    Nan::SetMethod(ctor_func, "createAlloca", IRBuilder::CreateAlloca);
    Nan::SetMethod(ctor_func, "createLoad", IRBuilder::CreateLoad);
    Nan::SetMethod(ctor_func, "createStore", IRBuilder::CreateStore);
    Nan::SetMethod(ctor_func, "createExtractElement", IRBuilder::CreateExtractElement);
    Nan::SetMethod(ctor_func, "createExtractValue", IRBuilder::CreateExtractValue);
    Nan::SetMethod(ctor_func, "createGetElementPointer", IRBuilder::CreateGetElementPointer);
    Nan::SetMethod(ctor_func, "createInBoundsGetElementPointer", IRBuilder::CreateInBoundsGetElementPointer);
    //    Nan::SetMethod(ctor_func, "createStructGetElementPointer", IRBuilder::CreateStructGetElementPointer);
    Nan::SetMethod(ctor_func, "createICmpEq", IRBuilder::CreateICmpEq);
    Nan::SetMethod(ctor_func, "createICmpSGt", IRBuilder::CreateICmpSGt);
    Nan::SetMethod(ctor_func, "createICmpUGt", IRBuilder::CreateICmpUGt);
    Nan::SetMethod(ctor_func, "createICmpUGE", IRBuilder::CreateICmpUGE);
    Nan::SetMethod(ctor_func, "createICmpULt", IRBuilder::CreateICmpULt);
    Nan::SetMethod(ctor_func, "createCondBr", IRBuilder::CreateCondBr);
    Nan::SetMethod(ctor_func, "createBr", IRBuilder::CreateBr);
    Nan::SetMethod(ctor_func, "createPhi", IRBuilder::CreatePhi);
    Nan::SetMethod(ctor_func, "createGlobalStringPtr", IRBuilder::CreateGlobalStringPtr);
    Nan::SetMethod(ctor_func, "createPointerCast", IRBuilder::CreatePointerCast);
    Nan::SetMethod(ctor_func, "createFPCast", IRBuilder::CreateFPCast);
    Nan::SetMethod(ctor_func, "createUnreachable", IRBuilder::CreateUnreachable);
    Nan::SetMethod(ctor_func, "createAnd", IRBuilder::CreateAnd);
    Nan::SetMethod(ctor_func, "createOr", IRBuilder::CreateOr);
    Nan::SetMethod(ctor_func, "createTrunc", IRBuilder::CreateTrunc);
    Nan::SetMethod(ctor_func, "createZExt", IRBuilder::CreateZExt);
    Nan::SetMethod(ctor_func, "createIntToPtr", IRBuilder::CreateIntToPtr);
    Nan::SetMethod(ctor_func, "createPtrToInt", IRBuilder::CreatePtrToInt);
    Nan::SetMethod(ctor_func, "createBitCast", IRBuilder::CreateBitCast);

    Nan::SetMethod(ctor_func, "createSwitch", IRBuilder::CreateSwitch);
    Nan::SetMethod(ctor_func, "createSelect", IRBuilder::CreateSelect);

    Nan::SetMethod(ctor_func, "createNswSub", IRBuilder::CreateNswSub);

    Nan::SetMethod(ctor_func, "createLandingPad", IRBuilder::CreateLandingPad);
    Nan::SetMethod(ctor_func, "createResume", IRBuilder::CreateResume);

    Nan::SetMethod(ctor_func, "createLifetimeStart", IRBuilder::CreateLifetimeStart);
    Nan::SetMethod(ctor_func, "createLifetimeEnd", IRBuilder::CreateLifetimeEnd);

    Nan::SetMethod(ctor_func, "getCurrentDebugLocation", IRBuilder::GetCurrentDebugLocation);
    Nan::SetMethod(ctor_func, "setCurrentDebugLocation", IRBuilder::SetCurrentDebugLocation);

    target->Set(context, Nan::New("IRBuilder").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(IRBuilder::New) {
    Nan::ThrowTypeError("IRBuilder is not meant to be instantiated.");
  }

  NAN_METHOD(IRBuilder::SetInsertPoint) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Nan::HandleScope scope;
    REQ_LLVM_BB_ARG(context, 0, bb);
    if (bb != NULL) {
      IRBuilder::builder.SetInsertPoint (bb);
    }
  }

  NAN_METHOD(IRBuilder::SetInsertPointStartBB) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Nan::HandleScope scope;
    REQ_LLVM_BB_ARG(context, 0, bb);
    if (bb != NULL) {
      IRBuilder::builder.SetInsertPoint (bb, bb->getFirstInsertionPt());
    }
  }

  NAN_METHOD(IRBuilder::GetInsertBlock) {
    Nan::HandleScope scope;
    llvm::BasicBlock *llvm_bb = IRBuilder::builder.GetInsertBlock();
    if (llvm_bb) {
      info.GetReturnValue().Set(BasicBlock::Create(llvm_bb));
      return;
    }

    info.GetReturnValue().Set(Nan::Null());
  }

  NAN_METHOD(IRBuilder::CreateRet) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;
    REQ_LLVM_VAL_ARG(context, 0,val);
    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreateRet(val)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateRetVoid) {
    Nan::HandleScope scope;
    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreateRetVoid()));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreatePointerCast) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;
    REQ_LLVM_VAL_ARG(context, 0,val);
    REQ_LLVM_TYPE_ARG(context, 1,ty);
    FALLBACK_EMPTY_UTF8_ARG(context, 2,name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreatePointerCast(val, ty, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateFPCast) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;
    REQ_LLVM_VAL_ARG(context, 0,val);
    REQ_LLVM_TYPE_ARG(context, 1,ty);
    FALLBACK_EMPTY_UTF8_ARG(context, 2,name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreateFPCast(val, ty, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateUnreachable) {
    Nan::HandleScope scope;
    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreateUnreachable()));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateAnd) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, lhs);
    REQ_LLVM_VAL_ARG(context, 1, rhs);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreateAnd(lhs, rhs, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateOr) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, lhs);
    REQ_LLVM_VAL_ARG(context, 1, rhs);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreateOr(lhs, rhs, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateTrunc) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, V);
    REQ_LLVM_TYPE_ARG(context, 1, dest_ty);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreateTrunc(V, dest_ty, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateZExt) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, V);
    REQ_LLVM_TYPE_ARG(context, 1, dest_ty);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreateZExt(V, dest_ty, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateIntToPtr) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, V);
    REQ_LLVM_TYPE_ARG(context, 1, dest_ty);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreateIntToPtr(V, dest_ty, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreatePtrToInt) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, V);
    REQ_LLVM_TYPE_ARG(context, 1, dest_ty);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreatePtrToInt(V, dest_ty, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateBitCast) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, V);
    REQ_LLVM_TYPE_ARG(context, 1, dest_ty);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(builder.CreateBitCast(V, dest_ty, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateCall) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_TYPE_ARG(context, 0, type);
    REQ_LLVM_VAL_ARG(context, 1, callee);
    REQ_ARRAY_ARG(context, 2, argv);
    FALLBACK_EMPTY_UTF8_ARG(context, 3, name);

    std::vector<llvm::Value*> ArgsV;
    for (unsigned i = 0, e = argv->Length(); i != e; ++i) {
      llvm::Value* arg = Value::GetLLVMObj(context, argv->Get(context, i).ToLocalChecked());
      ArgsV.push_back(arg);
      assert(ArgsV.back() != 0); // XXX throw an exception here
    }

    auto FT = static_cast<llvm::FunctionType*>(type); // XXX need to make this safe...
    Local<v8::Value> result = Call::Create(IRBuilder::builder.CreateCall(FT, callee, ArgsV, *name));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateInvoke) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_TYPE_ARG(context, 0, type);
    REQ_LLVM_VAL_ARG(context, 1, callee);
    REQ_ARRAY_ARG(context, 2, argv);
    REQ_LLVM_BB_ARG(context, 3, normal_dest);
    REQ_LLVM_BB_ARG(context, 4, unwind_dest);
    FALLBACK_EMPTY_UTF8_ARG(context, 5, name);

    std::vector<llvm::Value*> ArgsV;
    for (unsigned i = 0, e = argv->Length(); i != e; ++i) {
      llvm::Value* arg = Value::GetLLVMObj(context, argv->Get(context, i).ToLocalChecked());
      ArgsV.push_back(arg);
      assert(ArgsV.back() != 0); // XXX throw an exception here
    }

    auto FT = static_cast<llvm::FunctionType*>(type); // XXX need to make this safe...
    Local<v8::Value> result = Invoke::Create(IRBuilder::builder.CreateInvoke(FT, callee, normal_dest, unwind_dest, ArgsV, *name));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateFAdd) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, left);
    REQ_LLVM_VAL_ARG(context, 1, right);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);
    
    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateFAdd(left, right, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateAlloca) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_TYPE_ARG(context, 0, ty);
    FALLBACK_EMPTY_UTF8_ARG(context, 1, name);
    
    Local<v8::Value> result = AllocaInst::Create(IRBuilder::builder.CreateAlloca(ty, 0, *name));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateLoad) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_TYPE_ARG(context, 0, ty);
    REQ_LLVM_VAL_ARG(context, 1, val);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);
    
    Local<v8::Value> result = LoadInst::Create(IRBuilder::builder.CreateLoad(ty, val, *name));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateStore) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, val);
    REQ_LLVM_VAL_ARG(context, 1, ptr);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateStore(val,ptr)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateExtractElement) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, val);
    REQ_LLVM_VAL_ARG(context, 1, idx);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateExtractElement(val,idx, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateExtractValue) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, val);
    REQ_INT_ARG(context, 1, idx);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateExtractValue(val,idx, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateGetElementPointer) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Nan::HandleScope scope;

    REQ_LLVM_TYPE_ARG(context, 0, ty);
    REQ_LLVM_VAL_ARG(context, 1, val);
    REQ_ARRAY_ARG(context, 2, idxv);
    FALLBACK_EMPTY_UTF8_ARG(context, 3, name);

    std::vector<llvm::Value*> IdxV;
    for (unsigned i = 0, e = idxv->Length(); i != e; ++i) {
      llvm::Value* idx = Value::GetLLVMObj(context, idxv->Get(context, i).ToLocalChecked());
      IdxV.push_back(idx);
      assert(IdxV.back() != 0); // XXX throw an exception here
    }

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateGEP(ty, val, IdxV, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateInBoundsGetElementPointer) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_TYPE_ARG(context, 0, ty);
    REQ_LLVM_VAL_ARG(context, 1, val);
    REQ_ARRAY_ARG(context, 2, idxv);
    FALLBACK_EMPTY_UTF8_ARG(context, 3, name);

    std::vector<llvm::Value*> IdxV;
    for (unsigned i = 0, e = idxv->Length(); i != e; ++i) {
      llvm::Value* idx = Value::GetLLVMObj(context, idxv->Get(context, i).ToLocalChecked());
      IdxV.push_back(idx);
      assert(IdxV.back() != 0); // XXX throw an exception here
    }

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateInBoundsGEP(ty, val, IdxV, *name)));
    info.GetReturnValue().Set(result);
  }

  /*
  NAN_METHOD(IRBuilder::CreateStructGetElementPointer) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, val);
    REQ_INT_ARG(context, 1, idx);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateStructGEP(val, idx, *name)));
    info.GetReturnValue().Set(result);
  }
  */
  
  NAN_METHOD(IRBuilder::CreateICmpEq) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, left);
    REQ_LLVM_VAL_ARG(context, 1, right);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateICmpEQ(left, right, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateICmpSGt) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, left);
    REQ_LLVM_VAL_ARG(context, 1, right);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateICmpSGT(left, right, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateICmpUGt) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, left);
    REQ_LLVM_VAL_ARG(context, 1, right);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateICmpUGT(left, right, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateICmpUGE) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, left);
    REQ_LLVM_VAL_ARG(context, 1, right);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateICmpUGE(left, right, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateICmpULt) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, left);
    REQ_LLVM_VAL_ARG(context, 1, right);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateICmpULT(left, right, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateBr) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_BB_ARG(context, 0, dest);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateBr(dest)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateCondBr) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, cond);
    REQ_LLVM_BB_ARG(context, 1, thenPart);
    REQ_LLVM_BB_ARG(context, 2, elsePart);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateCondBr(cond, thenPart, elsePart)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreatePhi) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Nan::HandleScope scope;

    REQ_LLVM_TYPE_ARG(context, 0, ty);
    REQ_INT_ARG(context, 1, incoming_values);
    FALLBACK_EMPTY_UTF8_ARG(context,  2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreatePHI(ty, incoming_values, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateGlobalStringPtr) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    Nan::HandleScope scope;

    FALLBACK_EMPTY_UTF8_ARG(context, 0, val);
    FALLBACK_EMPTY_UTF8_ARG(context, 1, name);

    Local<v8::Value> result = Constant::Create(IRBuilder::builder.CreateGlobalStringPtr(*val, *name));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateSwitch) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, V);
    REQ_LLVM_BB_ARG(context, 1, Dest);
    REQ_INT_ARG(context, 2, num_cases);

    Local<v8::Value> result = Switch::Create(IRBuilder::builder.CreateSwitch(V, Dest, num_cases));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateSelect) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, C);
    REQ_LLVM_VAL_ARG(context, 1, True);
    REQ_LLVM_VAL_ARG(context, 2, False);
    FALLBACK_EMPTY_UTF8_ARG(context, 3, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateSelect(C, True, False, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateNswSub) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, lhs);
    REQ_LLVM_VAL_ARG(context, 1, rhs);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateNSWSub(lhs, rhs, *name)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateLandingPad) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Nan::HandleScope scope;

    REQ_LLVM_TYPE_ARG(context, 0, ty);
    REQ_INT_ARG(context, 1, num_clauses);
    FALLBACK_EMPTY_UTF8_ARG(context, 2, name);

    Local<v8::Value> result = LandingPad::Create(IRBuilder::builder.CreateLandingPad(ty,
										     num_clauses, *name));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateResume) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, val);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateResume(val)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateLifetimeStart) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, val);
    REQ_LLVM_CONST_INT_ARG(context, 1, size);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateLifetimeStart(val, size)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::CreateLifetimeEnd) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;

    REQ_LLVM_VAL_ARG(context, 0, val);
    REQ_LLVM_CONST_INT_ARG(context, 1, size);

    Local<v8::Value> result = Instruction::Create(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateLifetimeEnd(val, size)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(IRBuilder::GetCurrentDebugLocation) {
#if false
    Nan::HandleScope scope;

    Local<v8::Value> result = DebugLoc::Create(IRBuilder::builder.getCurrentDebugLocation());
    info.GetReturnValue().Set(result);
#endif
  }

  NAN_METHOD(IRBuilder::SetCurrentDebugLocation) {
#if false
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    Nan::HandleScope scope;
    REQ_LLVM_DEBUGLOC_ARG(context, 0, debugloc);
    IRBuilder::builder.SetCurrentDebugLocation(debugloc);
#endif
  }

  llvm::IRBuilder<> IRBuilder::builder(TheContext);
  Nan::Persistent<v8::FunctionTemplate> IRBuilder::constructor;
  Nan::Persistent<v8::Function> IRBuilder::constructor_func;
}

