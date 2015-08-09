#ifndef NODE_LLVM_BASICBLOCK_H
#define NODE_LLVM_BASICBLOCK_H

#include "node-llvm.h"
namespace jsllvm {


  class BasicBlock : public LLVMObjectWrap < ::llvm::BasicBlock, BasicBlock> {
  public:
    static void Init(v8::Handle<v8::Object> target);

  private:
    typedef LLVMObjectWrap< ::llvm::BasicBlock, BasicBlock> BaseType;
    friend class LLVMObjectWrap< ::llvm::BasicBlock, BasicBlock>;

    BasicBlock(llvm::BasicBlock *llvm_bb) : BaseType(llvm_bb) { }
    BasicBlock() : BaseType(nullptr) { }
    virtual ~BasicBlock() { }

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;

    static NAN_METHOD(New);
    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);
    static NAN_GETTER(GetName);
    static NAN_GETTER(GetParent);
  };
};

#endif /* NODE_LLVM_BASICBLOCK_H */
