#include "node-llvm.h"
#include "value.h"
#include "instruction.h"
#include "dibuilder.h"

using namespace node;
using namespace v8;

namespace jsllvm {

void Instruction::Init(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->Inherit(Value::s_ct);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("Instruction"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "setDebugLoc", Instruction::SetDebugLoc);

    s_func = Persistent<Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("Instruction"), s_func);
}

Instruction::Instruction(llvm::Instruction *llvm_instr)
    : llvm_instr(llvm_instr) {}

Instruction::Instruction() : llvm_instr(NULL) {}

Instruction::~Instruction() {}

Handle<v8::Value> Instruction::New(llvm::Instruction *llvm_instr) {
    HandleScope scope;
    Local<Object> new_instance = Instruction::s_func->NewInstance();
    Instruction *new_instr = new Instruction(llvm_instr);
    new_instr->Wrap(new_instance);
    return scope.Close(new_instance);
}

Handle<::v8::Value> Instruction::New(const Arguments &args) {
    HandleScope scope;
    Instruction *instr = new Instruction();
    instr->Wrap(args.This());
    return args.This();
}

Handle<::v8::Value> Instruction::SetDebugLoc(const Arguments &args) {
    HandleScope scope;
    Instruction *instr = ObjectWrap::Unwrap<Instruction>(args.This());

    REQ_LLVM_DEBUGLOC_ARG(0, debugloc);

    instr->llvm_instr->setDebugLoc(debugloc);
    return scope.Close(Undefined());
}

Persistent<FunctionTemplate> Instruction::s_ct;
Persistent<Function> Instruction::s_func;
};
