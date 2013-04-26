#ifndef NODE_LLVM_INSTRUCTION_H
#define NODE_LLVM_INSTRUCTION_H

#include "node-llvm.h"

namespace jsllvm {

  class Instruction : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::Instruction *llvm_instr);

    static llvm::Instruction* GetLLVMObj (v8::Local<v8::Value> instruction) {
      if (instruction->IsNull())
	return NULL;
      return node::ObjectWrap::Unwrap<Instruction>(instruction->ToObject())->llvm_instr;
    }

    static v8::Persistent<v8::FunctionTemplate> s_ct;
  private:
    llvm::Instruction* llvm_instr;

    Instruction(llvm::Instruction *llvm_instr);
    Instruction();
    virtual ~Instruction();

    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> SetDebugLoc(const v8::Arguments& args);

    static v8::Persistent<v8::Function> s_func;
  };

};

#endif /* NODE_LLVM_TYPE_H */

