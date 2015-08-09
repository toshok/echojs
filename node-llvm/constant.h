#ifndef NODE_LLVM_CONSTANT_H
#define NODE_LLVM_CONSTANT_H

#include "node-llvm.h"
namespace jsllvm {


  class Constant : public Nan::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

  private:
    Constant();
    virtual ~Constant();

    static NAN_METHOD(New);
    static NAN_METHOD(GetNull);
    static NAN_METHOD(GetAggregateZero);
    static NAN_METHOD(GetBoolValue);
    static NAN_METHOD(GetIntegerValue);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

};

#endif /* NODE_LLVM_CONSTANT_H */
