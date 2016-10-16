#ifndef NODE_LLVM_CONSTANTFP_H
#define NODE_LLVM_CONSTANTFP_H

#include "node-llvm.h"
namespace jsllvm {


  class ConstantFP : public Nan::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

  private:
    ConstantFP() { }
    virtual ~ConstantFP() { }

    static NAN_METHOD(New);
    static NAN_METHOD(GetDouble);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

}

#endif /* NODE_LLVM_CONSTANTFP_H */
