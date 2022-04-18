#ifndef NODE_LLVM_MODULE_H
#define NODE_LLVM_MODULE_H

#include "node-llvm.h"

namespace jsllvm {

  class Module : public LLVMObjectWrap< ::llvm::Module, Module> {
  public:
    static NAN_MODULE_INIT(Init);

  private:
    typedef LLVMObjectWrap< ::llvm::Module, Module> BaseType;
    friend class LLVMObjectWrap< ::llvm::Module, Module>;

    Module(::llvm::Module *llvm_module) : BaseType(llvm_module) { }
    Module() : BaseType(nullptr) { }
    virtual ~Module() { }

    static NAN_METHOD(New);
    static NAN_METHOD(GetGlobalVariable);
    static NAN_METHOD(GetOrInsertIntrinsic);
    static NAN_METHOD(GetOrInsertFunction);
    static NAN_METHOD(GetOrInsertGlobal);
    static NAN_METHOD(GetOrInsertExternalFunction);
    static NAN_METHOD(GetFunction);
    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);
    static NAN_METHOD(WriteToFile);
    static NAN_METHOD(WriteBitcodeToFile);

    static NAN_METHOD(AddModuleFlag);

    static NAN_METHOD(SetDataLayout);
    static NAN_METHOD(SetTriple);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };
}

#endif /* NODE_LLVM_MODULE_H */
