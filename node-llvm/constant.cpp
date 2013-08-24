#include "node-llvm.h"
#include "constant.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void Constant::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("Constant"));

    s_func = Persistent< ::v8::Function>::New(s_ct->GetFunction());

    NODE_SET_METHOD(s_func, "getNull", Constant::GetNull);
    NODE_SET_METHOD(s_func, "getAggregateZero", Constant::GetAggregateZero);
    NODE_SET_METHOD(s_func, "getBoolValue", Constant::GetBoolValue);
    NODE_SET_METHOD(s_func, "getIntegerValue", Constant::GetIntegerValue);

    target->Set(String::NewSymbol("Constant"),
		s_func);
  }

  Constant::Constant()
  {
  }

  Constant::~Constant()
  {
  }

  Handle<v8::Value> Constant::New(const Arguments& args)
  {
    return ThrowException(Exception::Error(String::New("Constant is not meant to be instantiated.")));
  }

  Handle<v8::Value> Constant::GetNull (const Arguments& args)
  {
    HandleScope scope;
    REQ_LLVM_TYPE_ARG(0, ty);
    Handle<v8::Value> result = Value::New(llvm::Constant::getNullValue(ty));
    return scope.Close(result);
  }

  Handle<v8::Value> Constant::GetAggregateZero (const Arguments& args)
  {
    HandleScope scope;
    REQ_LLVM_TYPE_ARG(0, ty);
    Handle<v8::Value> result = Value::New(llvm::ConstantAggregateZero::get(ty));
    return scope.Close(result);
  }

  Handle<v8::Value> Constant::GetBoolValue (const Arguments& args)
  {
    HandleScope scope;
    REQ_BOOL_ARG(0, b);
    Handle<v8::Value> result = Value::New(llvm::Constant::getIntegerValue(llvm::Type::getInt8Ty(llvm::getGlobalContext()), llvm::APInt(8, b?1:0)));
    return scope.Close(result);
  }

  Handle<v8::Value> Constant::GetIntegerValue (const Arguments& args)
  {
    HandleScope scope;
    REQ_LLVM_TYPE_ARG(0, ty);
    REQ_INT_ARG(1, v);

    Handle<v8::Value> result;
    if (args.Length() == 2)
      result = Value::New(llvm::Constant::getIntegerValue(ty, llvm::APInt(ty->getPrimitiveSizeInBits(), v)));
    else if (args.Length() == 3 && args[2]->IsNumber() && ty->getPrimitiveSizeInBits() == 64) {
      // allow a 3 arg form for 64 bit ints:
      // constant = llvm.Constant.getIntegerValue types.int64, ch, cl
      int64_t vhi = v;
      int64_t vlo = (int64_t)args[2]->NumberValue();
      result = Value::New(llvm::Constant::getIntegerValue(ty, llvm::APInt(ty->getPrimitiveSizeInBits(), (int64_t)(vhi << 32 | vlo))));
    }
    else {
      return ThrowException(Exception::TypeError(String::New("Invalid parameters to Constant.getIntegerValue")));
    }

    return scope.Close(result);
  }

  Persistent<FunctionTemplate> Constant::s_ct;
  Persistent< ::v8::Function> Constant::s_func;
};
