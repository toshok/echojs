/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include "ejs-llvm.h"
#include "basicblock.h"
#include "irbuilder.h"
#include "type.h"
#include "constant.h"
#include "constantarray.h"
#include "constantfp.h"
#include "callinvoke.h"
#include "functiontype.h"
#include "globalvariable.h"
#include "structtype.h"
#include "arraytype.h"
#include "module.h"
#include "function.h"
#include "value.h"
#include "switch.h"
#include "allocainst.h"
#include "loadinst.h"
#include "landingpad.h"
#include "dibuilder.h"

namespace ejsllvm {
  llvm::LLVMContext TheContext;
}

std::string& trim(std::string& str)
{
    str.erase(0, str.find_first_not_of(" \n"));       //prefixing spaces
    str.erase(str.find_last_not_of(" \n")+1);         //surfixing spaces
    return str;
}


using namespace ejsllvm;

/* useful strings literals */
#include "ejs-llvm-atoms-gen.c"

extern "C" {

void
_ejs_llvm_init (ejsval global)
{
    // initialize our atoms before anything else
    _ejs_init_static_strings();

    Type_init (global);
    FunctionType_init (global);
    StructType_init (global);
    ArrayType_init (global);

    Function_init (global);
    GlobalVariable_init (global);
    Module_init (global);
    Value_init (global);
    BasicBlock_init (global);
    IRBuilder_init (global);
    Call_init (global);
    Invoke_init (global);
    Constant_init (global);
    ConstantArray_init (global);
    ConstantFP_init (global);
    Switch_init (global);
    LandingPad_init (global);
    AllocaInst_init (global);
    LoadInst_init (global);
#if notyet
    PHINode_init (global);
#endif

    DIBuilder_init (global);
    DIType_init(global);
    DIScope_init(global);
    DISubprogram_init(global);
    DICompileUnit_init(global);
    DIFile_init(global);
    DILexicalBlock_init(global);
    DebugLoc_init(global);
}


};
