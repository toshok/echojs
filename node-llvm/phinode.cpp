#include "node-llvm.h"
#include "type.h"
#include "phinode.h"
#include "value.h"
#include "basicblock.h"

using namespace node;
using namespace v8;

namespace jsllvm {

  void PHINode::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("PHINode"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "dump", PHINode::Dump);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "toString", PHINode::ToString);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "addIncoming", PHINode::AddIncoming);

    s_func = Persistent< ::v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("PHINode"),
		s_func);
  }

  PHINode::PHINode(llvm::PHINode *llvm_phi) : llvm_phi(llvm_phi)
  {
  }

  PHINode::PHINode() : llvm_phi(NULL)
  {
  }

  PHINode::~PHINode()
  {
  }

  Handle<v8::Value> PHINode::New(const Arguments& args)
  {
    HandleScope scope;
    PHINode* phi = new PHINode();
    phi->Wrap(args.This());
    return args.This();
  }


  Handle<v8::Value> PHINode::New(llvm::PHINode *llvm_phi)
  {
    HandleScope scope;
    Local<Object> new_instance = PHINode::s_func->NewInstance();
    PHINode* new_phi = new PHINode(llvm_phi);
    new_phi->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> PHINode::Dump (const Arguments& args)
  {
    HandleScope scope;
    PHINode* phi = ObjectWrap::Unwrap<PHINode>(args.This());
    phi->llvm_phi->dump();
    return scope.Close(Undefined());
  }

  Handle<v8::Value> PHINode::ToString(const Arguments& args)
  {
    HandleScope scope;
    PHINode* phi = ObjectWrap::Unwrap<PHINode>(args.This());

    std::string str;
    llvm::raw_string_ostream str_ostream(str);
    phi->llvm_phi->print(str_ostream);

    return scope.Close(String::New(trim(str_ostream.str()).c_str()));
  }

  Handle<v8::Value> PHINode::AddIncoming (const Arguments& args)
  {
    HandleScope scope;
    PHINode* phi = ObjectWrap::Unwrap<PHINode>(args.This());
    REQ_LLVM_VAL_ARG(0, incoming_val);
    REQ_LLVM_BB_ARG(1, incoming_bb);
    phi->llvm_phi->addIncoming(incoming_val, incoming_bb);
    return scope.Close(Undefined());
  }

  Persistent<FunctionTemplate> PHINode::s_ct;
  Persistent<Function> PHINode::s_func;
};


