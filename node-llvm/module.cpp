#include <fstream>
#include "node-llvm.h"
#include "type.h"
#include "module.h"
#include "function.h"
#include "value.h"
#include "globalvariable.h"
#include "metadata.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  NAN_MODULE_INIT(Module::Init) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("Module").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "getGlobalVariable", Module::GetGlobalVariable);
    Nan::SetPrototypeMethod(ctor, "getOrInsertIntrinsic", Module::GetOrInsertIntrinsic);
    Nan::SetPrototypeMethod(ctor, "getOrInsertFunction", Module::GetOrInsertFunction);
    Nan::SetPrototypeMethod(ctor, "getOrInsertGlobal", Module::GetOrInsertGlobal);
    Nan::SetPrototypeMethod(ctor, "getOrInsertExternalFunction", Module::GetOrInsertExternalFunction);
    Nan::SetPrototypeMethod(ctor, "getFunction", Module::GetFunction);
    Nan::SetPrototypeMethod(ctor, "dump", Module::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", Module::ToString);
    Nan::SetPrototypeMethod(ctor, "writeToFile", Module::WriteToFile);
    Nan::SetPrototypeMethod(ctor, "writeBitcodeToFile", Module::WriteBitcodeToFile);
    Nan::SetPrototypeMethod(ctor, "setDataLayout", Module::SetDataLayout);
    Nan::SetPrototypeMethod(ctor, "setTriple", Module::SetTriple);
    Nan::SetPrototypeMethod(ctor, "addModuleFlag", Module::AddModuleFlag);

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);
    target->Set(context, Nan::New("Module").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(Module::New) {
    if (info.Length()) {
      REQ_UTF8_ARG(context, 0, name);
      llvm::Module* llvm_module = new llvm::Module(*name, TheContext);
      llvm_module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);

#if notyet
      // Darwin only supports dwarf2.
      if (llvm::Triple(llvm::sys::getProcessTriple()).isOSDarwin())
#endif
      llvm_module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 2);

      Module* module = new Module(llvm_module);
      module->Wrap(info.This());
    }
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(Module::GetOrInsertIntrinsic) {
    auto module = Unwrap(info.This());

    REQ_UTF8_ARG(context, 0, id);
#if false
    REQ_ARRAY_ARG(context, 1, paramTypes);

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
      assert(0 && "shouldn't make it here");
    }

#if false
    llvm::Function* f = llvm::Intrinsic::getDeclaration (module->llvm_obj, intrinsic_id, param_types);
#else
    llvm::Function* f = llvm::Intrinsic::getDeclaration (module->llvm_obj, intrinsic_id);
#endif

    Local<v8::Value> result = Function::Create(f);
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(Module::GetOrInsertFunction) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    auto module = Unwrap(info.This());

    REQ_UTF8_ARG(context, 0, name);
    REQ_LLVM_TYPE_ARG(context, 1, returnType);
    REQ_ARRAY_ARG(context, 2, paramTypes);

    std::vector< llvm::Type*> param_types;
    for (uint32_t i = 0; i < paramTypes->Length(); i ++) {
      param_types.push_back (Type::GetLLVMObj(context, paramTypes->Get(context, i).ToLocalChecked()));
    }

    llvm::FunctionType *FT = llvm::FunctionType::get(returnType, param_types, false);

    llvm::Function* f = static_cast< llvm::Function*>(module->llvm_obj->getOrInsertFunction(*name, FT).getCallee());

    // XXX this needs to come from the js call, since when we hoist anonymous methods we'll need to give them a private linkage.
    f->setLinkage (llvm::Function::ExternalLinkage);

    //f->setCallingConv (llvm::CallingConv::ARM_AAPCS);

    // XXX the args might not be identifiers but might instead be destructuring expressions.  punt for now.

#if notyet
    // Set names for all arguments.
    unsigned Idx = 0;
    for (Function::arg_iterator AI = F->arg_begin(); Idx != Args.size();
	 ++AI, ++Idx)
      AI->setName(Args[Idx]);
