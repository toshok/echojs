#ifndef NODE_LLVM_ARRAYTYPE_H
#define NODE_LLVM_ARRAYTYPE_H

#include "node-llvm.h"
namespace jsllvm {


  class ArrayType : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::ArrayType *ty);

    static llvm::ArrayType* GetLLVMObj (v8::Local<v8::Value> value) {
      if (value->IsNull())
	return NULL;
      return node::ObjectWrap::Unwrap<ArrayType>(value->ToObject())->llvm_ty;
    }

  private:
    llvm::ArrayType* llvm_ty;

    ArrayType(llvm::ArrayType *llvm_ty);
    ArrayType();
    virtual ~ArrayType();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Get(const v8::Arguments& args);

    static v8::Handle<v8::Value> Dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> ToString(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

};

#endif /* NODE_LLVM_ARRAYTYPE_H */
