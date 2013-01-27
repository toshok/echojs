#include "node-llvm.h"
#include "type.h"
using namespace node;
using namespace v8;


namespace jsllvm {


  void Type::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("Type"));

#define LLVM_SET_METHOD(obj, name) NODE_SET_METHOD(obj, #name, Type::name)
#define LLVM_SET_PROTOTYPE_METHOD(obj, name) NODE_SET_PROTOTYPE_METHOD(obj, #name, Type::name)
    LLVM_SET_METHOD(s_ct, getDoubleTy);
    LLVM_SET_METHOD(s_ct, getInt64Ty);
    LLVM_SET_METHOD(s_ct, getInt32Ty);
    LLVM_SET_METHOD(s_ct, getInt16Ty);
    LLVM_SET_METHOD(s_ct, getInt8Ty);
    LLVM_SET_METHOD(s_ct, getVoidTy);

    LLVM_SET_PROTOTYPE_METHOD(s_ct, pointerTo);
    LLVM_SET_PROTOTYPE_METHOD(s_ct, isVoid);
    LLVM_SET_PROTOTYPE_METHOD(s_ct, dump);
#undef LLVM_SET_METHOD
#undef LLVM_SET_PROTOTYPE_METHOD

    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", Type::ToString);

    s_func = Persistent<Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("Type"),
		s_func);
  }

#define LLVM_TYPE_METHOD_PROXY(name) LLVM_TYPE_METHOD(name,name)
#define LLVM_TYPE_METHOD(name,llvm_ty)					\
  Handle<Value> Type::name(const Arguments& args)			\
  {									\
    HandleScope scope;							\
    Local<Object> result = s_func->NewInstance();			\
    Type* result_type = new Type(llvm::Type::llvm_ty(llvm::getGlobalContext())); \
    result_type->Wrap(result);						\
    return scope.Close(result);						\
  }

  LLVM_TYPE_METHOD_PROXY(getDoubleTy)
  LLVM_TYPE_METHOD_PROXY(getInt64Ty)
  LLVM_TYPE_METHOD_PROXY(getInt32Ty)
  LLVM_TYPE_METHOD_PROXY(getInt16Ty)
  LLVM_TYPE_METHOD_PROXY(getInt8Ty)
  LLVM_TYPE_METHOD_PROXY(getVoidTy)

#undef LLVM_TYPE_METHOD_PROXY
#undef LLVM_TYPE_METHOD

  Handle<Value> Type::New(llvm::Type *ty)
  {
    HandleScope scope;
    Local<Object> new_instance = Type::s_func->NewInstance();
    Type* new_type = new Type(ty);
    new_type->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<Value> Type::New(const Arguments& args)
  {
    HandleScope scope;
    Type* type = new Type();
    type->Wrap(args.This());
    return args.This();
  }

  Handle<Value> Type::pointerTo(const Arguments& args)
  {
    HandleScope scope;
    Type* type = ObjectWrap::Unwrap<Type>(args.This());
    Local<Object> new_instance = s_func->NewInstance();
    Type* new_type = new Type(type->llvm_ty->getPointerTo());
    new_type->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<Value> Type::isVoid(const Arguments& args)
  {
    HandleScope scope;
    Type* type = ObjectWrap::Unwrap<Type>(args.This());
    Handle<Boolean> result = v8::Boolean::New(type->llvm_ty->isVoidTy());
    return scope.Close(result);
  }

  Handle<Value> Type::ToString(const Arguments& args)
  {
    HandleScope scope;
    Type* type = ObjectWrap::Unwrap<Type>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    type->llvm_ty->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Handle<Value> Type::dump(const Arguments& args)
  {
    HandleScope scope;
    Type* type = ObjectWrap::Unwrap<Type>(args.This());
    type->llvm_ty->dump();
    return scope.Close(Undefined());
  }

  Type::Type(llvm::Type *llvm_ty) : llvm_ty(llvm_ty)
  {
  }

  Type::Type() : llvm_ty(NULL)
  {
  }

  Type::~Type()
  {
  }

  v8::Persistent<v8::FunctionTemplate> Type::s_ct;
  v8::Persistent<v8::Function> Type::s_func;
};
