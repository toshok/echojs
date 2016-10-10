#include "node-llvm.h"
#include "constant.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void Constant::Init(Handle<Object> target)
  {
    Nan::HandleScope scope;

    Local<FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("Constant").ToLocalChecked());

    Local<Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);

    Nan::SetMethod(ctor_func, "getNull", Constant::GetNull);
    Nan::SetMethod(ctor_func, "getAggregateZero", Constant::GetAggregateZero);
    Nan::SetMethod(ctor_func, "getBoolValue", Constant::GetBoolValue);
    Nan::SetMethod(ctor_func, "getIntegerValue", Constant::GetIntegerValue);

    target->Set(Nan::New("Constant").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(Constant::New) {
    Nan::ThrowError("Constant is not meant to be instantiated.");
  }

  NAN_METHOD(Constant::GetNull) {
    REQ_LLVM_TYPE_ARG(0, ty);
    info.GetReturnValue().Set(Value::Create(llvm::Constant::getNullValue(ty)));
  }

  NAN_METHOD(Constant::GetAggregateZero) {
    REQ_LLVM_TYPE_ARG(0, ty);
    info.GetReturnValue().Set(Value::Create(llvm::ConstantAggregateZero::get(ty)));
  }

  NAN_METHOD(Constant::GetBoolValue) {
    REQ_BOOL_ARG(0, b);
    Local<v8::Value> result = Value::Create(llvm::Constant::getIntegerValue(llvm::Type::getInt8Ty(llvm::getGlobalContext()), llvm::APInt(8, b?1:0)));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(Constant::GetIntegerValue) {
    REQ_LLVM_TYPE_ARG(0, ty);
    REQ_INT_ARG(1, v);

    Local<v8::Value> result;
    if (info.Length() == 2) {
      result = Value::Create(llvm::Constant::getIntegerValue(ty, llvm::APInt(ty->getPrimitiveSizeInBits(), v)));
    }
    else if (info.Length() == 3 && info[2]->IsNumber() && ty->getPrimitiveSizeInBits() == 64) {
      // allow a 3 arg form for 64 bit ints:
      // constant = llvm.Constant.getIntegerValue types.int64, ch, cl
      uint64_t vhi = v;
      uint32_t vlo = (uint32_t)info[2]->NumberValue();
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
