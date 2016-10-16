#include "node-llvm.h"
#include "structtype.h"
#include "type.h"

using namespace node;
using namespace v8;


namespace jsllvm {


  void StructType::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(Type::constructor));
    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("StructType").ToLocalChecked());


    Nan::SetMethod(ctor, "create", StructType::Create);

    Nan::SetPrototypeMethod(ctor, "dump", StructType::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", StructType::ToString);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("StructType").ToLocalChecked(), ctor_func);
  }

  NAN_METHOD(StructType::Create) {
    REQ_UTF8_ARG (0, name);
    REQ_ARRAY_ARG (1, elementTypes)

    std::vector< llvm::Type*> element_types;
    for (uint32_t i = 0; i < elementTypes->Length(); i ++) {
      element_types.push_back (Type::GetLLVMObj(elementTypes->Get(i)));
    }

    // BaseType::Create, not StructType::Create, since the latter is ambiguous
    info.GetReturnValue().Set(BaseType::Create(llvm::StructType::create(llvm::getGlobalContext(), element_types, *name)));
  }

  NAN_METHOD(StructType::New) {
    auto type = new StructType();
    type->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(StructType::ToString) {
    auto type = new StructType();

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    type->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(StructType::Dump) {
    auto type = new StructType();
    type->llvm_obj->dump();
  }

  Nan::Persistent<v8::FunctionTemplate> StructType::constructor;
  Nan::Persistent<v8::Function> StructType::constructor_func;
}
