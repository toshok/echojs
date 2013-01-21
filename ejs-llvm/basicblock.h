#ifndef EJS_LLVM_BASICBLOCK_H
#define EJS_LLVM_BASICBLOCK_H

#include "ejs-llvm.h"

void _ejs_llvm_BasicBlock_init (ejsval exports);

ejsval _ejs_llvm_BasicBlock_new(llvm::BasicBlock* llvm_fun);

llvm::BasicBlock* _ejs_llvm_BasicBlock_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_BASICBLOCK_H */
