#include "node-llvm.h"
#include "type.h"
#include "functiontype.h"
#include "structtype.h"
#include "value.h"
#include "function.h"
#include "basicblock.h"
#include "phinode.h"
#include "irbuilder.h"
#include "module.h"
#include "constant.h"
#include "constantfp.h"

std::string& trim(std::string& str)
{
  str.erase(0, str.find_first_not_of(" \n"));       //prefixing spaces
  str.erase(str.find_last_not_of(" \n")+1);         //surfixing spaces
  return str;
}


extern "C" {
  static void init (v8::Handle<v8::Object> target)
  {
    jsllvm::Type::Init(target);
    jsllvm::FunctionType::Init(target);
    jsllvm::StructType::Init(target);
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