#endif

    Local<v8::Value> result = Function::Create(f);
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(Module::GetGlobalVariable) {
    v8::Isolate *isolate = info.GetIsolate();
    auto module = Unwrap(info.This());

    REQ_UTF8_ARG(context, 0, name);
    REQ_BOOL_ARG(isolate, 1, allowInternal);

    info.GetReturnValue().Set(GlobalVariable::Create(module->llvm_obj->getGlobalVariable(*name, allowInternal)));
  }

  NAN_METHOD(Module::GetOrInsertGlobal) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    auto module = Unwrap(info.This());

    REQ_UTF8_ARG(context, 0, name);
    REQ_LLVM_TYPE_ARG(context, 1, type);

    info.GetReturnValue().Set(GlobalVariable::Create(static_cast<llvm::GlobalVariable*>(module->llvm_obj->getOrInsertGlobal(*name, type))));
  }

  NAN_METHOD(Module::GetOrInsertExternalFunction) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    auto module = Unwrap(info.This());

    REQ_UTF8_ARG(context, 0, name);
    REQ_LLVM_TYPE_ARG(context, 1, return_type);
    REQ_ARRAY_ARG(context, 2, paramTypes);

    std::vector< llvm::Type*> param_types;
    for (uint32_t i = 0; i < paramTypes->Length(); i ++) {
      param_types.push_back (Type::GetLLVMObj(context, paramTypes->Get(context, i).ToLocalChecked()));
    }

    llvm::FunctionType *FT = llvm::FunctionType::get(return_type, param_types, false);

    llvm::Function* f = static_cast< llvm::Function*>(module->llvm_obj->getOrInsertFunction(*name, FT).getCallee());
    f->setLinkage (llvm::Function::ExternalLinkage);
    //f->setCallingConv (llvm::CallingConv::ARM_AAPCS);

    info.GetReturnValue().Set(Function::Create(f));
  }

  NAN_METHOD(Module::GetFunction) {
    auto module = Unwrap(info.This());

    REQ_UTF8_ARG(context, 0, name);
	
    llvm::Function* f = static_cast< llvm::Function*>(module->llvm_obj->getFunction(*name));

    if (f) {
      info.GetReturnValue().Set(Function::Create(f));
      return;
    }
    info.GetReturnValue().Set(Nan::Null());
  }

  NAN_METHOD(Module::Dump) {
    auto module = Unwrap(info.This());
    module->llvm_obj->dump();
  }
  
  NAN_METHOD(Module::ToString) {
    auto module = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    module->llvm_obj->print(str_ostream, NULL);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(Module::WriteBitcodeToFile) {
    auto module = Unwrap(info.This());

    REQ_UTF8_ARG(context, 0, path);

    std::error_code error;
    llvm::raw_fd_ostream OS(*path, error, llvm::sys::fs::OpenFlags::OF_None);
    // check error

    llvm::WriteBitcodeToFile (*module->llvm_obj, OS);
  }

  NAN_METHOD(Module::WriteToFile) {
    auto module = Unwrap(info.This());

    REQ_UTF8_ARG(context, 0, path);

    std::ofstream output_file(*path);
    llvm::raw_os_ostream raw_stream(output_file);
    module->llvm_obj->print(raw_stream, NULL);
    output_file.close();
  }

  NAN_METHOD(Module::AddModuleFlag) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    auto module = Unwrap(info.This());

    REQ_LLVM_MDNODE_ARG(context, 0, node);

    module->llvm_obj->addModuleFlag (node);
  }
  
  NAN_METHOD(Module::SetDataLayout) {
    auto module = Unwrap(info.This());

    REQ_UTF8_ARG(context, 0, dataLayout);

    module->llvm_obj->setDataLayout (*dataLayout);
  }

  NAN_METHOD(Module::SetTriple) {
    auto module = Unwrap(info.This());

    REQ_UTF8_ARG(context, 0, triple);

    module->llvm_obj->setTargetTriple (*triple);
  }

  Nan::Persistent<v8::FunctionTemplate> Module::constructor;
  Nan::Persistent<v8::Function> Module::constructor_func;
}
