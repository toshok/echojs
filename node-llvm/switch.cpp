#include "node-llvm.h"
#include "type.h"
#include "switch.h"
#include "instruction.h"
#include "value.h"
#include "basicblock.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  NAN_MODULE_INIT(Switch::Init) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(Instruction::constructor));
    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("SwitchInst").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "dump", Switch::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", Switch::ToString);
    Nan::SetPrototypeMethod(ctor, "addCase", Switch::AddCase);

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);
    target->Set(context, Nan::New("SwitchInst").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(Switch::New) {
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(Switch::Dump) {
    auto _switch = Unwrap(info.This());
    _switch->llvm_obj->dump();
  }

  NAN_METHOD(Switch::ToString) {
    auto _switch = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    _switch->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(Switch::AddCase) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    auto _switch = Unwrap(info.This());
    REQ_LLVM_VAL_ARG(context, 0, OnVal);
    REQ_LLVM_BB_ARG(context, 1, Dest);
    _switch->llvm_obj->addCase(static_cast<llvm::ConstantInt*>(OnVal), Dest);
  }

  Nan::Persistent<v8::FunctionTemplate> Switch::constructor;
  Nan::Persistent<v8::Function> Switch::constructor_func;
}


