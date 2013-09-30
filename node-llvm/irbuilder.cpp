#include "node-llvm.h"
#include "irbuilder.h"
#include "type.h"
#include "value.h"
#include "instruction.h"
#include "landingpad.h"
#include "switch.h"
#include "callinvoke.h"
#include "basicblock.h"

using namespace node;
using namespace v8;


namespace jsllvm {

  void IRBuilder::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("IRBuilder"));

    s_func = Persistent<Function>::New(s_ct->GetFunction());

    NODE_SET_METHOD(s_func, "setInsertPoint", IRBuilder::SetInsertPoint);
    NODE_SET_METHOD(s_func, "setInsertPointStartBB", IRBuilder::SetInsertPointStartBB);
    NODE_SET_METHOD(s_func, "getInsertBlock", IRBuilder::GetInsertBlock);
    NODE_SET_METHOD(s_func, "createRet", IRBuilder::CreateRet);
    NODE_SET_METHOD(s_func, "createRetVoid", IRBuilder::CreateRetVoid);
    NODE_SET_METHOD(s_func, "createCall", IRBuilder::CreateCall);
    NODE_SET_METHOD(s_func, "createInvoke", IRBuilder::CreateInvoke);
    NODE_SET_METHOD(s_func, "createFAdd", IRBuilder::CreateFAdd);
    NODE_SET_METHOD(s_func, "createAlloca", IRBuilder::CreateAlloca);
    NODE_SET_METHOD(s_func, "createLoad", IRBuilder::CreateLoad);
    NODE_SET_METHOD(s_func, "createStore", IRBuilder::CreateStore);
    NODE_SET_METHOD(s_func, "createExtractElement", IRBuilder::CreateExtractElement);
    NODE_SET_METHOD(s_func, "createExtractValue", IRBuilder::CreateExtractValue);
    NODE_SET_METHOD(s_func, "createGetElementPointer", IRBuilder::CreateGetElementPointer);
    NODE_SET_METHOD(s_func, "createInBoundsGetElementPointer", IRBuilder::CreateInBoundsGetElementPointer);
    NODE_SET_METHOD(s_func, "createStructGetElementPointer", IRBuilder::CreateStructGetElementPointer);
    NODE_SET_METHOD(s_func, "createICmpEq", IRBuilder::CreateICmpEq);
    NODE_SET_METHOD(s_func, "createICmpSGt", IRBuilder::CreateICmpSGt);
    NODE_SET_METHOD(s_func, "createICmpUGt", IRBuilder::CreateICmpUGt);
    NODE_SET_METHOD(s_func, "createICmpULt", IRBuilder::CreateICmpULt);
    NODE_SET_METHOD(s_func, "createCondBr", IRBuilder::CreateCondBr);
    NODE_SET_METHOD(s_func, "createBr", IRBuilder::CreateBr);
    NODE_SET_METHOD(s_func, "createPhi", IRBuilder::CreatePhi);
    NODE_SET_METHOD(s_func, "createGlobalStringPtr", IRBuilder::CreateGlobalStringPtr);
    NODE_SET_METHOD(s_func, "createPointerCast", IRBuilder::CreatePointerCast);
    NODE_SET_METHOD(s_func, "createFPCast", IRBuilder::CreateFPCast);
    NODE_SET_METHOD(s_func, "createUnreachable", IRBuilder::CreateUnreachable);
    NODE_SET_METHOD(s_func, "createAnd", IRBuilder::CreateAnd);
    NODE_SET_METHOD(s_func, "createOr", IRBuilder::CreateOr);
    NODE_SET_METHOD(s_func, "createZExt", IRBuilder::CreateZExt);
    NODE_SET_METHOD(s_func, "createIntToPtr", IRBuilder::CreateIntToPtr);
    NODE_SET_METHOD(s_func, "createBitCast", IRBuilder::CreateBitCast);

    NODE_SET_METHOD(s_func, "createSwitch", IRBuilder::CreateSwitch);
    NODE_SET_METHOD(s_func, "createSelect", IRBuilder::CreateSelect);

    NODE_SET_METHOD(s_func, "createNswSub", IRBuilder::CreateNswSub);

    NODE_SET_METHOD(s_func, "createLandingPad", IRBuilder::CreateLandingPad);
    NODE_SET_METHOD(s_func, "createResume", IRBuilder::CreateResume);

    target->Set(String::NewSymbol("IRBuilder"),
		s_func);
  }

  v8::Handle<v8::Value> IRBuilder::New(const v8::Arguments& args)
  {
    return ThrowException(Exception::Error(String::New("IRBuilder is not meant to be instantiated.")));
  }

  v8::Handle<v8::Value> IRBuilder::SetInsertPoint(const v8::Arguments& args)
  {
    HandleScope scope;
    REQ_LLVM_BB_ARG(0, bb);
    if (bb != NULL)
      IRBuilder::builder.SetInsertPoint (bb);
    return scope.Close(Undefined());
  }

  v8::Handle<v8::Value> IRBuilder::SetInsertPointStartBB(const v8::Arguments& args)
  {
    HandleScope scope;
    REQ_LLVM_BB_ARG(0, bb);
    if (bb != NULL)
      IRBuilder::builder.SetInsertPoint (bb, bb->getFirstInsertionPt());
    return scope.Close(Undefined());
  }

  v8::Handle<v8::Value> IRBuilder::GetInsertBlock(const v8::Arguments& args)
  {
    HandleScope scope;
    llvm::BasicBlock *llvm_bb = IRBuilder::builder.GetInsertBlock();
    if (llvm_bb) {
      Handle<v8::Value> result = BasicBlock::New(llvm_bb);
      return scope.Close(result);
    }
    else
      return scope.Close(Null());
  }

  v8::Handle<v8::Value> IRBuilder::CreateRet(const v8::Arguments& args)
  {
    HandleScope scope;
    REQ_LLVM_VAL_ARG(0,val);
    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(builder.CreateRet(val)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateRetVoid(const v8::Arguments& args)
  {
    HandleScope scope;
    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(builder.CreateRetVoid()));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreatePointerCast(const v8::Arguments& args)
  {
    HandleScope scope;
    REQ_LLVM_VAL_ARG(0,val);
    REQ_LLVM_TYPE_ARG(1,ty);
    FALLBACK_EMPTY_UTF8_ARG(2,name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(builder.CreatePointerCast(val, ty, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateFPCast(const v8::Arguments& args)
  {
    HandleScope scope;
    REQ_LLVM_VAL_ARG(0,val);
    REQ_LLVM_TYPE_ARG(1,ty);
    FALLBACK_EMPTY_UTF8_ARG(2,name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(builder.CreateFPCast(val, ty, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateUnreachable(const v8::Arguments& args)
  {
    HandleScope scope;
    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(builder.CreateUnreachable()));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateAnd(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, lhs);
    REQ_LLVM_VAL_ARG(1, rhs);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(builder.CreateAnd(lhs, rhs, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateOr(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, lhs);
    REQ_LLVM_VAL_ARG(1, rhs);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(builder.CreateOr(lhs, rhs, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateZExt(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, V);
    REQ_LLVM_TYPE_ARG(1, dest_ty);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(builder.CreateZExt(V, dest_ty, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateIntToPtr(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, V);
    REQ_LLVM_TYPE_ARG(1, dest_ty);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(builder.CreateIntToPtr(V, dest_ty, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateBitCast(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, V);
    REQ_LLVM_TYPE_ARG(1, dest_ty);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(builder.CreateBitCast(V, dest_ty, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateCall(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, callee);
    REQ_ARRAY_ARG(1, argv);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    std::vector<llvm::Value*> ArgsV;
    for (unsigned i = 0, e = argv->Length(); i != e; ++i) {
      llvm::Value* arg = Value::GetLLVMObj(argv->Get(i));
      ArgsV.push_back(arg);
      if (ArgsV.back() == 0) abort(); // XXX throw an exception here
    }

    Handle<v8::Value> result = Call::New(IRBuilder::builder.CreateCall(callee, ArgsV, *name));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateInvoke(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, callee);
    REQ_ARRAY_ARG(1, argv);
    REQ_LLVM_BB_ARG(2, normal_dest);
    REQ_LLVM_BB_ARG(3, unwind_dest);
    FALLBACK_EMPTY_UTF8_ARG(4, name);

    std::vector<llvm::Value*> ArgsV;
    for (unsigned i = 0, e = argv->Length(); i != e; ++i) {
      llvm::Value* arg = Value::GetLLVMObj(argv->Get(i));
      ArgsV.push_back(arg);
      if (ArgsV.back() == 0) abort(); // XXX throw an exception here
    }

    Handle<v8::Value> result = Invoke::New(IRBuilder::builder.CreateInvoke(callee, normal_dest, unwind_dest, ArgsV, *name));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateFAdd(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, left);
    REQ_LLVM_VAL_ARG(1, right);
    FALLBACK_EMPTY_UTF8_ARG(2, name);
    
    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateFAdd(left, right, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateAlloca(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_TYPE_ARG(0, ty);
    FALLBACK_EMPTY_UTF8_ARG(1, name);
    
    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateAlloca(ty, 0, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateLoad(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, val);
    FALLBACK_EMPTY_UTF8_ARG(1, name);
    
    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateLoad(val, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateStore(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, val);
    REQ_LLVM_VAL_ARG(1, ptr);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateStore(val,ptr)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateExtractElement(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, val);
    REQ_LLVM_VAL_ARG(1, idx);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateExtractElement(val,idx, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateExtractValue(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, val);
    REQ_INT_ARG(1, idx);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateExtractValue(val,idx, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateGetElementPointer(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, val);
    REQ_ARRAY_ARG(1, idxv);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    std::vector<llvm::Value*> IdxV;
    for (unsigned i = 0, e = idxv->Length(); i != e; ++i) {
      llvm::Value* idx = Value::GetLLVMObj(idxv->Get(i));
      IdxV.push_back(idx);
      if (IdxV.back() == 0) abort(); // XXX throw an exception here
    }

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateGEP(val, IdxV, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateInBoundsGetElementPointer(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, val);
    REQ_ARRAY_ARG(1, idxv);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    std::vector<llvm::Value*> IdxV;
    for (unsigned i = 0, e = idxv->Length(); i != e; ++i) {
      llvm::Value* idx = Value::GetLLVMObj(idxv->Get(i));
      IdxV.push_back(idx);
      if (IdxV.back() == 0) abort(); // XXX throw an exception here
    }

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateInBoundsGEP(val, IdxV, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateStructGetElementPointer(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, val);
    REQ_INT_ARG(1, idx);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateStructGEP(val, idx, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateICmpEq(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, left);
    REQ_LLVM_VAL_ARG(1, right);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateICmpEQ(left, right, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateICmpSGt(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, left);
    REQ_LLVM_VAL_ARG(1, right);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateICmpSGT(left, right, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateICmpUGt(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, left);
    REQ_LLVM_VAL_ARG(1, right);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateICmpUGT(left, right, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateICmpULt(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, left);
    REQ_LLVM_VAL_ARG(1, right);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateICmpULT(left, right, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateBr(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_BB_ARG(0, dest);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateBr(dest)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateCondBr(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, cond);
    REQ_LLVM_BB_ARG(1, thenPart);
    REQ_LLVM_BB_ARG(2, elsePart);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateCondBr(cond, thenPart, elsePart)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreatePhi(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_TYPE_ARG(0, ty);
    REQ_INT_ARG(1, incoming_values);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreatePHI(ty, incoming_values, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateGlobalStringPtr(const v8::Arguments& args)
  {
    HandleScope scope;

    FALLBACK_EMPTY_UTF8_ARG(0, val);
    FALLBACK_EMPTY_UTF8_ARG(1, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateGlobalStringPtr(*val, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateSwitch(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, V);
    REQ_LLVM_BB_ARG(1, Dest);
    REQ_INT_ARG(2, num_cases);

    Handle<v8::Value> result = Switch::New(IRBuilder::builder.CreateSwitch(V, Dest, num_cases));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateSelect(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, C);
    REQ_LLVM_VAL_ARG(1, True);
    REQ_LLVM_VAL_ARG(2, False);
    FALLBACK_EMPTY_UTF8_ARG(3, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateSelect(C, True, False, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateNswSub(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, lhs);
    REQ_LLVM_VAL_ARG(1, rhs);
    FALLBACK_EMPTY_UTF8_ARG(2, name);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateNSWSub(lhs, rhs, *name)));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateLandingPad(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_TYPE_ARG(0, ty);
    REQ_LLVM_VAL_ARG(1, persFn);
    REQ_INT_ARG(2, num_clauses);
    FALLBACK_EMPTY_UTF8_ARG(3, name);

    Handle<v8::Value> result = LandingPad::New(IRBuilder::builder.CreateLandingPad(ty, persFn, num_clauses, *name));
    return scope.Close(result);
  }

  v8::Handle<v8::Value> IRBuilder::CreateResume(const v8::Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_VAL_ARG(0, val);

    Handle<v8::Value> result = Instruction::New(static_cast<llvm::Instruction*>(IRBuilder::builder.CreateResume(val)));
    return scope.Close(result);
  }

  llvm::IRBuilder<> IRBuilder::builder(llvm::getGlobalContext());
  Persistent<FunctionTemplate> IRBuilder::s_ct;
  Persistent< ::v8::Function> IRBuilder::s_func;
};

