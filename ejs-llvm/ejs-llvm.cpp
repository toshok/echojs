#include "ejs-llvm.h"
#include "type.h"
#include "functiontype.h"
#include "structtype.h"

std::string& trim(std::string& str)
{
  str.erase(0, str.find_first_not_of(" \n"));       //prefixing spaces
  str.erase(str.find_last_not_of(" \n")+1);         //surfixing spaces
  return str;
}


extern "C" {
void
_ejs_llvm_init (ejsval global)
{
  _ejs_llvm_Type_init (global);
  _ejs_llvm_FunctionType_init (global);
  _ejs_llvm_StructType_init (global);
#if notyet
  _ejs_llvm_Value_init (global);
  _ejs_llvm_Function_init (global);
  _ejs_llvm_BasicBlock_init (global);
  _ejs_llvm_PHINode_init (global);
  _ejs_llvm_IRBuilder_init (global);
  _ejs_llvm_Module_init (global);
  _ejs_llvm_Constant_init (global);
  _ejs_llvm_ConstantFP_init (global);
#endif
}


};
