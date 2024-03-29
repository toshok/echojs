#include "node-llvm.h"
#include "type.h"
#include "landingpad.h"
#include "value.h"
#include "basicblock.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  NAN_MODULE_INIT(LandingPad::Init) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("LandingPadInst").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "dump", LandingPad::Dump);
    Nan::SetPrototypeMethod(ctor, "toString", LandingPad::ToString);
    Nan::SetPrototypeMethod(ctor, "setCleanup", LandingPad::SetCleanup);
    Nan::SetPrototypeMethod(ctor, "addClause", LandingPad::AddClause);

    v8::Isolate *isolate = target->GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    

    Local<v8::Function> ctor_func = ctor->GetFunction(context).ToLocalChecked();
    constructor_func.Reset(ctor_func);
    target->Set(context, Nan::New("LandingPadInst").ToLocalChecked(), ctor_func).Check();
  }

  NAN_METHOD(LandingPad::New) {
    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(LandingPad::Dump) {
    auto landing_pad = Unwrap(info.This());
    landing_pad->llvm_obj->dump();
  }

  NAN_METHOD(LandingPad::ToString) {
    auto landing_pad = Unwrap(info.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    landing_pad->llvm_obj->print(str_ostream);

    info.GetReturnValue().Set(Nan::New(trim(str_ostream.str()).c_str()).ToLocalChecked());
  }

  NAN_METHOD(LandingPad::SetCleanup) {
    v8::Isolate *isolate = info.GetIsolate();
    auto landing_pad = Unwrap(info.This());
    REQ_BOOL_ARG(isolate, 0, flag);
    landing_pad->llvm_obj->setCleanup(flag);
  }

  NAN_METHOD(LandingPad::AddClause) {
    v8::Isolate *isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();    
    auto landing_pad = Unwrap(info.This());
    REQ_LLVM_VAL_ARG(context, 0, clause_val);
    landing_pad->llvm_obj->addClause(llvm::cast<llvm::Constant>(clause_val));
  }

  Nan::Persistent<v8::FunctionTemplate> LandingPad::constructor;
  Nan::Persistent<v8::Function> LandingPad::constructor_func;
}


