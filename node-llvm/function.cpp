#include "node-llvm.h"
#include "type.h"
#include "function.h"
#include "functiontype.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void Function::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("LLVMFunction").ToLocalChecked());

    Nan::SetAccessor(ctor->InstanceTemplate(), Nan::New("args").ToLocalChecked(), Function::GetArgs);

    Nan::SetAccessor(ctor->InstanceTemplate(), Nan::New("argSize").ToLocalChecked(), Function::GetArgSize);
    Nan::SetAccessor(ctor->InstanceTemplate(), Nan::New("name").ToLocalChecked(), Function::GetName);
    Nan::SetAccessor(ctor->InstanceTemplate(), Nan::New("returnType").ToLocalChecked(), Function::GetReturnType);
    Nan::SetAccessor(ctor->InstanceTemplate(), Nan::New("type").ToLocalChecked(), Function::GetType);
    Nan::SetAccessor(ctor->InstanceTemplate(), Nan::New("doesNotThrow").ToLocalChecked(), Function::GetDoesNotThrow);
    Nan::SetAccessor(ctor->InstanceTemplate(), Nan::New("onlyReadsMemory").ToLocalChecked(), Function::GetOnlyReadsMemory);
    Nan::SetAccessor(ctor->InstanceTemplate(), Nan::New("doesNotAccessMemory").ToLocalChecked(), Function::GetDoesNotAccessMemory);

    Nan::SetPrototypeMethod(ctor, "dump", Function::Dump);
    Nan::SetPrototypeMethod(ctor, "setOnlyReadsMemory", Function::SetOnlyReadsMemory);
    Nan::SetPrototypeMethod(ctor, "setDoesNotAccessMemory", Function::SetDoesNotAccessMemory);
    Nan::SetPrototypeMethod(ctor, "setDoesNotThrow", Function::SetDoesNotThrow);
    Nan::SetPrototypeMethod(ctor, "setGC", Function::SetGC);
    Nan::SetPrototypeMethod(ctor, "setExternalLinkage", Function::SetInternalLinkage);
    Nan::SetPrototypeMethod(ctor, "setInternalLinkage", Function::SetInternalLinkage);
    Nan::SetPrototypeMethod(ctor, "setStructRet", Function::SetStructRet);
    Nan::SetPrototypeMethod(ctor, "hasStructRetAttr", Function::HasStructRetAttr);
    Nan::SetPrototypeMethod(ctor, "toString", Function::ToString);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("LLVMFunction").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(Function::New) {
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(Function::Dump) {
    auto fun = Unwrap(info.This());
    fun->llvm_obj->dump();
  }

  NAN_METHOD(Function::SetDoesNotAccessMemory) {
    auto fun = Unwrap(info.This());
    fun->llvm_obj->setDoesNotAccessMemory();
  }

  NAN_METHOD(Function::SetOnlyReadsMemory) {
    auto fun = Unwrap(info.This());
    fun->llvm_obj->setOnlyReadsMemory();
  }

  NAN_METHOD(Function::SetDoesNotThrow) {
    auto fun = Unwrap(info.This());
    fun->llvm_obj->setDoesNotThrow();
  }

  NAN_METHOD(Function::SetGC) {
    auto fun = Unwrap(info.This());
    REQ_UTF8_ARG(0, name);
    fun->llvm_obj->setGC(*name);
  }

  NAN_METHOD(Function::SetExternalLinkage) {
    auto fun = Unwrap(info.This());
    fun->llvm_obj->setLinkage (llvm::Function::ExternalLinkage);
  }

  NAN_METHOD(Function::SetInternalLinkage) {
    auto fun = Unwrap(info.This());
    fun->llvm_obj->setLinkage (llvm::Function::InternalLinkage);
  }

  NAN_METHOD(Function::ToString) {
    auto fun = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    fun->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(Function::SetStructRet) {
    auto fun = Unwrap(info.This());
    fun->llvm_obj->addAttribute(1 /* first arg */,
				llvm::Attribute::StructRet);

  }

  NAN_METHOD(Function::HasStructRetAttr) {
    auto fun = Unwrap(info.This());
    info.GetReturnValue().Set(fun->llvm_obj->hasStructRetAttr());
  }

  NAN_GETTER(Function::GetArgSize) {
    auto fun = Unwrap(info.This());
    info.GetReturnValue().Set(static_cast<int32_t>(fun->llvm_obj->arg_size()));
  }

  NAN_GETTER(Function::GetArgs) {
    auto fun = Unwrap(info.This());
    uint32_t size = fun->llvm_obj->arg_size();
    Local<v8::Array> result = Nan::New<v8::Array>(size);

    uint32_t Idx = 0;
    for (llvm::Function::arg_iterator AI = fun->llvm_obj->arg_begin(); Idx != size;
	 ++AI, ++Idx) {
      result->Set(Idx, Value::Create(AI));
    }

    info.GetReturnValue().Set(result);
  }

  NAN_GETTER(Function::GetName) {
    auto fun = Unwrap(info.This());
    std::string name = fun->llvm_obj->getName();
    info.GetReturnValue().Set(Nan::New(name.c_str()).ToLocalChecked());
  }

  NAN_GETTER(Function::GetReturnType) {
    auto fun = Unwrap(info.This());
    info.GetReturnValue().Set(Type::Create(fun->llvm_obj->getReturnType()));
  }

  NAN_GETTER(Function::GetType) {
    auto fun = Unwrap(info.This());
    info.GetReturnValue().Set(FunctionType::Create(fun->llvm_obj->getFunctionType()));
  }

  NAN_GETTER(Function::GetDoesNotThrow) {
    auto fun = Unwrap(info.This());
    info.GetReturnValue().Set(fun->llvm_obj->doesNotThrow());
  }

  NAN_GETTER(Function::GetDoesNotAccessMemory) {
    auto fun = Unwrap(info.This());
    info.GetReturnValue().Set(fun->llvm_obj->doesNotAccessMemory());
  }

  NAN_GETTER(Function::GetOnlyReadsMemory) {
    auto fun = Unwrap(info.This());
    info.GetReturnValue().Set(fun->llvm_obj->onlyReadsMemory());
  }

  Nan::Persistent<v8::FunctionTemplate> Function::constructor;
  Nan::Persistent<v8::Function> Function::constructor_func;

};
