#include <fstream>
#include "node-llvm.h"
#include "type.h"
#include "module.h"
#include "function.h"
#include "value.h"
#include "globalvariable.h"

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

    NODE_SET_PROTOTYPE_METHOD(s_ct, "getGlobalVariable", Module::GetGlobalVariable);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "getOrInsertIntrinsic", Module::GetOrInsertIntrinsic);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "getOrInsertFunction", Module::GetOrInsertFunction);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "getOrInsertGlobal", Module::GetOrInsertGlobal);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "getOrInsertExternalFunction", Module::GetOrInsertExternalFunction);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "getFunction", Module::GetFunction);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", Module::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", Module::ToString);
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

  Handle<v8::Value> Module::New(llvm::Module *llvm_module)
  {
    HandleScope scope;
    Local<Object> new_instance = Module::s_func->NewInstance();
    Module* new_module = new Module(llvm_module);
    new_module->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> Module::New(const Arguments& args)
  {
    HandleScope scope;
    if (args.Length()) {
      REQ_UTF8_ARG(0, name);
      Module* module = new Module(new llvm::Module(std::string(*name), llvm::getGlobalContext()));
      module->Wrap(args.This());
    }
    return scope.Close(args.This());
  }

  Handle<v8::Value> Module::GetOrInsertIntrinsic (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());

    REQ_UTF8_ARG(0, id);
#if false
    REQ_ARRAY_ARG(1, paramTypes);

    std::vector< llvm::Type*> param_types;
    for (int i = 0; i < paramTypes->Length(); i ++) {
      param_types.push_back (Type::GetLLVMObj(paramTypes->Get(i)));
    }
#endif
    char *idstr = *id;
    llvm::Intrinsic::ID intrinsic_id;

    if (!strcmp (idstr, "@llvm.gcroot")) {
      intrinsic_id = llvm::Intrinsic::gcroot;
    }
    else {
      abort();
    }

#if false
    llvm::Function* f = llvm::Intrinsic::getDeclaration (module->llvm_module, intrinsic_id, param_types);
#else
    llvm::Function* f = llvm::Intrinsic::getDeclaration (module->llvm_module, intrinsic_id);
#endif

    Handle<v8::Value> result = Function::New(f);
    return scope.Close(result);
  }

  Handle<v8::Value> Module::GetOrInsertFunction (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());

    REQ_UTF8_ARG(0, name);
    REQ_LLVM_TYPE_ARG(1, returnType);
    REQ_ARRAY_ARG(2, paramTypes);

    std::vector< llvm::Type*> param_types;
    for (int i = 0; i < paramTypes->Length(); i ++) {
      param_types.push_back (Type::GetLLVMObj(paramTypes->Get(i)));
    }

    llvm::FunctionType *FT = llvm::FunctionType::get(returnType, param_types, false);

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

    Handle<v8::Value> result = Function::New(f);
    return scope.Close(result);
  }

  Handle<v8::Value> Module::GetGlobalVariable (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());

    REQ_UTF8_ARG(0, name);
    REQ_BOOL_ARG(1, allowInternal);

    return scope.Close(GlobalVariable::New(module->llvm_module->getGlobalVariable(*name, allowInternal)));
  }

  Handle<v8::Value> Module::GetOrInsertGlobal (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());

    REQ_UTF8_ARG(0, name);
    REQ_LLVM_TYPE_ARG(1, type);

    return scope.Close(GlobalVariable::New(static_cast<llvm::GlobalVariable*>(module->llvm_module->getOrInsertGlobal(*name, type))));
  }

  Handle<v8::Value> Module::GetOrInsertExternalFunction (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());

    REQ_UTF8_ARG(0, name);
    REQ_LLVM_TYPE_ARG(1, return_type);
    REQ_ARRAY_ARG(2, paramTypes);

    std::vector< llvm::Type*> param_types;
    for (int i = 0; i < paramTypes->Length(); i ++) {
      param_types.push_back (Type::GetLLVMObj(paramTypes->Get(i)));
    }

    llvm::FunctionType *FT = llvm::FunctionType::get(return_type, param_types, false);

    llvm::Function* f = static_cast< llvm::Function*>(module->llvm_module->getOrInsertFunction(*name, FT));
    f->setLinkage (llvm::Function::ExternalLinkage);

    Handle<v8::Value> result = Function::New(f);
    return scope.Close(result);
  }

  Handle<v8::Value> Module::GetFunction (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());

    REQ_UTF8_ARG(0, name);
	
    llvm::Function* f = static_cast< llvm::Function*>(module->llvm_module->getFunction(*name));

    if (f) {
      Handle<v8::Value> result = Function::New(f);
      return scope.Close(result);
    }
    return scope.Close(Null());
  }

  Handle<v8::Value> Module::Dump (const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());
    module->llvm_module->dump();
    return scope.Close(Undefined());
  }


  Handle<v8::Value> Module::ToString(const Arguments& args)
  {
    HandleScope scope;
    Module* module = ObjectWrap::Unwrap<Module>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    module->llvm_module->print(str_ostream, NULL);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Handle<v8::Value> Module::WriteToFile (const Arguments& args)
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
