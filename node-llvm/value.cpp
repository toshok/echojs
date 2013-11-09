#include "node-llvm.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void Value::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("Value"));

    NODE_SET_PROTOTYPE_METHOD (s_ct, "dump", Value::Dump);
    NODE_SET_PROTOTYPE_METHOD (s_ct, "setName", Value::SetName);
    NODE_SET_PROTOTYPE_METHOD (s_ct, "toString", Value::ToString);

    s_func = Persistent<Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("Value"),
		s_func);
  }

  Value::Value(llvm::Value *llvm_val) : llvm_val(llvm_val)
  {
  }

  Value::Value() : llvm_val(NULL)
  {
  }

  Value::~Value()
  {
  }

  Handle<v8::Value> Value::New(llvm::Value *llvm_val)
  {
    HandleScope scope;
    Local<Object> new_instance = Value::s_func->NewInstance();
    Value* new_val = new Value(llvm_val);
    new_val->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle< ::v8::Value> Value::New(const Arguments& args)
  {
    HandleScope scope;
    Value* val = new Value();
    val->Wrap(args.This());
    return args.This();
  }

  Handle< ::v8::Value> Value::Dump(const Arguments& args)
  {
    HandleScope scope;
    Value* val = ObjectWrap::Unwrap<Value>(args.This());
    val->llvm_val->dump();
    return scope.Close(Undefined());
  }

  Handle< ::v8::Value> Value::SetName(const Arguments& args)
  {
    HandleScope scope;
    Value* val = ObjectWrap::Unwrap<Value>(args.This());

    REQ_UTF8_ARG (0, name);
    val->llvm_val->setName(*name);
    return scope.Close(Undefined());
  }

  Handle< ::v8::Value> Value::ToString(const Arguments& args)
  {
    HandleScope scope;
    Value* val = ObjectWrap::Unwrap<Value>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    val->llvm_val->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Persistent<FunctionTemplate> Value::s_ct;
  Persistent<Function> Value::s_func;

};
