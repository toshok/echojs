#include "node-llvm.h"
#include "type.h"
#include "callinvoke.h"
#include "value.h"
#include "basicblock.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void Call::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("CallInst"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", Call::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", Call::ToString);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setOnlyReadsMemory", Call::SetOnlyReadsMemory);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setDoesNotAccessMemory", Call::SetDoesNotAccessMemory);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setDoesNotThrow", Call::SetDoesNotThrow);

    s_func = Persistent< ::v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("CallInst"),
		s_func);
  }

  Call::Call(llvm::CallInst *llvm_call) : llvm_call(llvm_call)
  {
  }

  Call::Call() : llvm_call(NULL)
  {
  }

  Call::~Call()
  {
  }

  Handle<v8::Value> Call::New(const Arguments& args)
  {
    HandleScope scope;
    return args.This();
  }

  Handle<v8::Value> Call::New(llvm::CallInst *llvm_call)
  {
    HandleScope scope;
    Local<Object> new_instance = Call::s_func->NewInstance();
    Call* new_call = new Call(llvm_call);
    new_call->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> Call::Dump (const Arguments& args)
  {
    HandleScope scope;
    Call* _call = ObjectWrap::Unwrap<Call>(args.This());
    _call->llvm_call->dump();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> Call::ToString(const Arguments& args)
  {
    HandleScope scope;
    Call* _call = ObjectWrap::Unwrap<Call>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    _call->llvm_call->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Handle<v8::Value> Call::SetOnlyReadsMemory(const v8::Arguments& args)
  {
    HandleScope scope;
    Call* _call = ObjectWrap::Unwrap<Call>(args.This());
    _call->llvm_call->setOnlyReadsMemory();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> Call::SetDoesNotAccessMemory(const v8::Arguments& args)
  {
    HandleScope scope;
    Call* _call = ObjectWrap::Unwrap<Call>(args.This());
    _call->llvm_call->setDoesNotAccessMemory();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> Call::SetDoesNotThrow(const v8::Arguments& args)
  {
    HandleScope scope;
    Call* _call = ObjectWrap::Unwrap<Call>(args.This());
    _call->llvm_call->setDoesNotThrow();
    return scope.Close(Undefined());
  }

  Persistent<FunctionTemplate> Call::s_ct;
  Persistent<Function> Call::s_func;



  void Invoke::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("InvokeInst"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", Invoke::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", Invoke::ToString);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setOnlyReadsMemory", Invoke::SetOnlyReadsMemory);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setDoesNotAccessMemory", Invoke::SetDoesNotAccessMemory);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setDoesNotThrow", Invoke::SetDoesNotThrow);

    s_func = Persistent< ::v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("InvokeInst"),
		s_func);
  }

  Invoke::Invoke(llvm::InvokeInst *llvm_invoke) : llvm_invoke(llvm_invoke)
  {
  }

  Invoke::Invoke() : llvm_invoke(NULL)
  {
  }

  Invoke::~Invoke()
  {
  }

  Handle<v8::Value> Invoke::New(const Arguments& args)
  {
    HandleScope scope;
    return args.This();
  }

  Handle<v8::Value> Invoke::New(llvm::InvokeInst *llvm_invoke)
  {
    HandleScope scope;
    Local<Object> new_instance = Invoke::s_func->NewInstance();
    Invoke* new_invoke = new Invoke(llvm_invoke);
    new_invoke->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> Invoke::Dump (const Arguments& args)
  {
    HandleScope scope;
    Invoke* _invoke = ObjectWrap::Unwrap<Invoke>(args.This());
    _invoke->llvm_invoke->dump();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> Invoke::ToString(const Arguments& args)
  {
    HandleScope scope;
    Invoke* _invoke = ObjectWrap::Unwrap<Invoke>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    _invoke->llvm_invoke->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Handle<v8::Value> Invoke::SetOnlyReadsMemory(const v8::Arguments& args)
  {
    HandleScope scope;
    Invoke* _invoke = ObjectWrap::Unwrap<Invoke>(args.This());
    _invoke->llvm_invoke->setOnlyReadsMemory();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> Invoke::SetDoesNotAccessMemory(const v8::Arguments& args)
  {
    HandleScope scope;
    Invoke* _invoke = ObjectWrap::Unwrap<Invoke>(args.This());
    _invoke->llvm_invoke->setDoesNotAccessMemory();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> Invoke::SetDoesNotThrow(const v8::Arguments& args)
  {
    HandleScope scope;
    Invoke* _invoke = ObjectWrap::Unwrap<Invoke>(args.This());
    _invoke->llvm_invoke->setDoesNotThrow();
    return scope.Close(Undefined());
  }

  Persistent<FunctionTemplate> Invoke::s_ct;
  Persistent<Function> Invoke::s_func;
};


