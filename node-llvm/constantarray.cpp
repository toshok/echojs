#include "node-llvm.h"
#include "constantarray.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void ConstantArray::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("ConstantArray").ToLocalChecked());

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);

    Nan::SetMethod(ctor_func, "get", ConstantArray::Get);
    target->Set(Nan::New("ConstantArray").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(ConstantArray::New) {
    Nan::ThrowTypeError("ConstantArray is not meant to be instantiated.");
  }

  NAN_METHOD(ConstantArray::Get) {
    REQ_LLVM_TYPE_ARG(0, array_type);
    REQ_ARRAY_ARG(1, elements);

    std::vector< llvm::Constant*> element_constants;
    for (uint32_t i = 0; i < elements->Length(); i ++) {
      element_constants.push_back (static_cast<llvm::Constant*>(Value::GetLLVMObj(elements->Get(i))));
    }

    Local<v8::Value> result = Value::Create(llvm::ConstantArray::get(static_cast<llvm::ArrayType*>(array_type), element_constants));
    info.GetReturnValue().Set(result);
  }

  Nan::Persistent<v8::FunctionTemplate> ConstantArray::constructor;
  Nan::Persistent<v8::Function> ConstantArray::constructor_func;
};
