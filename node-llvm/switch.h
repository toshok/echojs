#ifndef NODE_LLVM_SWITCH_H
#define NODE_LLVM_SWITCH_H

#include "node-llvm.h"
namespace jsllvm {


  class Switch : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(::llvm::SwitchInst *llvm_switch);

  private:
    ::llvm::SwitchInst *llvm_switch;

    Switch(::llvm::SwitchInst *llvm_switch);
    Switch();
    virtual ~Switch();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> ToString(const v8::Arguments& args);
    static v8::Handle<v8::Value> AddCase(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };
};

#endif /* NODE_LLVM_SWITCH_H */
