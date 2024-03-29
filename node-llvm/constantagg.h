#ifndef NODE_LLVM_CONSTANTAGG_H
#define NODE_LLVM_CONSTANTAGG_H

#include "node-llvm.h"
namespace jsllvm {


  class ConstantAggregateZero : public Nan::ObjectWrap {
  public:
    static NAN_MODULE_INIT(Init);

  private:
    ConstantAggregateZero() { }
    virtual ~ConstantAggregateZero() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Get);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

}

#endif /* NODE_LLVM_CONSTANTAGG_H */
