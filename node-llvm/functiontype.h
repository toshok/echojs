#ifndef NODE_LLVM_FUNCTIONTYPE_H
#define NODE_LLVM_FUNCTIONTYPE_H

#include "node-llvm.h"
namespace jsllvm {


  class FunctionType : public LLVMObjectWrap< ::llvm::FunctionType, FunctionType> {
  public:
    static NAN_MODULE_INIT(Init);

  private:
    typedef LLVMObjectWrap< ::llvm::FunctionType, FunctionType> BaseType;
    friend class LLVMObjectWrap< ::llvm::FunctionType, FunctionType>;

    FunctionType(llvm::FunctionType *llvm_ty) : BaseType(llvm_ty) { }
    FunctionType() : BaseType(nullptr) { }
    virtual ~FunctionType() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Get);

    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);
    static NAN_METHOD(GetReturnType);
    static NAN_METHOD(GetParamType);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

}

#endif /* NODE_LLVM_FUNCTIONTYPE_H */
