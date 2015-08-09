#include "node-llvm.h"
#include "constantagg.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void ConstantAggregateZero::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("ConstantAggregateZero").ToLocalChecked());

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);

    Nan::SetMethod(ctor_func, "get", ConstantAggregateZero::Get);

    target->Set(Nan::New("ConstantAggregateZero").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(ConstantAggregateZero::New) {
    Nan::ThrowTypeError("ConstantAggregateZero is not meant to be instantiated.");
  }

  NAN_METHOD(ConstantAggregateZero::Get) {
    REQ_LLVM_TYPE_ARG(0, ty);
    Local<v8::Value> result = Value::Create(llvm::ConstantAggregateZero::get(ty));
    info.GetReturnValue().Set(result);
  }

  Nan::Persistent<v8::FunctionTemplate> ConstantAggregateZero::constructor;
  Nan::Persistent<v8::Function> ConstantAggregateZero::constructor_func;
};
