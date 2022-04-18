#include "node-llvm.h"
#include "constantagg.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  NAN_MODULE_INIT(ConstantAggregateZero::Init) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("ConstantAggregateZero").ToLocalChecked());

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);

    Nan::SetMethod(ctor_func, "get", ConstantAggregateZero::Get);

    target->Set(context, Nan::New("ConstantAggregateZero").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(ConstantAggregateZero::New) {
    Nan::ThrowTypeError("ConstantAggregateZero is not meant to be instantiated.");
  }

  NAN_METHOD(ConstantAggregateZero::Get) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    REQ_LLVM_TYPE_ARG(context, 0, ty);
    Local<v8::Value> result = Value::Create(llvm::ConstantAggregateZero::get(ty));
    info.GetReturnValue().Set(result);
  }

  Nan::Persistent<v8::FunctionTemplate> ConstantAggregateZero::constructor;
  Nan::Persistent<v8::Function> ConstantAggregateZero::constructor_func;
}
