#include "node-llvm.h"
#include "constantfp.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  NAN_MODULE_INIT(ConstantFP::Init) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("ConstantFP").ToLocalChecked());

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);

    Nan::SetMethod(ctor_func, "getDouble", ConstantFP::GetDouble);

    target->Set(context, Nan::New("ConstantFP").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(ConstantFP::New) {
    Nan::ThrowTypeError("ConstantFP is not meant to be instantiated.");
  }

  NAN_METHOD(ConstantFP::GetDouble) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    REQ_DOUBLE_ARG(context, 0, v);
    Local<v8::Value> result = Value::Create(llvm::ConstantFP::get(TheContext, llvm::APFloat(v)));
    info.GetReturnValue().Set(result);
  }

  Nan::Persistent<v8::FunctionTemplate> ConstantFP::constructor;
  Nan::Persistent<v8::Function> ConstantFP::constructor_func;
}
