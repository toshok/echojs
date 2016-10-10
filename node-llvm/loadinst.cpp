#include "node-llvm.h"
#include "type.h"
#include "value.h"
#include "basicblock.h"
#include "instruction.h"
#include "loadinst.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void LoadInst::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(Instruction::constructor));
    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("LoadInst").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "dump", LoadInst::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", LoadInst::ToString);
    Nan::SetPrototypeMethod(ctor, "setAlignment", LoadInst::SetAlignment);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("LoadInst").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(LoadInst::New) {
    auto load = new LoadInst();
    load->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(LoadInst::Dump) {
    auto load = Unwrap(info.This());
    load->llvm_obj->dump();
  }

  NAN_METHOD(LoadInst::ToString) {
    auto load = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    load->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(LoadInst::SetAlignment) {
    auto load = Unwrap(info.This());

    REQ_INT_ARG (0, alignment);

    load->llvm_obj->setAlignment(alignment);
  }

  Nan::Persistent<v8::FunctionTemplate> LoadInst::constructor;
  Nan::Persistent<v8::Function> LoadInst::constructor_func;
}


