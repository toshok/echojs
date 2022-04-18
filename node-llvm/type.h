#ifndef NODE_LLVM_TYPE_H
#define NODE_LLVM_TYPE_H

#include "node-llvm.h"
namespace jsllvm {


  class Type : public LLVMObjectWrap< ::llvm::Type, Type> {
  public:
    static NAN_MODULE_INIT(Init);

  private:
    typedef LLVMObjectWrap< ::llvm::Type, Type> BaseType;
    friend class LLVMObjectWrap< ::llvm::Type, Type>;

    Type(llvm::Type *llvm_ty) : BaseType(llvm_ty) { }
    Type() : BaseType(nullptr) { }
    virtual ~Type() { }

    static NAN_METHOD(getDoubleTy);
    static NAN_METHOD(getInt64Ty);
    static NAN_METHOD(getInt32Ty);
    static NAN_METHOD(getInt16Ty);
    static NAN_METHOD(getInt8Ty);
    static NAN_METHOD(getInt1Ty);
    static NAN_METHOD(getVoidTy);

    static NAN_METHOD(New);
    static NAN_METHOD(isVoid);
    static NAN_METHOD(dump);
    static NAN_METHOD(ToString);

    static NAN_METHOD(pointerTo);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;

    friend class FunctionType;
    friend class StructType;
    friend class ArrayType;
  };

}

#endif /* NODE_LLVM_TYPE_H */
