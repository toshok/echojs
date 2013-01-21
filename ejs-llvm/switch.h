#ifndef EJS_LLVM_SWITCH_H
#define EJS_LLVM_SWITCH_H

#include "ejs-llvm.h"

extern void _ejs_llvm_Switch_init (ejsval exports);

ejsval _ejs_llvm_Switch_new(llvm::SwitchInst* llvm_switch);

extern llvm::SwitchInst* _ejs_llvm_Switch_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_SWITCH_H */
