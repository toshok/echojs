#include "node-llvm.h"
#include "arraytype.h"
#include "type.h"

using namespace node;
using namespace v8;


namespace jsllvm {


  NAN_MODULE_INIT(ArrayType::Init) {
    Nan::HandleScope scope;

    Local<FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(Type::constructor));
    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("ArrayType").ToLocalChecked());


    Nan::SetMethod(ctor, "get", ArrayType::Get);

    Nan::SetPrototypeMethod(ctor, "dump", ArrayType::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", ArrayType::ToString);

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);

    target->Set(context, Nan::New("ArrayType").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(ArrayType::Get) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    REQ_LLVM_TYPE_ARG (context, 0, elementType);
    REQ_INT_ARG (context, 1, numElements);

    info.GetReturnValue().Set(ArrayType::Create(llvm::ArrayType::get(elementType, numElements)));
  }

  NAN_METHOD(ArrayType::New) {
    if (info.This()->InternalFieldCount() == 0)
      return Nan::ThrowTypeError("Cannot Instantiate without new");

    ArrayType* type = new ArrayType();
    type->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(ArrayType::ToString) {
    ArrayType* type = ObjectWrap::Unwrap<ArrayType>(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    type->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(ArrayType::Dump) {
    ArrayType* type = ObjectWrap::Unwrap<ArrayType>(info.This());
    // type->llvm_obj->dump();
  }

  Nan::Persistent<v8::FunctionTemplate> ArrayType::constructor;
  Nan::Persistent<v8::Function> ArrayType::constructor_func;
}
