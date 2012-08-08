#include "node-llvm.h"
#include "structtype.h"
#include "type.h"

using namespace node;
using namespace v8;


namespace jsllvm {


  void StructType::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->Inherit(Type::s_ct);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("StructType"));


    NODE_SET_METHOD(s_ct, "create", StructType::Create);

    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", StructType::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", StructType::ToString);

    s_func = Persistent<Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("StructType"),
		s_func);
  }

  Handle<Value> StructType::Create(const Arguments& args)
  {
    HandleScope scope;

    REQ_UTF8_ARG (0, name);
    REQ_ARRAY_ARG (1, elementTypes)

    std::vector< llvm::Type*> element_types;
    for (int i = 0; i < elementTypes->Length(); i ++) {
      element_types.push_back (Type::GetLLVMObj(elementTypes->Get(i)));
    }

    return scope.Close(StructType::New(llvm::StructType::create(llvm::getGlobalContext(), element_types, *name)));
  }

  Handle<Value> StructType::New(llvm::StructType *ty)
  {
    HandleScope scope;
    Local<Object> new_instance = StructType::s_func->NewInstance();
    StructType* new_type = new StructType(ty);
    new_type->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<Value> StructType::New(const Arguments& args)
  {
    HandleScope scope;
    StructType* type = new StructType();
    type->Wrap(args.This());
    return args.This();
  }

  StructType::StructType(llvm::StructType *llvm_ty) : llvm_ty(llvm_ty)
  {
  }

  StructType::StructType() : llvm_ty(NULL)
  {
  }

  StructType::~StructType()
  {
  }

  Handle<Value> StructType::ToString(const Arguments& args)
  {
    HandleScope scope;
    StructType* type = ObjectWrap::Unwrap<StructType>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    type->llvm_ty->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Handle<Value> StructType::Dump(const Arguments& args)
  {
    HandleScope scope;
    StructType* type = ObjectWrap::Unwrap<StructType>(args.This());
    type->llvm_ty->dump();
    return scope.Close(Undefined());
  }

  v8::Persistent<v8::FunctionTemplate> StructType::s_ct;
  v8::Persistent<v8::Function> StructType::s_func;
};
