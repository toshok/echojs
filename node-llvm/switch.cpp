#include "node-llvm.h"
#include "type.h"
#include "switch.h"
#include "value.h"
#include "basicblock.h"

using namespace node;
using namespace v8;

namespace jsllvm {

void Switch::Init(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("SwitchInst"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", Switch::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", Switch::ToString);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "addCase", Switch::AddCase);

    s_func = Persistent<::v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("SwitchInst"), s_func);
}

Switch::Switch(llvm::SwitchInst *llvm_switch) : llvm_switch(llvm_switch) {}

Switch::Switch() : llvm_switch(NULL) {}

Switch::~Switch() {}

Handle<v8::Value> Switch::New(const Arguments &args) {
    HandleScope scope;
    return args.This();
}

Handle<v8::Value> Switch::New(llvm::SwitchInst *llvm_switch) {
    HandleScope scope;
    Local<Object> new_instance = Switch::s_func->NewInstance();
    Switch *new_switch = new Switch(llvm_switch);
    new_switch->Wrap(new_instance);
    return scope.Close(new_instance);
}

Handle<v8::Value> Switch::Dump(const Arguments &args) {
    HandleScope scope;
    Switch *_switch = ObjectWrap::Unwrap<Switch>(args.This());
    _switch->llvm_switch->dump();
    return scope.Close(Undefined());
}

Handle<v8::Value> Switch::ToString(const Arguments &args) {
    HandleScope scope;
    Switch *_switch = ObjectWrap::Unwrap<Switch>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    _switch->llvm_switch->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
}

Handle<v8::Value> Switch::AddCase(const Arguments &args) {
    HandleScope scope;
    Switch *_switch = ObjectWrap::Unwrap<Switch>(args.This());
    REQ_LLVM_VAL_ARG(0, OnVal);
    REQ_LLVM_BB_ARG(1, Dest);
    _switch->llvm_switch->addCase(static_cast<llvm::ConstantInt *>(OnVal),
                                  Dest);
    return scope.Close(Undefined());
}

Persistent<FunctionTemplate> Switch::s_ct;
Persistent<Function> Switch::s_func;
};
