#include "node-llvm.h"
#include "constantarray.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void ConstantArray::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("ConstantArray"));

    s_func = Persistent< ::v8::Function>::New(s_ct->GetFunction());

    NODE_SET_METHOD(s_func, "get", ConstantArray::Get);

    target->Set(String::NewSymbol("ConstantArray"), s_func);
  }

  ConstantArray::ConstantArray()
  {
  }

  ConstantArray::~ConstantArray()
  {
  }

  Handle<v8::Value> ConstantArray::New(const Arguments& args)
  {
    return ThrowException(Exception::Error(String::New("ConstantArray is not meant to be instantiated.")));
  }

  Handle<v8::Value> ConstantArray::Get (const Arguments& args)
  {
    HandleScope scope;
    REQ_LLVM_TYPE_ARG(0, array_type);
    REQ_ARRAY_ARG(1, elements);

    std::vector< llvm::Constant*> element_constants;
    for (uint32_t i = 0; i < elements->Length(); i ++) {
      element_constants.push_back (static_cast<llvm::Constant*>(Value::GetLLVMObj(elements->Get(i))));
    }

    Handle<v8::Value> result = Value::New(llvm::ConstantArray::get(static_cast<llvm::ArrayType*>(array_type), element_constants));
    return scope.Close(result);
  }

  Persistent<FunctionTemplate> ConstantArray::s_ct;
  Persistent< ::v8::Function> ConstantArray::s_func;
};
