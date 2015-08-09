#include "node-llvm.h"
#include "type.h"
#include "callinvoke.h"
#include "value.h"
#include "instruction.h"
#include "basicblock.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void Call::Init(Handle<Object> target)
  {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(Instruction::constructor));
    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("CallInst").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "dump", Call::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", Call::ToString);
    // XXX these should be setters
    Nan::SetPrototypeMethod(ctor, "setOnlyReadsMemory", Call::SetOnlyReadsMemory);
    Nan::SetPrototypeMethod(ctor, "setDoesNotAccessMemory", Call::SetDoesNotAccessMemory);
    Nan::SetPrototypeMethod(ctor, "setDoesNotThrow", Call::SetDoesNotThrow);
    Nan::SetPrototypeMethod(ctor, "setStructRet", Call::SetStructRet);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("CallInst").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(Call::New) {
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(Call::Dump) {
    auto call = Unwrap(info.This());
    call->llvm_obj->dump();
  }

  NAN_METHOD(Call::ToString) {
    auto call = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    call->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(Call::SetOnlyReadsMemory) {
    auto call = Unwrap(info.This());
    call->llvm_obj->setOnlyReadsMemory();
  }

  NAN_METHOD(Call::SetDoesNotAccessMemory) {
    auto call = Unwrap(info.This());
    call->llvm_obj->setDoesNotAccessMemory();
  }

  NAN_METHOD(Call::SetDoesNotThrow) {
    auto call = Unwrap(info.This());
    call->llvm_obj->setDoesNotThrow();
  }

  NAN_METHOD(Call::SetStructRet) {
    auto call = Unwrap(info.This());
    call->llvm_obj->addAttribute(1 /* first arg */,
				 llvm::Attribute::StructRet);
  }

  Nan::Persistent<v8::FunctionTemplate> Call::constructor;
  Nan::Persistent<v8::Function> Call::constructor_func;



  void Invoke::Init(Handle<Object> target)
  {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(Instruction::constructor));

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("InvokeInst").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "dump", Invoke::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", Invoke::ToString);
    Nan::SetPrototypeMethod(ctor, "setOnlyReadsMemory", Invoke::SetOnlyReadsMemory);
    Nan::SetPrototypeMethod(ctor, "setDoesNotAccessMemory", Invoke::SetDoesNotAccessMemory);
    Nan::SetPrototypeMethod(ctor, "setDoesNotThrow", Invoke::SetDoesNotThrow);
    Nan::SetPrototypeMethod(ctor, "setStructRet", Invoke::SetStructRet);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("InvokeInst").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(Invoke::New) {
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(Invoke::Dump) {
    auto invoke = Unwrap(info.This());
    invoke->llvm_obj->dump();
  }

  NAN_METHOD(Invoke::ToString) {
    auto invoke = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    invoke->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(Invoke::SetOnlyReadsMemory) {
    auto invoke = Unwrap(info.This());
    invoke->llvm_obj->setOnlyReadsMemory();
  }

  NAN_METHOD(Invoke::SetDoesNotAccessMemory) {
    auto invoke = Unwrap(info.This());
    invoke->llvm_obj->setDoesNotAccessMemory();
  }

  NAN_METHOD(Invoke::SetDoesNotThrow) {
    auto invoke = Unwrap(info.This());
    invoke->llvm_obj->setDoesNotThrow();
  }

  NAN_METHOD(Invoke::SetStructRet) {
    auto invoke = Unwrap(info.This());
    invoke->llvm_obj->addAttribute(1 /* first arg */,
				   llvm::Attribute::StructRet);
  }

  Nan::Persistent<v8::FunctionTemplate> Invoke::constructor;
  Nan::Persistent<v8::Function> Invoke::constructor_func;
};


