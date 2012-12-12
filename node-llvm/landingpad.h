#ifndef NODE_LLVM_LANDINGPAD_H
#define NODE_LLVM_LANDINGPAD_H

#include "node-llvm.h"
namespace jsllvm {


  class LandingPad : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(::llvm::LandingPadInst *llvm_landing_pad);

  private:
    ::llvm::LandingPadInst *llvm_landing_pad;

    LandingPad(::llvm::LandingPadInst *llvm_landing_pad);
    LandingPad();
    virtual ~LandingPad();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> ToString(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetCleanup(const v8::Arguments& args);
    static v8::Handle<v8::Value> AddClause(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };
};

#endif /* NODE_LLVM_LANDINGPAD_H */
