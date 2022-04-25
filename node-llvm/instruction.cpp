#include "node-llvm.h"
#include "value.h"
#include "instruction.h"
#include "dibuilder.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  NAN_MODULE_INIT(Instruction::Init) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(Value::constructor));

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("Instruction").ToLocalChecked());

#if false
    Nan::SetPrototypeMethod (ctor, "setDebugLoc", Instruction::SetDebugLoc);
#endif

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);
    target->Set(context, Nan::New("Instruction").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(Instruction::New) {
    auto instr = new Instruction();
    instr->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

#if false
  NAN_METHOD(Instruction::SetDebugLoc) {
    auto instr = Unwrap(info.This());
    REQ_LLVM_DEBUGLOC_ARG(0, debugloc);
    instr->llvm_obj->setDebugLoc(debugloc);
  }
#endif

  Nan::Persistent<v8::FunctionTemplate> Instruction::constructor;
  Nan::Persistent<v8::Function> Instruction::constructor_func;

}
