#ifndef NODE_LLVM_FUNCTIONTYPE_H
#define NODE_LLVM_FUNCTIONTYPE_H

#include "node-llvm.h"
namespace jsllvm {


  class FunctionType : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::FunctionType *ty);

    static llvm::FunctionType* GetLLVMObj (v8::Local<v8::Value> value) {
      if (value->IsNull())
	return NULL;
      return node::ObjectWrap::Unwrap<FunctionType>(value->ToObject())->llvm_ty;
    }

  private:
    llvm::FunctionType* llvm_ty;

    FunctionType(llvm::FunctionType *llvm_ty);
    FunctionType();
    virtual ~FunctionType();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Get(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

};

#endif /* NODE_LLVM_FUNCTIONTYPE_H */
