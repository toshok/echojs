#ifndef NODE_LLVM_VALUE_H
#define NODE_LLVM_VALUE_H

#include "node-llvm.h"
namespace jsllvm {

  class Value : public LLVMObjectWrap< ::llvm::Value, Value> {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static Nan::Persistent<v8::Function> constructor_func;
    static Nan::Persistent<v8::FunctionTemplate> constructor;

  private:
    typedef LLVMObjectWrap< ::llvm::Value, Value> BaseType;
    friend class LLVMObjectWrap< ::llvm::Value, Value>;

    Value(llvm::Value *llvm_val) : BaseType(llvm_val) { }
    Value() : BaseType(nullptr) { }
    virtual ~Value() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Dump);
    static NAN_METHOD(SetName);
    static NAN_METHOD(ToString);
  };

};

#endif /* NODE_LLVM_VALUE_H */

