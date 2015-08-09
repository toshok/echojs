#ifndef NODE_LLVM_GLOBALVARIABLE_H
#define NODE_LLVM_GLOBALVARIABLE_H

#include "node-llvm.h"
namespace jsllvm {

  class GlobalVariable : public LLVMObjectWrap< ::llvm::GlobalVariable, GlobalVariable> {
  public:
    static void Init(v8::Handle<v8::Object> target);

  private:
    typedef LLVMObjectWrap< ::llvm::GlobalVariable, GlobalVariable> BaseType;
    friend class LLVMObjectWrap< ::llvm::GlobalVariable, GlobalVariable>;

    GlobalVariable(llvm::GlobalVariable *llvm_global) : BaseType(llvm_global) { }
    GlobalVariable() : BaseType(nullptr) { }
    virtual ~GlobalVariable() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Dump);
    static NAN_METHOD(SetInitializer);
    static NAN_METHOD(ToString);
    static NAN_METHOD(SetAlignment);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

};

#endif /* NODE_LLVM_GLOBALVARIABLE_H */

