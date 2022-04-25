#if notyet

#include "node-llvm.h"
#include "metadata.h"
#include "function.h"
#include "module.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;


namespace jsllvm {

  // MDNode

  NAN_MODULE_INIT(MDNode::Init) {
    HandleScope scope;

    Local<FunctionTemplate> ctor = FunctionTemplate::New(New);

    constructor.Reset(ctor);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("MDNode"));

    NODE_SET_METHOD(s_ct, "get", MDNode::Get);

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("MDNode"),
		s_func);
  }

  v8::Handle<v8::Value> MDNode::New(llvm::MDNode* llvm_mdnode)
  {
    HandleScope scope;
    Local<Object> new_instance = MDNode::s_func->NewInstance();
    MDNode* new_mdnode = new MDNode(llvm_mdnode);
    new_mdnode->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> MDNode::New(const Arguments& args)
  {
    HandleScope scope;
    return scope.Close(args.This());
  }

  MDNode::MDNode(llvm::MDNode* llvm_mdnode)
    : llvm_mdnode(llvm_mdnode)
  {
  }

  MDNode::~MDNode()
  {
  }

  Handle<v8::Value> MDNode::Get(const Arguments& args)
  {
    HandleScope scope;

    REQ_ARRAY_ARG(0, vals);

    std::vector<llvm::Value*> Vals;
    for (unsigned i = 0, e = vals->Length(); i != e; ++i) {
      llvm::Value* val = Value::GetLLVMObj(vals->Get(i));
      Vals.push_back(val);
      EJS_ASSERT(Vals.back() != 0); // XXX throw an exception here
    }

    return scope.Close(MDNode::New(llvm::MDNode::get(llvm::getGlobalContext(), Vals)));
  }

  Persistent<FunctionTemplate> MDNode::s_ct;
  Persistent<v8::Function> MDNode::s_func;


  // MDString


  NAN_MODULE_INIT(MDString::Init) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("MDString"));

    NODE_SET_METHOD(s_ct, "get", MDString::Get);

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("MDString"),
		s_func);
  }

  v8::Handle<v8::Value> MDString::New(llvm::MDString* llvm_mdstring)
  {
    HandleScope scope;
    Local<Object> new_instance = MDString::s_func->NewInstance();
    MDString* new_mdstring = new MDString(llvm_mdstring);
    new_mdstring->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> MDString::New(const Arguments& args)
  {
    HandleScope scope;
    return scope.Close(args.This());
  }

  MDString::MDString(llvm::MDString* llvm_mdstring)
    : llvm_mdstring(llvm_mdstring)
  {
  }

  MDString::~MDString()
  {
  }

  Handle<v8::Value> MDString::Get(const Arguments& args)
  {
    HandleScope scope;

    REQ_UTF8_ARG(0,str);

    return scope.Close(MDString::New(llvm::MDString::get(llvm::getGlobalContext(), *str)));
  }

  Persistent<FunctionTemplate> MDString::constructor;
  Persistent<v8::Function> MDString::constructor_func;

}

#endif
