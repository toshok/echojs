#include "node-llvm.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  NAN_MODULE_INIT(Value::Init) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("Value").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "dump", Value::Dump);
    Nan::SetPrototypeMethod(ctor, "setName", Value::SetName);
    Nan::SetPrototypeMethod(ctor, "toString", Value::ToString);

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);
    target->Set(context, Nan::New("Value").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(Value::New) {
    Value* val = new Value();
    val->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(Value::Dump) {
    auto val = Unwrap(info.This());
    val->llvm_obj->dump();
  }

  NAN_METHOD(Value::SetName) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    auto val = Unwrap(info.This());
    REQ_UTF8_ARG (context, 0, name);
    val->llvm_obj->setName(*name);
  }

  NAN_METHOD(Value::ToString) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    auto val = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    val->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  Nan::Persistent<v8::FunctionTemplate> Value::constructor;
  Nan::Persistent<v8::Function> Value::constructor_func;

}
