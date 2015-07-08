#include "node-llvm.h"
#include "arraytype.h"
#include "type.h"

using namespace node;
using namespace v8;

namespace jsllvm {

void ArrayType::Init(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->Inherit(Type::s_ct);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("ArrayType"));

    NODE_SET_METHOD(s_ct, "get", ArrayType::Get);

    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", ArrayType::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", ArrayType::ToString);

    s_func = Persistent<Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("ArrayType"), s_func);
}

Handle<Value> ArrayType::Get(const Arguments &args) {
    HandleScope scope;

    REQ_LLVM_TYPE_ARG(0, elementType);
    REQ_INT_ARG(1, numElements);

    return scope.Close(
        ArrayType::New(llvm::ArrayType::get(elementType, numElements)));
}

Handle<Value> ArrayType::New(llvm::ArrayType *ty) {
    HandleScope scope;
    Local<Object> new_instance = ArrayType::s_func->NewInstance();
    ArrayType *new_type = new ArrayType(ty);
    new_type->Wrap(new_instance);
    return scope.Close(new_instance);
}

Handle<Value> ArrayType::New(const Arguments &args) {
    HandleScope scope;
    ArrayType *type = new ArrayType();
    type->Wrap(args.This());
    return args.This();
}

ArrayType::ArrayType(llvm::ArrayType *llvm_ty) : llvm_ty(llvm_ty) {}

ArrayType::ArrayType() : llvm_ty(NULL) {}

ArrayType::~ArrayType() {}

Handle<Value> ArrayType::ToString(const Arguments &args) {
    HandleScope scope;
    ArrayType *type = ObjectWrap::Unwrap<ArrayType>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    type->llvm_ty->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
}

Handle<Value> ArrayType::Dump(const Arguments &args) {
    HandleScope scope;
    ArrayType *type = ObjectWrap::Unwrap<ArrayType>(args.This());
    type->llvm_ty->dump();
    return scope.Close(Undefined());
}

v8::Persistent<v8::FunctionTemplate> ArrayType::s_ct;
v8::Persistent<v8::Function> ArrayType::s_func;
};
