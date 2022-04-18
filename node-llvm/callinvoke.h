#ifndef NODE_LLVM_SWITCHINVOKE_H
#define NODE_LLVM_SWITCHINVOKE_H

#include "node-llvm.h"
namespace jsllvm {


  class Call : public LLVMObjectWrap < ::llvm::CallInst, Call> {
  public:
    static NAN_MODULE_INIT(Init);

  private:
    typedef LLVMObjectWrap< ::llvm::CallInst, Call> BaseType;
    friend class LLVMObjectWrap< ::llvm::CallInst, Call>;

    Call(::llvm::CallInst *llvm_call) : BaseType(llvm_call) { }
    Call() : BaseType(nullptr) { }
    virtual ~Call() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);
    static NAN_METHOD(SetOnlyReadsMemory);
    static NAN_METHOD(SetDoesNotAccessMemory);
    static NAN_METHOD(SetDoesNotThrow);
    static NAN_METHOD(SetStructRet);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };


  class Invoke : public LLVMObjectWrap < ::llvm::InvokeInst, Invoke> {
  public:
    static NAN_MODULE_INIT(Init);

  private:
    typedef LLVMObjectWrap< ::llvm::InvokeInst, Invoke> BaseType;
    friend class LLVMObjectWrap< ::llvm::InvokeInst, Invoke>;

    Invoke(::llvm::InvokeInst *llvm_call) : BaseType(llvm_call) { }
    Invoke() : BaseType(nullptr) { }
    virtual ~Invoke() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);
    static NAN_METHOD(SetOnlyReadsMemory);
    static NAN_METHOD(SetDoesNotAccessMemory);
    static NAN_METHOD(SetDoesNotThrow);
    static NAN_METHOD(SetStructRet);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

}

#endif /* NODE_LLVM_CALLINVOKE_H */
