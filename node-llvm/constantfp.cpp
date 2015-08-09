#include "node-llvm.h"
#include "constantfp.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void ConstantFP::Init(Handle<Object> target){
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("ConstantFP").ToLocalChecked());

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);

    Nan::SetMethod(ctor_func, "getDouble", ConstantFP::GetDouble);

    target->Set(Nan::New("ConstantFP").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(ConstantFP::New) {
    Nan::ThrowTypeError("ConstantFP is not meant to be instantiated.");
  }

  NAN_METHOD(ConstantFP::GetDouble) {
    REQ_DOUBLE_ARG(0, v);
    Local<v8::Value> result = Value::Create(llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(v)));
    info.GetReturnValue().Set(result);
  }

  Nan::Persistent<v8::FunctionTemplate> ConstantFP::constructor;
  Nan::Persistent<v8::Function> ConstantFP::constructor_func;
};
