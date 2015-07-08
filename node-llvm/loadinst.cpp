#include "node-llvm.h"
#include "type.h"
#include "value.h"
#include "basicblock.h"
#include "instruction.h"
#include "loadinst.h"

using namespace node;
using namespace v8;

namespace jsllvm {

void LoadInst::Init(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->Inherit(Instruction::s_ct);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("LoadInst"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", LoadInst::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", LoadInst::ToString);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "setAlignment", LoadInst::SetAlignment);

    s_func = Persistent<::v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("LoadInst"), s_func);
}

LoadInst::LoadInst(llvm::LoadInst *llvm_load) : llvm_load(llvm_load) {}

LoadInst::LoadInst() : llvm_load(NULL) {}

LoadInst::~LoadInst() {}

Handle<v8::Value> LoadInst::New(const Arguments &args) {
    HandleScope scope;
    LoadInst *a = new LoadInst();
    a->Wrap(args.This());
    return args.This();
}

Handle<v8::Value> LoadInst::New(llvm::LoadInst *llvm_load) {
    HandleScope scope;
    Local<Object> new_instance = LoadInst::s_func->NewInstance();
    LoadInst *new_a = new LoadInst(llvm_load);
    new_a->Wrap(new_instance);
    return scope.Close(new_instance);
}

Handle<v8::Value> LoadInst::Dump(const Arguments &args) {
    HandleScope scope;
    LoadInst *a = ObjectWrap::Unwrap<LoadInst>(args.This());
    a->llvm_load->dump();
    return scope.Close(Undefined());
}

Handle<v8::Value> LoadInst::ToString(const Arguments &args) {
    HandleScope scope;
    LoadInst *a = ObjectWrap::Unwrap<LoadInst>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    a->llvm_load->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
}

Handle<::v8::Value> LoadInst::SetAlignment(const Arguments &args) {
    HandleScope scope;
    LoadInst *val = ObjectWrap::Unwrap<LoadInst>(args.This());

    REQ_INT_ARG(0, alignment);

    val->llvm_load->setAlignment(alignment);
    return scope.Close(Undefined());
}

Persistent<FunctionTemplate> LoadInst::s_ct;
Persistent<Function> LoadInst::s_func;
};
