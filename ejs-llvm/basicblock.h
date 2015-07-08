#ifndef EJS_LLVM_BASICBLOCK_H
#define EJS_LLVM_BASICBLOCK_H

#include "ejs-llvm.h"

namespace ejsllvm {
void BasicBlock_init(ejsval exports);

ejsval BasicBlock_new(llvm::BasicBlock *llvm_fun);

llvm::BasicBlock *BasicBlock_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_BASICBLOCK_H */
