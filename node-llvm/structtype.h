#ifndef NODE_LLVM_STRUCTTYPE_H
#define NODE_LLVM_STRUCTTYPE_H

#include "node-llvm.h"
namespace jsllvm {


  class StructType : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::StructType *ty);

    static llvm::StructType* GetLLVMObj (v8::Local<v8::Value> value) {
      if (value->IsNull())
	return NULL;
      return node::ObjectWrap::Unwrap<StructType>(value->ToObject())->llvm_ty;
    }

  private:
    llvm::StructType* llvm_ty;

    StructType(llvm::StructType *llvm_ty);
    StructType();
    virtual ~StructType();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Create(const v8::Arguments& args);

    static v8::Handle<v8::Value> Dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> ToString(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

};

#endif /* NODE_LLVM_STRUCTTYPE_H */
