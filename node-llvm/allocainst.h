#ifndef NODE_LLVM_ALLOCA_H
#define NODE_LLVM_ALLOCA_H

#include "node-llvm.h"
namespace jsllvm {

  class AllocaInst : public LLVMObjectWrap< ::llvm::AllocaInst, AllocaInst> {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;

  private:
    typedef LLVMObjectWrap< ::llvm::AllocaInst, AllocaInst> BaseType;
    friend class LLVMObjectWrap< ::llvm::AllocaInst, AllocaInst>;

    AllocaInst(::llvm::AllocaInst *llvm_alloca) : BaseType(llvm_alloca) { }
    AllocaInst() : BaseType(nullptr) { }
    virtual ~AllocaInst() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);
    static NAN_METHOD(SetAlignment);
  };
}

#endif /* NODE_LLVM_ALLOCA_H */
