#ifndef NODE_LLVM_VALUE_H
#define NODE_LLVM_VALUE_H

#include "node-llvm.h"
namespace jsllvm {

  class Value : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::Value *llvm_val);

    static llvm::Value* GetLLVMObj (v8::Local<v8::Value> value) {
      if (value->IsNull())
	return NULL;
      return node::ObjectWrap::Unwrap<Value>(value->ToObject())->llvm_val;
    }

  private:
    llvm::Value* llvm_val;

    Value(llvm::Value *llvm_val);
    Value();
    virtual ~Value();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetName(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

};

#endif /* NODE_LLVM_TYPE_H */

