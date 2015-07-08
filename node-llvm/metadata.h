#ifndef NODE_LLVM_METADATA_H
#define NODE_LLVM_METADATA_H

#include "node-llvm.h"
namespace jsllvm {

class MDString : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static llvm::MDString *GetLLVMObj(v8::Local<v8::Value> value) {
        return node::ObjectWrap::Unwrap<MDString>(value->ToObject())
            ->llvm_mdstring;
    }

  private:
    llvm::MDString *llvm_mdstring;

    MDString(llvm::MDString *llvm_mdstring);
    virtual ~MDString();

    static v8::Handle<v8::Value> New(llvm::MDString *llvm_mdstring);
    static v8::Handle<v8::Value> New(const v8::Arguments &args);

    static v8::Handle<v8::Value> Get(const v8::Arguments &args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
};

class MDNode : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static llvm::MDNode *GetLLVMObj(v8::Local<v8::Value> value) {
        return node::ObjectWrap::Unwrap<MDNode>(value->ToObject())->llvm_mdnode;
    }

  private:
    llvm::MDNode *llvm_mdnode;

    MDNode(llvm::MDNode *llvm_mdnode);
    virtual ~MDNode();

    static v8::Handle<v8::Value> New(llvm::MDNode *llvm_mdnode);
    static v8::Handle<v8::Value> New(const v8::Arguments &args);

    static v8::Handle<v8::Value> Get(const v8::Arguments &args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
};
};

#endif /* NODE_LLVM_METADATA_H */
