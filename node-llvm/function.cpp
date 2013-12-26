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
    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("name"), Function::GetName);
    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("returnType"), Function::GetReturnType);
    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("type"), Function::GetType);
    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("doesNotThrow"), Function::GetDoesNotThrow);
    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("onlyReadsMemory"), Function::GetOnlyReadsMemory);
    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("doesNotAccessMemory"), Function::GetDoesNotAccessMemory);

    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", Function::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setOnlyReadsMemory", Function::SetOnlyReadsMemory);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setDoesNotAccessMemory", Function::SetDoesNotAccessMemory);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setDoesNotThrow", Function::SetDoesNotThrow);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setGC", Function::SetGC);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setExternalLinkage", Function::SetInternalLinkage);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setInternalLinkage", Function::SetInternalLinkage);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", Function::ToString);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setStructRet", Function::SetStructRet);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "hasStructRetAttr", Function::HasStructRetAttr);

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
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

  Handle<v8::Value> Function::SetDoesNotAccessMemory (const Arguments& args)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(args.This());
    fun->llvm_fun->setDoesNotAccessMemory();
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

  Handle<v8::Value> Function::SetExternalLinkage (const Arguments& args)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(args.This());
    fun->llvm_fun->setLinkage (llvm::Function::ExternalLinkage);
    return scope.Close(Undefined());
  }

  Handle<v8::Value> Function::SetInternalLinkage (const Arguments& args)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(args.This());
    fun->llvm_fun->setLinkage (llvm::Function::InternalLinkage);
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

  Handle<v8::Value> Function::SetStructRet(const Arguments& args)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(args.This());

    fun->llvm_fun->addAttribute(1 /* first arg */,
				llvm::Attribute::StructRet);

    return scope.Close(Undefined());
  }

  Handle<v8::Value> Function::HasStructRetAttr (const Arguments& args)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(args.This());

    Handle<v8::Value> result = Boolean::New(fun->llvm_fun->hasStructRetAttr());

    return scope.Close(result);
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
    uint32_t size = fun->llvm_fun->arg_size();
    Local<Array> result = v8::Array::New(size);

    uint32_t Idx = 0;
    for (llvm::Function::arg_iterator AI = fun->llvm_fun->arg_begin(); Idx != size;
	 ++AI, ++Idx) {
      result->Set(Idx, Value::New(AI));
    }

    return scope.Close(result);
  }

  Handle<v8::Value> Function::GetName (Local<String> property, const AccessorInfo& info)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(info.This());
    std::string fun_name = fun->llvm_fun->getName();
    return scope.Close(String::New(fun_name.c_str()));
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

  Handle<v8::Value> Function::GetDoesNotAccessMemory (Local<String> property, const AccessorInfo& info)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(info.This());
    Handle<v8::Value> result = Boolean::New(fun->llvm_fun->doesNotAccessMemory());
    return scope.Close(result);
  }

  Handle<v8::Value> Function::GetOnlyReadsMemory (Local<String> property, const AccessorInfo& info)
  {
    HandleScope scope;
    Function* fun = ObjectWrap::Unwrap<Function>(info.This());
    Handle<v8::Value> result = Boolean::New(fun->llvm_fun->onlyReadsMemory());
    return scope.Close(result);
  }

  Persistent<FunctionTemplate> Function::s_ct;
  Persistent<v8::Function> Function::s_func;

};
