#ifndef NODE_LLVM_CONSTANT_H
#define NODE_LLVM_CONSTANT_H

#include "node-llvm.h"
namespace jsllvm {


  class Constant : public LLVMObjectWrap< ::llvm::Constant, Constant> {
  public:
    static NAN_MODULE_INIT(Init);

  private:
    typedef LLVMObjectWrap< ::llvm::Constant, Constant> BaseType;
    friend class LLVMObjectWrap< ::llvm::Constant, Constant>;

    Constant(llvm::Constant *llvm_val) : BaseType(llvm_val) { }
    Constant() : BaseType(nullptr) { }
    virtual ~Constant() {}

    static NAN_METHOD(New);
    static NAN_METHOD(GetNull);
    static NAN_METHOD(GetAggregateZero);
    static NAN_METHOD(GetBoolValue);
    static NAN_METHOD(GetIntegerValue);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

}

#endif /* NODE_LLVM_CONSTANT_H */
