#ifndef NODE_LLVM_CONSTANTARRAY_H
#define NODE_LLVM_CONSTANTARRAY_H

#include "node-llvm.h"
namespace jsllvm {


  class ConstantArray : public Nan::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

  private:
    ConstantArray() { }
    virtual ~ConstantArray() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Get);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

}

#endif /* NODE_LLVM_CONSTANTARRAY_H */
