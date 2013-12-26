#ifndef NODE_LLVM_SWITCHINVOKE_H
#define NODE_LLVM_SWITCHINVOKE_H

#include "node-llvm.h"
namespace jsllvm {


  class Call : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(::llvm::CallInst *llvm_call);

  private:
    ::llvm::CallInst *llvm_call;

    Call(::llvm::CallInst *llvm_call);
    Call();
    virtual ~Call();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> ToString(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetOnlyReadsMemory(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetDoesNotAccessMemory(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetDoesNotThrow(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetStructRet (const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };


  class Invoke : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(::llvm::InvokeInst *llvm_invoke);

  private:
    ::llvm::InvokeInst *llvm_invoke;

    Invoke(::llvm::InvokeInst *llvm_invoke);
    Invoke();
    virtual ~Invoke();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> ToString(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetOnlyReadsMemory(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetDoesNotAccessMemory(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetDoesNotThrow(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetStructRet (const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

};

#endif /* NODE_LLVM_CALLINVOKE_H */
