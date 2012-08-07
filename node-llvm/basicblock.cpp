#include "node-llvm.h"
#include "basicblock.h"
#include "function.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void BasicBlock::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("BasicBlock"));

    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("name"), BasicBlock::GetName);
    s_ct->InstanceTemplate()->SetAccessor(String::NewSymbol("parent"), BasicBlock::GetParent);

    NODE_SET_PROTOTYPE_METHOD (s_ct, "dump", BasicBlock::Dump);
    NODE_SET_PROTOTYPE_METHOD (s_ct, "toString", BasicBlock::ToString);

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("BasicBlock"),
		s_func);
  }

  BasicBlock::BasicBlock(llvm::BasicBlock *llvm_bb) : llvm_bb(llvm_bb)
  {
  }

  BasicBlock::BasicBlock() : llvm_bb(NULL)
  {
  }

  BasicBlock::~BasicBlock()
  {
  }

  Handle< ::v8::Value> BasicBlock::New(llvm::BasicBlock *llvm_bb)
  {
    HandleScope scope;
    Local<Object> new_instance = BasicBlock::s_func->NewInstance();
    BasicBlock* new_bb = new BasicBlock(llvm_bb);
    new_bb->Wrap(new_instance);
    return scope.Close(new_instance);
  }


  Handle< ::v8::Value> BasicBlock::New(const Arguments& args)
  {
    HandleScope scope;

    if (args.Length()) {
      REQ_UTF8_ARG(0, blockname);
      REQ_LLVM_FUN_ARG(1, fun);

      BasicBlock* bb = new BasicBlock(llvm::BasicBlock::Create(llvm::getGlobalContext(), *blockname, fun));
      bb->Wrap(args.This());
    }
    return scope.Close(args.This());
  }

  Handle<v8::Value> BasicBlock::Dump(const Arguments& args)
  {
    HandleScope scope;
    BasicBlock* bb = ObjectWrap::Unwrap<BasicBlock>(args.This());
    bb->llvm_bb->dump();
    return scope.Close(Undefined());
  }

  Handle< ::v8::Value> BasicBlock::ToString(const Arguments& args)
  {
    HandleScope scope;
    BasicBlock* bb = ObjectWrap::Unwrap<BasicBlock>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    bb->llvm_bb->print(str_ostream);

    return scope.Close(String::New(str.c_str(), str.size()));
  }

  Handle<v8::Value> BasicBlock::GetParent(Local<String> property, const AccessorInfo& info)
  {
    HandleScope scope;
    BasicBlock* bb = ObjectWrap::Unwrap<BasicBlock>(info.This());
    Handle<v8::Value> result = Function::New(bb->llvm_bb->getParent());
    return scope.Close(result);
  }

  Handle<v8::Value> BasicBlock::GetName(Local<String> property, const AccessorInfo& info)
  {
    HandleScope scope;
    BasicBlock* bb = ObjectWrap::Unwrap<BasicBlock>(info.This());
    Handle<v8::String> result = String::New(bb->llvm_bb->getName().data(), bb->llvm_bb->getName().size());
    return scope.Close(result);
  }

  Persistent<FunctionTemplate> BasicBlock::s_ct;
  Persistent<v8::Function> BasicBlock::s_func;
};
