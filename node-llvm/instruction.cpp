#include "node-llvm.h"
#include "value.h"
#include "instruction.h"
#include "dibuilder.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void Instruction::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(Value::constructor));

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("Instruction").ToLocalChecked());

    Nan::SetPrototypeMethod (ctor, "setDebugLoc", Instruction::SetDebugLoc);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("Instruction").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(Instruction::New) {
    auto instr = new Instruction();
    instr->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(Instruction::SetDebugLoc) {
    auto instr = Unwrap(info.This());
    REQ_LLVM_DEBUGLOC_ARG(0, debugloc);
    instr->llvm_obj->setDebugLoc(debugloc);
  }

  Nan::Persistent<v8::FunctionTemplate> Instruction::constructor;
  Nan::Persistent<v8::Function> Instruction::constructor_func;

};
