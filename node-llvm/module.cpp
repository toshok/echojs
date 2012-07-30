#include <fstream>
#include "node-llvm.h"
#include "type.h"
#include "module.h"
#include "function.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void Module::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("Module"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "getOrInsertFunction", Module::GetOrInsertFunction);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "getOrInsertExternalFunction", Module::GetOrInsertExternalFunction);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "getFunction", Module::GetFunction);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", Module::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "writeToFile", Module::WriteToFile);

    s_func = Persistent< ::v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("Module"),
		s_func);
  }

  Module::Module(llvm::Module *llvm_module) : llvm_module(llvm_module)
  {
  }

  Module::Module() : llvm_module(NULL)
  {
  }

  Module::~Module()
  {
  }

  Handle<Value> Module::New(llvm::Module *llvm_module)
  {
    HandleScope scope;
    Local<Object> new_instance = Module::s_func->NewInstance();
    Module* new_module = new Module(llvm_module);
    new_module->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<Value> Module::New(const Arguments& args)
  {
    HandleScope scope;
    if (args.Length()) {
      REQ_UTF8_ARG(0, name);
      printf ("name = %s\n", *name);
      Module* module = new Module(new llvm::Module(*name, llvm::getGlobalContext()));
      module->Wrap(args.This());
    }
    return scope.Close(args.This());
  }

  Handle<Value> Module::GetOrInsertFunction (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());

    REQ_UTF8_ARG(0, name);
    REQ_INT_ARG(1, fun_argc);

    std::vector< llvm::Type*> arg_types;
    for (int i = 0; i < fun_argc; i ++) {
      arg_types.push_back (/*XXX EjsValueType*/llvm::Type::getInt32Ty(llvm::getGlobalContext())->getPointerTo());
    }

    llvm::FunctionType *FT = llvm::FunctionType::get(/*XXX EjsValueType*/llvm::Type::getInt32Ty(llvm::getGlobalContext())->getPointerTo(),
							 arg_types, false);

    llvm::Function* f = static_cast< llvm::Function*>(module->llvm_module->getOrInsertFunction(*name, FT));

    // XXX this needs to come from the js call, since when we hoist anonymous methods we'll need to give them a private linkage.
    f->setLinkage (llvm::Function::ExternalLinkage);

    // XXX the args might not be identifiers but might instead be destructuring expressions.  punt for now.

#if notyet
    // Set names for all arguments.
    unsigned Idx = 0;
    for (Function::arg_iterator AI = F->arg_begin(); Idx != Args.size();
	 ++AI, ++Idx)
      AI->setName(Args[Idx]);
#endif

    Handle<Value> result = Function::New(f);
    return scope.Close(result);
  }

  Handle<Value> Module::GetOrInsertExternalFunction (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());

    REQ_UTF8_ARG(0, name);
    REQ_LLVM_TYPE_ARG(1, return_type);

    std::vector< llvm::Type*> arg_types;
    for (int i = 0, e = args.Length() - 2; i < e; i ++) {
      arg_types.push_back (jsllvm::Type::GetLLVMObj(args[i+2]));
    }
    llvm::FunctionType *FT = llvm::FunctionType::get(return_type, arg_types, false);

    llvm::Function* f = static_cast< llvm::Function*>(module->llvm_module->getOrInsertFunction(*name, FT));
    f->setLinkage (llvm::Function::ExternalLinkage);

    Handle<Value> result = Function::New(f);
    return scope.Close(result);
  }

  Handle<Value> Module::GetFunction (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());

    REQ_UTF8_ARG(0, name);
	
    llvm::Function* f = static_cast< llvm::Function*>(module->llvm_module->getFunction(*name));

    Handle<Value> result = Function::New(f);
    return scope.Close(result);
  }

  Handle<Value> Module::Dump (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());
    module->llvm_module->dump();
    return scope.Close(Undefined());
  }

  Handle<Value> Module::WriteToFile (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());

    REQ_UTF8_ARG(0, path);

    std::ofstream output_file(*path);
    llvm::raw_os_ostream raw_stream(output_file);
    module->llvm_module->print(raw_stream, NULL);
    output_file.close();

    return scope.Close(Undefined());
  }

  Persistent<FunctionTemplate> Module::s_ct;
  Persistent< ::v8::Function> Module::s_func;
};
