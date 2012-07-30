#ifndef NODE_LLVM_MODULE_H
#define NODE_LLVM_MODULE_H

#include "node-llvm.h"

namespace jsllvm {


  class Module : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(::llvm::Module *llvm_module);
  private:
    ::llvm::Module *llvm_module;

    Module(::llvm::Module *llvm_module);
    Module();
    virtual ~Module();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> GetOrInsertFunction (const v8::Arguments& args);
    static v8::Handle<v8::Value> GetOrInsertExternalFunction (const v8::Arguments& args);
    static v8::Handle<v8::Value> GetFunction (const v8::Arguments& args);
    static v8::Handle<v8::Value> Dump (const v8::Arguments& args);
    static v8::Handle<v8::Value> WriteToFile (const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };
};

#endif /* NODE_LLVM_MODULE_H */
