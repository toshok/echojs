#ifndef NODE_LLVM_CONSTANT_H
#define NODE_LLVM_CONSTANT_H

#include "node-llvm.h"
namespace jsllvm {

class Constant : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

  private:
    Constant();
    virtual ~Constant();

    static v8::Handle<v8::Value> New(const v8::Arguments &args);
    static v8::Handle<v8::Value> GetNull(const v8::Arguments &args);
    static v8::Handle<v8::Value> GetAggregateZero(const v8::Arguments &args);
    static v8::Handle<v8::Value> GetBoolValue(const v8::Arguments &args);
    static v8::Handle<v8::Value> GetIntegerValue(const v8::Arguments &args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
};
};

#endif /* NODE_LLVM_CONSTANT_H */
