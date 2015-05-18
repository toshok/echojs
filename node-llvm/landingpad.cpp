#include "node-llvm.h"
#include "type.h"
#include "landingpad.h"
#include "value.h"
#include "basicblock.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void LandingPad::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("LandingPadInst"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", LandingPad::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", LandingPad::ToString);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setCleanup", LandingPad::SetCleanup);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "addClause", LandingPad::AddClause);

    s_func = Persistent< ::v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("LandingPadInst"),
		s_func);
  }

  LandingPad::LandingPad(llvm::LandingPadInst *llvm_landing_pad) : llvm_landing_pad(llvm_landing_pad)
  {
  }

  LandingPad::LandingPad() : llvm_landing_pad(NULL)
  {
  }

  LandingPad::~LandingPad()
  {
  }

  Handle<v8::Value> LandingPad::New(const Arguments& args)
  {
    HandleScope scope;
    return args.This();
  }

  Handle<v8::Value> LandingPad::New(llvm::LandingPadInst *llvm_landing_pad)
  {
    HandleScope scope;
    Local<Object> new_instance = LandingPad::s_func->NewInstance();
    LandingPad* new_landing_pad = new LandingPad(llvm_landing_pad);
    new_landing_pad->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> LandingPad::Dump (const Arguments& args)
  {
    HandleScope scope;
    LandingPad* landing_pad = ObjectWrap::Unwrap<LandingPad>(args.This());
    landing_pad->llvm_landing_pad->dump();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> LandingPad::ToString(const Arguments& args)
  {
    HandleScope scope;
    LandingPad* landing_pad = ObjectWrap::Unwrap<LandingPad>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    landing_pad->llvm_landing_pad->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Handle<v8::Value> LandingPad::SetCleanup (const Arguments& args)
  {
    HandleScope scope;
    LandingPad* landing_pad = ObjectWrap::Unwrap<LandingPad>(args.This());
    REQ_BOOL_ARG(0, flag);
    landing_pad->llvm_landing_pad->setCleanup(flag);
    return scope.Close(Undefined());
  }

  Handle<v8::Value> LandingPad::AddClause (const Arguments& args)
  {
    HandleScope scope;
    LandingPad* landing_pad = ObjectWrap::Unwrap<LandingPad>(args.This());
    REQ_LLVM_VAL_ARG(0, clause_val);
    landing_pad->llvm_landing_pad->addClause(llvm::cast<llvm::Constant>(clause_val));
    return scope.Close(Undefined());
  }

  Persistent<FunctionTemplate> LandingPad::s_ct;
  Persistent<Function> LandingPad::s_func;
};


