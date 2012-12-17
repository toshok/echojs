#include "node-llvm.h"
#include "type.h"
#include "function.h"
#include "functiontype.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void Function::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("LLVMFunction"));

    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("args"), Function::GetArgs);
    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("argSize"), Function::GetArgSize);
    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("returnType"), Function::GetReturnType);
    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("type"), Function::GetType);
    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("doesNotThrow"), Function::GetDoesNotThrow);

    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", Function::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setOnlyReadsMemory", Function::SetOnlyReadsMemory);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setDoesNotThrow", Function::SetDoesNotThrow);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setGC", Function::SetGC);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", Function::ToString);

    s_func = Persistent< ::v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("LLVMFunction"),
		s_func);
  }

  Function::Function(llvm::Function *llvm_fun) : llvm_fun(llvm_fun)
  {
  }

  Function::Function() : llvm_fun(NULL)
  {
  }

  Function::~Function()
  {
  }

  Handle<v8::Value> Function::New(const Arguments& args)
  {
    HandleScope scope;
    return scope.Close(args.This());
  }

  Handle<v8::Value> Function::New(llvm::Function *llvm_fun)
  {
    HandleScope scope;
    Local<Object> new_instance = Function::s_func->NewInstance();
    Function* new_fun = new Function(llvm_fun);
    new_fun->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> Function::Dump (const Arguments& args)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(args.This());
    fun->llvm_fun->dump();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> Function::SetOnlyReadsMemory (const Arguments& args)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(args.This());
    fun->llvm_fun->setOnlyReadsMemory();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> Function::SetDoesNotThrow (const Arguments& args)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(args.This());
    fun->llvm_fun->setDoesNotThrow();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> Function::SetGC (const Arguments& args)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(args.This());
    REQ_UTF8_ARG(0, name);
    fun->llvm_fun->setGC(*name);
    return scope.Close(Undefined());
  }

  Handle<v8::Value> Function::ToString(const Arguments& args)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    fun->llvm_fun->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Handle<v8::Value> Function::GetArgSize (Local<String> property, const AccessorInfo& info)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(info.This());
    Handle<Integer> result = v8::Integer::New(fun->llvm_fun->arg_size());
    return scope.Close(result);
  }

  Handle<v8::Value> Function::GetArgs (Local<String> property, const AccessorInfo& info)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(info.This());
    int size = fun->llvm_fun->arg_size();
    Local<Array> result = v8::Array::New(size);

    unsigned Idx = 0;
    for (llvm::Function::arg_iterator AI = fun->llvm_fun->arg_begin(); Idx != size;
	 ++AI, ++Idx) {
      result->Set(Idx, Value::New(AI));
    }

    return scope.Close(result);
  }

  Handle<v8::Value> Function::GetReturnType (Local<String> property, const AccessorInfo& info)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(info.This());
    Handle<v8::Value> result = Type::New(fun->llvm_fun->getReturnType());
    return scope.Close(result);
  }

  Handle<v8::Value> Function::GetType (Local<String> property, const AccessorInfo& info)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(info.This());
    Handle<v8::Value> result = FunctionType::New(fun->llvm_fun->getFunctionType());
    return scope.Close(result);
  }

  Handle<v8::Value> Function::GetDoesNotThrow (Local<String> property, const AccessorInfo& info)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(info.This());
    Handle<v8::Value> result = Boolean::New(fun->llvm_fun->doesNotThrow());
    return scope.Close(result);
  }

  Persistent<FunctionTemplate> Function::s_ct;
  Persistent< ::v8::Function> Function::s_func;

};
