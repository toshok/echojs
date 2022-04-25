#include "node-llvm.h"
#include "functiontype.h"
#include "type.h"

using namespace node;
using namespace v8;


namespace jsllvm {


  NAN_MODULE_INIT(FunctionType::Init) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(Type::constructor));

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("FunctionType").ToLocalChecked());

    Nan::SetMethod(ctor, "get", FunctionType::Get);

    Nan::SetPrototypeMethod(ctor, "getReturnType", FunctionType::GetReturnType);
    Nan::SetPrototypeMethod(ctor, "getParamType", FunctionType::GetParamType);

    Nan::SetPrototypeMethod(ctor, "dump", FunctionType::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", FunctionType::ToString);

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);
    target->Set(context, Nan::New("FunctionType").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(FunctionType::Get) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    REQ_LLVM_TYPE_ARG(context, 0, returnType);
    REQ_ARRAY_ARG(context, 1, argTypes);

    std::vector< llvm::Type*> arg_types;
    for (uint32_t i = 0; i < argTypes->Length(); i ++) {
      arg_types.push_back (Type::GetLLVMObj(context, argTypes->Get(context, i).ToLocalChecked()));
    }

    ::llvm::FunctionType *FT = llvm::FunctionType::get(returnType,
						       arg_types, false);

    v8::Local<v8::Value> result = FunctionType::Create(FT);
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(FunctionType::New) {
    auto type = new FunctionType();
    type->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(FunctionType::GetReturnType) {
    auto type = Unwrap(info.This());
    info.GetReturnValue().Set(Type::Create(type->llvm_obj->getReturnType()));
  }

  NAN_METHOD(FunctionType::GetParamType) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    auto type = Unwrap(info.This());
    REQ_INT_ARG(context, 0, i);
    info.GetReturnValue().Set(Type::Create(type->llvm_obj->getParamType(i)));
  }

  NAN_METHOD(FunctionType::ToString) {
    auto type = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    type->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(FunctionType::Dump) {
    auto type = Unwrap(info.This());
    type->llvm_obj->dump();
  }

  Nan::Persistent<v8::FunctionTemplate> FunctionType::constructor;
  Nan::Persistent<v8::Function> FunctionType::constructor_func;
}
