#ifndef NODE_LLVM_ALLOCA_H
#define NODE_LLVM_ALLOCA_H

#include "node-llvm.h"
namespace jsllvm {

  class AllocaInst : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(::llvm::AllocaInst *llvm_alloca);

  private:
    ::llvm::AllocaInst *llvm_alloca;

    AllocaInst(::llvm::AllocaInst *llvm_alloca);
    AllocaInst();
    virtual ~AllocaInst();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> ToString(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetAlignment(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };
};

#endif /* NODE_LLVM_ALLOCA_H */
