#include "node-llvm.h"
#include "type.h"
using namespace node;
using namespace v8;


namespace jsllvm {

  NAN_MODULE_INIT(Type::Init) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("Type").ToLocalChecked());

#define LLVM_SET_METHOD(obj, name) Nan::SetMethod(obj, #name, Type::name)
#define LLVM_SET_PROTOTYPE_METHOD(obj, name) Nan::SetPrototypeMethod(obj, #name, Type::name)
    LLVM_SET_METHOD(ctor, getDoubleTy);
    LLVM_SET_METHOD(ctor, getInt64Ty);
    LLVM_SET_METHOD(ctor, getInt32Ty);
    LLVM_SET_METHOD(ctor, getInt16Ty);
    LLVM_SET_METHOD(ctor, getInt8Ty);
    LLVM_SET_METHOD(ctor, getInt1Ty);
    LLVM_SET_METHOD(ctor, getVoidTy);

    LLVM_SET_PROTOTYPE_METHOD(ctor, pointerTo);
    LLVM_SET_PROTOTYPE_METHOD(ctor, isVoid);
    LLVM_SET_PROTOTYPE_METHOD(ctor, dump);
#undef LLVM_SET_METHOD
#undef LLVM_SET_PROTOTYPE_METHOD

    Nan::SetPrototypeMethod(ctor, "toString", Type::ToString);

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);
    target->Set(context, Nan::New("Type").ToLocalChecked(), ctor_func).Check();
  }

#define LLVM_TYPE_METHOD_PROXY(name) LLVM_TYPE_METHOD(name,name)
#define LLVM_TYPE_METHOD(name,llvm_ty)					\
  NAN_METHOD(Type::name) {						\
    info.GetReturnValue().Set(Type::Create(llvm::Type::llvm_ty(TheContext))); \
  }

  LLVM_TYPE_METHOD_PROXY(getDoubleTy)
  LLVM_TYPE_METHOD_PROXY(getInt64Ty)
  LLVM_TYPE_METHOD_PROXY(getInt32Ty)
  LLVM_TYPE_METHOD_PROXY(getInt16Ty)
  LLVM_TYPE_METHOD_PROXY(getInt8Ty)
  LLVM_TYPE_METHOD_PROXY(getInt1Ty)
  LLVM_TYPE_METHOD_PROXY(getVoidTy)

#undef LLVM_TYPE_METHOD_PROXY
#undef LLVM_TYPE_METHOD

  NAN_METHOD(Type::New) {
    auto type = new Type();
    type->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(Type::pointerTo) {
    auto type = Unwrap(info.This());
    info.GetReturnValue().Set(Type::Create(type->llvm_obj->getPointerTo()));
  }

  NAN_METHOD(Type::isVoid) {
    auto type = Unwrap(info.This());
    info.GetReturnValue().Set(type->llvm_obj->isVoidTy());
  }

  NAN_METHOD(Type::ToString) {
    auto type = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    type->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(Type::dump) {
    auto type = Unwrap(info.This());
    type->llvm_obj->dump();
  }

  Nan::Persistent<v8::FunctionTemplate> Type::constructor;
  Nan::Persistent<v8::Function> Type::constructor_func;
}
