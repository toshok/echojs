#include "node-llvm.h"
#include "type.h"
#include "switch.h"
#include "instruction.h"
#include "value.h"
#include "basicblock.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void Switch::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(Instruction::constructor));
    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("SwitchInst").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "dump", Switch::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", Switch::ToString);
    Nan::SetPrototypeMethod(ctor, "addCase", Switch::AddCase);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("SwitchInst").ToLocalChecked(), ctor_func);
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
    auto _switch = Unwrap(info.This());
    REQ_LLVM_VAL_ARG(0, OnVal);
    REQ_LLVM_BB_ARG(1, Dest);
    _switch->llvm_obj->addCase(static_cast<llvm::ConstantInt*>(OnVal), Dest);
  }

  Nan::Persistent<v8::FunctionTemplate> Switch::constructor;
  Nan::Persistent<v8::Function> Switch::constructor_func;
}


