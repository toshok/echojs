#include "node-llvm.h"
#include "value.h"
#include "module.h"
#include "type.h"
#include "globalvariable.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  NAN_MODULE_INIT(GlobalVariable::Init) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("GlobalVariable").ToLocalChecked());

    Nan::SetPrototypeMethod (ctor, "dump", GlobalVariable::Dump);
    Nan::SetPrototypeMethod (ctor, "setInitializer", GlobalVariable::SetInitializer);
    Nan::SetPrototypeMethod (ctor, "setAlignment", GlobalVariable::SetAlignment);
    Nan::SetPrototypeMethod (ctor, "toString", GlobalVariable::ToString);

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);
    target->Set(context, Nan::New("GlobalVariable").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(GlobalVariable::New) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    if (info.Length()) {
      REQ_LLVM_MODULE_ARG(context, 0, module);
      REQ_LLVM_TYPE_ARG(context, 1, type);
      REQ_UTF8_ARG(context, 2, name);
      REQ_NULLABLE_LLVM_CONST_ARG(context, 3, init);
      REQ_BOOL_ARG(isolate, 4, visible);

      GlobalVariable* val = new GlobalVariable(new ::llvm::GlobalVariable(*module, type, false, visible ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::InternalLinkage, init, *name));

      val->Wrap(info.This());
    }
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(GlobalVariable::Dump) {
    auto val = Unwrap(info.This());
    val->llvm_obj->dump();
  }

  NAN_METHOD(GlobalVariable::SetInitializer) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    auto val = Unwrap(info.This());

    REQ_LLVM_CONST_ARG (context, 0, init);

    val->llvm_obj->setInitializer(init);
  }

  NAN_METHOD(GlobalVariable::SetAlignment) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    auto val = Unwrap(info.This());

    REQ_INT_ARG (context, 0, alignment);

    val->llvm_obj->setAlignment(llvm::MaybeAlign(alignment));
  }

  NAN_METHOD(GlobalVariable::ToString) {
    auto val = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    val->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  Nan::Persistent<v8::FunctionTemplate> GlobalVariable::constructor;
  Nan::Persistent<v8::Function> GlobalVariable::constructor_func;

}
