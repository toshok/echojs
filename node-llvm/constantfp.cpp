#include "node-llvm.h"
#include "constantfp.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

void ConstantFP::Init(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("ConstantFP"));

    s_func = Persistent<::v8::Function>::New(s_ct->GetFunction());

    NODE_SET_METHOD(s_func, "getDouble", ConstantFP::GetDouble);

    target->Set(String::NewSymbol("ConstantFP"), s_func);
}

ConstantFP::ConstantFP() {}

ConstantFP::~ConstantFP() {}

Handle<v8::Value> ConstantFP::New(const Arguments &args) {
    return ThrowException(Exception::Error(
        String::New("ConstantFP is not meant to be instantiated.")));
}

Handle<v8::Value> ConstantFP::GetDouble(const Arguments &args) {
    HandleScope scope;
    REQ_DOUBLE_ARG(0, v);
    Handle<v8::Value> result = Value::New(
        llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(v)));
    return scope.Close(result);
}

Persistent<FunctionTemplate> ConstantFP::s_ct;
Persistent<::v8::Function> ConstantFP::s_func;
};
