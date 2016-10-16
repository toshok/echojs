#ifndef NODE_LLVM_LANDINGPAD_H
#define NODE_LLVM_LANDINGPAD_H

#include "node-llvm.h"
namespace jsllvm {

  class LandingPad : public LLVMObjectWrap< ::llvm::LandingPadInst, LandingPad> {
  public:
    static void Init(v8::Handle<v8::Object> target);

  private:
    typedef LLVMObjectWrap< ::llvm::LandingPadInst, LandingPad> BaseType;
    friend class LLVMObjectWrap< ::llvm::LandingPadInst, LandingPad>;

    LandingPad(::llvm::LandingPadInst *llvm_landing_pad) : BaseType(llvm_landing_pad) { }
    LandingPad() : BaseType(nullptr) { }
    virtual ~LandingPad() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Dump);
    static NAN_METHOD(ToString);
    static NAN_METHOD(SetCleanup);
    static NAN_METHOD(AddClause);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };
}

#endif /* NODE_LLVM_LANDINGPAD_H */
