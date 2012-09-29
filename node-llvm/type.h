#ifndef NODE_LLVM_TYPE_H
#define NODE_LLVM_TYPE_H

#include "node-llvm.h"
namespace jsllvm {


  class Type : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::Type *ty);

    static llvm::Type* GetLLVMObj (v8::Local<v8::Value> value) {
      if (value->IsNull())
	return NULL;
      return node::ObjectWrap::Unwrap<Type>(value->ToObject())->llvm_ty;
    }

  private:
    llvm::Type* llvm_ty;

    Type(llvm::Type *llvm_ty);
    Type();
    virtual ~Type();

#define LLVM_TYPE_METHOD(name) static v8::Handle<v8::Value> name(const v8::Arguments& args)

    LLVM_TYPE_METHOD(getDoubleTy);
    LLVM_TYPE_METHOD(getInt64Ty);
    LLVM_TYPE_METHOD(getInt32Ty);
    LLVM_TYPE_METHOD(getInt8Ty);
    LLVM_TYPE_METHOD(getVoidTy);

#undef LLVM_TYPE_METHOD

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> isVoid(const v8::Arguments& args);
    static v8::Handle<v8::Value> dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> ToString(const v8::Arguments& args);

    static v8::Handle<v8::Value> pointerTo(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;

    friend class FunctionType;
    friend class StructType;
  };

};

#endif /* NODE_LLVM_TYPE_H */
