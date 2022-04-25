#ifndef NODE_LLVM_LOAD_H
#define NODE_LLVM_LOAD_H

#include "node-llvm.h"
namespace jsllvm {

  class LoadInst : public LLVMObjectWrap< ::llvm::LoadInst, LoadInst> {
  public:
    static NAN_MODULE_INIT(Init);

  private:
    typedef LLVMObjectWrap< ::llvm::LoadInst, LoadInst> BaseType;
    friend class LLVMObjectWrap< ::llvm::LoadInst, LoadInst>;

    LoadInst(::llvm::LoadInst *llvm_load) : BaseType(llvm_load) { }
    LoadInst() : BaseType(nullptr) { }
    virtual ~LoadInst() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);
    static NAN_METHOD(SetAlignment);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };
}

#endif /* NODE_LLVM_LOAD_H */
