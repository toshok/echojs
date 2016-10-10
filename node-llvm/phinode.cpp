#include "node-llvm.h"
#include "type.h"
#include "phinode.h"
#include "value.h"
#include "basicblock.h"
#include "instruction.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void PHINode::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(Instruction::constructor));
    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("PHINode").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "dump", PHINode::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", PHINode::ToString);
    Nan::SetPrototypeMethod(ctor, "addIncoming", PHINode::AddIncoming);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("PHINode").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(PHINode::New) {
    auto phi = new PHINode();
    phi->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(PHINode::Dump) {
    auto phi = Unwrap(info.This());
    phi->llvm_obj->dump();
  }

  NAN_METHOD(PHINode::ToString) {
    auto phi = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    phi->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(PHINode::AddIncoming) {
    auto phi = Unwrap(info.This());
    REQ_LLVM_VAL_ARG(0, incoming_val);
    REQ_LLVM_BB_ARG(1, incoming_bb);
    phi->llvm_obj->addIncoming(incoming_val, incoming_bb);
  }

  Nan::Persistent<v8::FunctionTemplate> PHINode::constructor;
  Nan::Persistent<v8::Function> PHINode::constructor_func;
}


