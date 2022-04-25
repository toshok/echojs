#ifndef NODE_LLVM_METADATA_H
#define NODE_LLVM_METADATA_H

#include "node-llvm.h"
namespace jsllvm {

  class MDString : public Nan::ObjectWrap {
  public:
    static NAN_MODULE_INIT(Init);

    static llvm::MDString* GetLLVMObj (v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<MDString>(value->ToObject(context).ToLocalChecked())->llvm_mdstring;
    }

  private:
    llvm::MDString* llvm_mdstring;

    MDString(llvm::MDString* llvm_mdstring);
    virtual ~MDString();

    static v8::Local<v8::Value> Create(llvm::MDString* llvm_mdstring);

    static NAN_METHOD(New);
    static NAN_METHOD(Get);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

  class MDNode : public Nan::ObjectWrap {
  public:
    static NAN_MODULE_INIT(Init);

    static llvm::MDNode* GetLLVMObj (v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<MDNode>(value->ToObject(context).ToLocalChecked())->llvm_mdnode;
    }

  private:
    llvm::MDNode* llvm_mdnode;

    MDNode(llvm::MDNode* llvm_mdnode);
    virtual ~MDNode();

    static v8::Local<v8::Value> Create(llvm::MDNode* llvm_mdnode);

    static NAN_METHOD(New);
    static NAN_METHOD(Get);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

}

#endif /* NODE_LLVM_METADATA_H */
