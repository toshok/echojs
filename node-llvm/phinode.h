#ifndef NODE_LLVM_PHINODE_H
#define NODE_LLVM_PHINODE_H

#include "node-llvm.h"
namespace jsllvm {


  class PHINode : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(::llvm::PHINode *llvm_phi);

  private:
    ::llvm::PHINode *llvm_phi;

    PHINode(::llvm::PHINode *llvm_phi);
    PHINode();
    virtual ~PHINode();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> AddIncoming(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };
};

#endif /* NODE_LLVM_PHINODE_H */
