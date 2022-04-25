#include "node-llvm.h"
#include "constant.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  NAN_MODULE_INIT(Constant::Init) {
    Nan::HandleScope scope;

    Local<FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("Constant").ToLocalChecked());

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);

    Nan::SetMethod(ctor_func, "getNull", Constant::GetNull);
    Nan::SetMethod(ctor_func, "getAggregateZero", Constant::GetAggregateZero);
    Nan::SetMethod(ctor_func, "getBoolValue", Constant::GetBoolValue);
    Nan::SetMethod(ctor_func, "getIntegerValue", Constant::GetIntegerValue);

    target->Set(context, Nan::New("Constant").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(Constant::New) {
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(Constant::GetNull) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    REQ_LLVM_TYPE_ARG(context, 0, ty);
    info.GetReturnValue().Set(Value::Create(llvm::Constant::getNullValue(ty)));
  }

  NAN_METHOD(Constant::GetAggregateZero) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    REQ_LLVM_TYPE_ARG(context, 0, ty);
    info.GetReturnValue().Set(Value::Create(llvm::ConstantAggregateZero::get(ty)));
  }

  NAN_METHOD(Constant::GetBoolValue) {
    v8::Isolate *isolate = info.GetIsolate();

    REQ_BOOL_ARG(isolate, 0, b);
    Local<v8::Value> result = Value::Create(llvm::Constant::getIntegerValue(llvm::Type::getInt8Ty(TheContext), llvm::APInt(8, b?1:0)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(Constant::GetIntegerValue) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    REQ_LLVM_TYPE_ARG(context, 0, ty);
    REQ_INT_ARG(context, 1, v);

    Local<v8::Value> result;
    if (info.Length() == 2) {
      result = Value::Create(llvm::Constant::getIntegerValue(ty, llvm::APInt(ty->getPrimitiveSizeInBits(), v)));
    }
    else if (info.Length() == 3 && info[2]->IsNumber() && ty->getPrimitiveSizeInBits() == 64) {
      // allow a 3 arg form for 64 bit ints:
      // constant = llvm.Constant.getIntegerValue types.int64, ch, cl
      uint64_t vhi = v;
      uint32_t vlo = (uint32_t)info[2]->NumberValue(context).ToChecked();
      result = Value::Create (llvm::Constant::getIntegerValue(ty, llvm::APInt(ty->getPrimitiveSizeInBits(), (int64_t)((vhi << 32) | vlo))));
    }
    else {
      Nan::ThrowTypeError("Invalid parameters to Constant.getIntegerValue");
    }

    info.GetReturnValue().Set(result);
  }

  Nan::Persistent<v8::FunctionTemplate> Constant::constructor;
  Nan::Persistent<v8::Function> Constant::constructor_func;
}
