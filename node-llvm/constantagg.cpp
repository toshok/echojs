#include "node-llvm.h"
#include "constantagg.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;

namespace jsllvm {

void ConstantAggregateZero::Init(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("ConstantAggregateZero"));

    s_func = Persistent<::v8::Function>::New(s_ct->GetFunction());

    NODE_SET_METHOD(s_func, "get", ConstantAggregateZero::Get);

    target->Set(String::NewSymbol("ConstantAggregateZero"), s_func);
}

ConstantAggregateZero::ConstantAggregateZero() {}

ConstantAggregateZero::~ConstantAggregateZero() {}

Handle<v8::Value> ConstantAggregateZero::New(const Arguments &args) {
    return ThrowException(Exception::Error(
        String::New("ConstantAggregateZero is not meant to be instantiated.")));
}

Handle<v8::Value> ConstantAggregateZero::Get(const Arguments &args) {
    HandleScope scope;
    REQ_LLVM_TYPE_ARG(0, ty);
    Handle<v8::Value> result = Value::New(llvm::ConstantAggregateZero::get(ty));
    return scope.Close(result);
}

Persistent<FunctionTemplate> ConstantAggregateZero::s_ct;
Persistent<::v8::Function> ConstantAggregateZero::s_func;
};
