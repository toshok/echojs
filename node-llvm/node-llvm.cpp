#include "node-llvm.h"
#include "type.h"
#include "value.h"
#include "function.h"
#include "basicblock.h"
#include "phinode.h"
#include "irbuilder.h"
#include "module.h"
#include "constant.h"
#include "constantfp.h"

extern "C" {
  static void init (v8::Handle<v8::Object> target)
  {
    jsllvm::Type::Init(target);
    jsllvm::Value::Init(target);
    jsllvm::Function::Init(target);
    jsllvm::BasicBlock::Init(target);
    jsllvm::PHINode::Init(target);
    jsllvm::IRBuilder::Init(target);
    jsllvm::Module::Init(target);
    jsllvm::Constant::Init(target);
    jsllvm::ConstantFP::Init(target);
  }

  NODE_MODULE(llvm, init);
}
