#include "node-llvm.h"
#include "type.h"
#include "value.h"
#include "basicblock.h"
#include "instruction.h"
#include "allocainst.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void AllocaInst::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->Inherit (Instruction::s_ct);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("AllocaInst"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", AllocaInst::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", AllocaInst::ToString);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setAlignment", AllocaInst::SetAlignment);

    s_func = Persistent< ::v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("AllocaInst"),
		s_func);
  }

  AllocaInst::AllocaInst(llvm::AllocaInst *llvm_alloca) : llvm_alloca(llvm_alloca)
  {
  }

  AllocaInst::AllocaInst() : llvm_alloca(NULL)
  {
  }

  AllocaInst::~AllocaInst()
  {
  }

  Handle<v8::Value> AllocaInst::New(const Arguments& args)
  {
    HandleScope scope;
    AllocaInst* a = new AllocaInst();
    a->Wrap(args.This());
    return args.This();
  }


  Handle<v8::Value> AllocaInst::New(llvm::AllocaInst *llvm_alloca)
  {
    HandleScope scope;
    Local<Object> new_instance = AllocaInst::s_func->NewInstance();
    AllocaInst* new_a = new AllocaInst(llvm_alloca);
    new_a->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> AllocaInst::Dump (const Arguments& args)
  {
    HandleScope scope;
    AllocaInst* a = ObjectWrap::Unwrap<AllocaInst>(args.This());
    a->llvm_alloca->dump();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> AllocaInst::ToString(const Arguments& args)
  {
    HandleScope scope;
    AllocaInst* a = ObjectWrap::Unwrap<AllocaInst>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    a->llvm_alloca->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Handle< ::v8::Value> AllocaInst::SetAlignment(const Arguments& args)
  {
    HandleScope scope;
    AllocaInst* val = ObjectWrap::Unwrap<AllocaInst>(args.This());

    REQ_INT_ARG (0, alignment);

    val->llvm_alloca->setAlignment(alignment);
    return scope.Close(Undefined());
  }

  Persistent<FunctionTemplate> AllocaInst::s_ct;
  Persistent<Function> AllocaInst::s_func;
};


