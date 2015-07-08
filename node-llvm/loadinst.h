#ifndef NODE_LLVM_LOAD_H
#define NODE_LLVM_LOAD_H

#include "node-llvm.h"
namespace jsllvm {

class LoadInst : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(::llvm::LoadInst *llvm_load);

  private:
    ::llvm::LoadInst *llvm_load;

    LoadInst(::llvm::LoadInst *llvm_load);
    LoadInst();
    virtual ~LoadInst();

    static v8::Handle<v8::Value> New(const v8::Arguments &args);
    static v8::Handle<v8::Value> Dump(const v8::Arguments &args);
    static v8::Handle<v8::Value> ToString(const v8::Arguments &args);
    static v8::Handle<v8::Value> SetAlignment(const v8::Arguments &args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
};
};

#endif /* NODE_LLVM_LOAD_H */
