#ifndef NODE_LLVM_CONSTANTFP_H
#define NODE_LLVM_CONSTANTFP_H

#include "node-llvm.h"
namespace jsllvm {


  class ConstantFP : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

  private:
    ConstantFP();
    virtual ~ConstantFP();

    static v8::Handle<v8::Value> New (const v8::Arguments& args);
    static v8::Handle<v8::Value> GetDouble (const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

};

#endif /* NODE_LLVM_CONSTANTFP_H */
