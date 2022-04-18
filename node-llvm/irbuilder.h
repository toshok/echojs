#ifndef NODE_LLVM_IRBUILDER_H
#define NODE_LLVM_IRBUILDER_H

#include "node-llvm.h"
namespace jsllvm {


  class IRBuilder : public Nan::ObjectWrap {
  public:
    static NAN_MODULE_INIT(Init);

    static v8::Local<v8::Value> Create(llvm::IRBuilder<> *builder);

  private:
    static ::llvm::IRBuilder<> builder;

    static NAN_METHOD(New);
    static NAN_METHOD(SetInsertPoint);
    static NAN_METHOD(SetInsertPointStartBB);
    static NAN_METHOD(GetInsertBlock);
    static NAN_METHOD(CreateRet);
    static NAN_METHOD(CreateRetVoid);
    static NAN_METHOD(CreatePointerCast);
    static NAN_METHOD(CreateFPCast);
    static NAN_METHOD(CreateCall);
    static NAN_METHOD(CreateInvoke);
    static NAN_METHOD(CreateFAdd);
    static NAN_METHOD(CreateAlloca);
    static NAN_METHOD(CreateLoad);
    static NAN_METHOD(CreateStore);
    static NAN_METHOD(CreateExtractElement);
    static NAN_METHOD(CreateExtractValue);
    static NAN_METHOD(CreateGetElementPointer);
    static NAN_METHOD(CreateInBoundsGetElementPointer);
    static NAN_METHOD(CreateStructGetElementPointer);
    static NAN_METHOD(CreateICmpEq);
    static NAN_METHOD(CreateICmpSGt);
    static NAN_METHOD(CreateICmpUGE);
    static NAN_METHOD(CreateICmpUGt);
    static NAN_METHOD(CreateICmpULt);
    static NAN_METHOD(CreateBr);
    static NAN_METHOD(CreateCondBr);
    static NAN_METHOD(CreatePhi);
    static NAN_METHOD(CreateGlobalStringPtr);
    static NAN_METHOD(CreateUnreachable);
    static NAN_METHOD(CreateAnd);
    static NAN_METHOD(CreateOr);
    static NAN_METHOD(CreateTrunc);
    static NAN_METHOD(CreateZExt);
    static NAN_METHOD(CreateIntToPtr);
    static NAN_METHOD(CreatePtrToInt);
    static NAN_METHOD(CreateBitCast);

    static NAN_METHOD(CreateSwitch);
    static NAN_METHOD(CreateSelect);

    static NAN_METHOD(CreateNswSub);

    static NAN_METHOD(CreateLandingPad);
    static NAN_METHOD(CreateResume);

    static NAN_METHOD(CreateLifetimeStart);
    static NAN_METHOD(CreateLifetimeEnd);

    static NAN_METHOD(GetCurrentDebugLocation);
    static NAN_METHOD(SetCurrentDebugLocation);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

}


#endif /* NODE_LLVM_IRBUILDER_H */
