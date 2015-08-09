#ifndef NODE_LLVM_INSTRUCTION_H
#define NODE_LLVM_INSTRUCTION_H

#include "node-llvm.h"

namespace jsllvm {

  class Instruction : public LLVMObjectWrap< ::llvm::Instruction, Instruction> {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;

  private:
    typedef LLVMObjectWrap< ::llvm::Instruction, Instruction> BaseType;
    friend class LLVMObjectWrap< ::llvm::Instruction, Instruction>;

    Instruction(llvm::Instruction *llvm_instr) : BaseType(llvm_instr) { }
    Instruction() : BaseType(nullptr) { }
    virtual ~Instruction() { }

    static NAN_METHOD(New);
    static NAN_METHOD(SetDebugLoc);

  };

};

#endif /* NODE_LLVM_TYPE_H */

