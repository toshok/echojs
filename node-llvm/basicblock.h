#ifndef NODE_LLVM_BASICBLOCK_H
#define NODE_LLVM_BASICBLOCK_H

#include "node-llvm.h"
namespace jsllvm {


  class BasicBlock : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::BasicBlock *bb);

    static llvm::BasicBlock* GetLLVMObj (v8::Local<v8::Value> value) {
      if (value->IsNull())
	return NULL;
      return node::ObjectWrap::Unwrap<BasicBlock>(value->ToObject())->llvm_bb;
    }

  private:
    llvm::BasicBlock* llvm_bb;

    BasicBlock(llvm::BasicBlock *llvm_bb);
    BasicBlock();
    virtual ~BasicBlock();

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> GetName(v8::Local<v8::String> property, const v8::AccessorInfo& info);
    static v8::Handle<v8::Value> GetParent(v8::Local<v8::String> property, const v8::AccessorInfo& info);
  };
};

#endif /* NODE_LLVM_BASICBLOCK_H */
