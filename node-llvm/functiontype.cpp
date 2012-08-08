#include "node-llvm.h"
#include "functiontype.h"
#include "type.h"

using namespace node;
using namespace v8;


namespace jsllvm {


  void FunctionType::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->Inherit(Type::s_ct);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("FunctionType"));


    NODE_SET_METHOD(s_ct, "get", FunctionType::Get);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", FunctionType::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", FunctionType::ToString);

    s_func = Persistent<Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("FunctionType"),
		s_func);
  }

  Handle<Value> FunctionType::Get(const Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_TYPE_ARG(0, returnType);
    REQ_ARRAY_ARG(1, argTypes);

    std::vector< llvm::Type*> arg_types;
    for (int i = 0; i < argTypes->Length(); i ++) {
      arg_types.push_back (Type::GetLLVMObj(argTypes->Get(i)));
    }

    ::llvm::FunctionType *FT = llvm::FunctionType::get(returnType,
						       arg_types, false);

    v8::Handle<v8::Value> result = FunctionType::New(FT);
    return scope.Close(result);
  }

  Handle<Value> FunctionType::New(llvm::FunctionType *ty)
  {
    HandleScope scope;
    Local<Object> new_instance = FunctionType::s_func->NewInstance();
    FunctionType* new_type = new FunctionType(ty);
    new_type->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<Value> FunctionType::New(const Arguments& args)
  {
    HandleScope scope;
    FunctionType* type = new FunctionType();
    type->Wrap(args.This());
    return args.This();
  }

  FunctionType::FunctionType(llvm::FunctionType *llvm_ty) : llvm_ty(llvm_ty)
  {
  }

  FunctionType::FunctionType() : llvm_ty(NULL)
  {
  }

  FunctionType::~FunctionType()
  {
  }

  Handle<Value> FunctionType::ToString(const Arguments& args)
  {
    HandleScope scope;
    FunctionType* type = ObjectWrap::Unwrap<FunctionType>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    type->llvm_ty->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Handle<Value> FunctionType::Dump(const Arguments& args)
  {
    HandleScope scope;
    FunctionType* type = ObjectWrap::Unwrap<FunctionType>(args.This());
    type->llvm_ty->dump();
    return scope.Close(Undefined());
  }

  v8::Persistent<v8::FunctionTemplate> FunctionType::s_ct;
  v8::Persistent<v8::Function> FunctionType::s_func;
};
