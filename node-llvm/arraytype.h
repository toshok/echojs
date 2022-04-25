#ifndef NODE_LLVM_ARRAYTYPE_H
#define NODE_LLVM_ARRAYTYPE_H

#include "node-llvm.h"
namespace jsllvm {


  class ArrayType : public LLVMObjectWrap< ::llvm::ArrayType, ArrayType> {
  public:
    static NAN_MODULE_INIT(Init);

  private:
    typedef LLVMObjectWrap< ::llvm::ArrayType, ArrayType> BaseType;
    friend class LLVMObjectWrap< ::llvm::ArrayType, ArrayType>;

    ArrayType(llvm::ArrayType *llvm_ty) : BaseType(llvm_ty) { }
    ArrayType() : BaseType(nullptr) { }
    virtual ~ArrayType() { }

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;

    static NAN_METHOD(New);
    static NAN_METHOD(Get);
    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);

  };

}

#endif /* NODE_LLVM_ARRAYTYPE_H */
