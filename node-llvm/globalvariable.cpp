#include "node-llvm.h"
#include "value.h"
#include "module.h"
#include "type.h"
#include "globalvariable.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void GlobalVariable::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("GlobalVariable"));

    NODE_SET_PROTOTYPE_METHOD (s_ct, "dump", GlobalVariable::Dump);
    NODE_SET_PROTOTYPE_METHOD (s_ct, "setInitializer", GlobalVariable::SetInitializer);
    NODE_SET_PROTOTYPE_METHOD (s_ct, "setAlignment", GlobalVariable::SetAlignment);
    NODE_SET_PROTOTYPE_METHOD (s_ct, "toString", GlobalVariable::ToString);

    s_func = Persistent<Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("GlobalVariable"),
		s_func);
  }

  GlobalVariable::GlobalVariable(llvm::GlobalVariable *llvm_global) : llvm_global(llvm_global)
  {
  }

  GlobalVariable::GlobalVariable() : llvm_global(NULL)
  {
  }

  GlobalVariable::~GlobalVariable()
  {
  }

  Handle<v8::Value> GlobalVariable::New(llvm::GlobalVariable *llvm_global)
  {
    HandleScope scope;
    Local<Object> new_instance = GlobalVariable::s_func->NewInstance();
    GlobalVariable* new_val = new GlobalVariable(llvm_global);
    new_val->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle< ::v8::Value> GlobalVariable::New(const Arguments& args)
  {
    HandleScope scope;
    if (args.Length()) {
      REQ_LLVM_MODULE_ARG(0, module);
      REQ_LLVM_TYPE_ARG(1, type);
      REQ_UTF8_ARG(2, name);
      REQ_NULLABLE_LLVM_CONST_ARG(3, init);
      REQ_BOOL_ARG(4, visible);

      GlobalVariable* val = new GlobalVariable(new ::llvm::GlobalVariable(*module, type, false, visible ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::InternalLinkage, init, *name));

      val->Wrap(args.This());
    }
    return args.This();
  }

  Handle< ::v8::Value> GlobalVariable::Dump(const Arguments& args)
  {
    HandleScope scope;
    GlobalVariable* val = ObjectWrap::Unwrap<GlobalVariable>(args.This());
    val->llvm_global->dump();
    return scope.Close(Undefined());
  }

  Handle< ::v8::Value> GlobalVariable::SetInitializer(const Arguments& args)
  {
    HandleScope scope;
    GlobalVariable* val = ObjectWrap::Unwrap<GlobalVariable>(args.This());

    REQ_LLVM_CONST_ARG (0, init);

    val->llvm_global->setInitializer(init);
    return scope.Close(Undefined());
  }

  Handle< ::v8::Value> GlobalVariable::SetAlignment(const Arguments& args)
  {
    HandleScope scope;
    GlobalVariable* val = ObjectWrap::Unwrap<GlobalVariable>(args.This());

    REQ_INT_ARG (0, alignment);

    val->llvm_global->setAlignment(alignment);
    return scope.Close(Undefined());
  }

  Handle< ::v8::Value> GlobalVariable::ToString(const Arguments& args)
  {
    HandleScope scope;
    GlobalVariable* val = ObjectWrap::Unwrap<GlobalVariable>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    val->llvm_global->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Persistent<FunctionTemplate> GlobalVariable::s_ct;
  Persistent<Function> GlobalVariable::s_func;

};
