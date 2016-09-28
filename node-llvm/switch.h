#ifndef NODE_LLVM_SWITCH_H
#define NODE_LLVM_SWITCH_H

#include "node-llvm.h"
namespace jsllvm {

  class Switch : public LLVMObjectWrap< ::llvm::SwitchInst, Switch> {
  public:
    static void Init(v8::Handle<v8::Object> target);

  private:
    typedef LLVMObjectWrap< ::llvm::SwitchInst, Switch> BaseType;
    friend class LLVMObjectWrap< ::llvm::SwitchInst, Switch>;

    Switch(::llvm::SwitchInst *llvm_switch) : BaseType(llvm_switch) { }
    Switch() : BaseType(nullptr) { }
    virtual ~Switch() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);
    static NAN_METHOD(AddCase);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };
}

#endif /* NODE_LLVM_SWITCH_H */
