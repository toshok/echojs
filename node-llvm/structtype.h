#ifndef NODE_LLVM_STRUCTTYPE_H
#define NODE_LLVM_STRUCTTYPE_H

#include "node-llvm.h"
namespace jsllvm {

  class StructType : public LLVMObjectWrap< ::llvm::StructType, StructType> {
  public:
    static NAN_MODULE_INIT(Init);

  private:
    typedef LLVMObjectWrap< ::llvm::StructType, StructType> BaseType;
    friend class LLVMObjectWrap< ::llvm::StructType, StructType>;

    StructType(llvm::StructType *llvm_ty) : BaseType(llvm_ty) { }
    StructType() : BaseType(nullptr) { }
    virtual ~StructType() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Create);
    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

}

#endif /* NODE_LLVM_STRUCTTYPE_H */
