#ifndef NODE_LLVM_GLOBALVARIABLE_H
#define NODE_LLVM_GLOBALVARIABLE_H

#include "node-llvm.h"
namespace jsllvm {

  class GlobalVariable : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::GlobalVariable *llvm_global);

    static llvm::GlobalVariable* GetLLVMObj (v8::Local<v8::Value> value) {
      if (value->IsNull())
	return NULL;
      return node::ObjectWrap::Unwrap<GlobalVariable>(value->ToObject())->llvm_global;
    }

  private:
    llvm::GlobalVariable* llvm_global;

    GlobalVariable(llvm::GlobalVariable *llvm_global);
    GlobalVariable();
    virtual ~GlobalVariable();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Dump(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetInitializer(const v8::Arguments& args);
    static v8::Handle<v8::Value> ToString(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

};

#endif /* NODE_LLVM_GLOBALVARIABLE_H */

