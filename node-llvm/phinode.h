#ifndef NODE_LLVM_PHINODE_H
#define NODE_LLVM_PHINODE_H

#include "node-llvm.h"
namespace jsllvm {

  class PHINode : public LLVMObjectWrap< ::llvm::PHINode, PHINode> {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Local<v8::Value> Create(::llvm::PHINode *llvm_phi);

  private:
    typedef LLVMObjectWrap< ::llvm::PHINode, PHINode> BaseType;
    friend class LLVMObjectWrap< ::llvm::PHINode, PHINode>;

    PHINode(::llvm::PHINode *llvm_phi) : BaseType(llvm_phi) { }
    PHINode() : BaseType(nullptr) { }
    virtual ~PHINode() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);
    static NAN_METHOD(AddIncoming);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };
}

#endif /* NODE_LLVM_PHINODE_H */
