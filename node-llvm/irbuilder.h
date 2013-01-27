#ifndef NODE_LLVM_IRBUILDER_H
#define NODE_LLVM_IRBUILDER_H

#include "node-llvm.h"
namespace jsllvm {


  class IRBuilder : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::IRBuilder<> *builder);

  private:
    static ::llvm::IRBuilder<> builder;

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetInsertPoint(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetInsertPointStartBB(const v8::Arguments& args);
    static v8::Handle<v8::Value> GetInsertBlock(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateRet(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateRetVoid(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreatePointerCast(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateFPCast(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateCall(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateInvoke(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateFAdd(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateAlloca(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateLoad(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateStore(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateExtractElement(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateGetElementPointer(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateInBoundsGetElementPointer(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateStructGetElementPointer(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateICmpEq(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateICmpSGt(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateBr(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateCondBr(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreatePhi(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateGlobalStringPtr(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateUnreachable(const v8::Arguments& args);

    static v8::Handle<v8::Value> CreateSwitch(const v8::Arguments& args);

    static v8::Handle<v8::Value> CreateLandingPad(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateResume(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

};


#endif /* NODE_LLVM_IRBUILDER_H */
