#include "node-llvm.h"
#include "basicblock.h"
#include "function.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void BasicBlock::Init(v8::Handle<Object> target)
  {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("BasicBlock").ToLocalChecked());

    Nan::SetAccessor(ctor->InstanceTemplate(),
		     Nan::New("name").ToLocalChecked(),
		     BasicBlock::GetName);
    Nan::SetAccessor(ctor->InstanceTemplate(),
		     Nan::New("parent").ToLocalChecked(),
		     BasicBlock::GetParent);

    Nan::SetPrototypeMethod(ctor, "dump", BasicBlock::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", BasicBlock::ToString);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("BasicBlock").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(BasicBlock::New) {
    if (info.Length()) {
      REQ_UTF8_ARG(0, blockname);
      REQ_LLVM_FUN_ARG(1, fun);

      BasicBlock* bb = new BasicBlock(llvm::BasicBlock::Create(llvm::getGlobalContext(), *blockname, fun));
      bb->Wrap(info.This());
    }
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(BasicBlock::Dump) {
    auto bb = Unwrap(info.This());
    bb->llvm_obj->dump();
  }

  NAN_METHOD(BasicBlock::ToString) {
    auto bb = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    bb->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_GETTER(BasicBlock::GetParent) {
    auto bb = Unwrap(info.This());

    Local<v8::Value> result = Function::Create(bb->llvm_obj->getParent());
    info.GetReturnValue().Set(result);
  }

  NAN_GETTER(BasicBlock::GetName) {
    auto bb = Unwrap(info.This());

    Local<v8::String> result = Nan::New(bb->llvm_obj->getName().data(), bb->llvm_obj->getName().size()).ToLocalChecked();
    info.GetReturnValue().Set(result);
  }

  Nan::Persistent<FunctionTemplate> BasicBlock::constructor;
  Nan::Persistent<v8::Function> BasicBlock::constructor_func;
};
