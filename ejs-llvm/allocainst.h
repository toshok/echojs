#ifndef EJS_LLVM_ALLOCAINST_H
#define EJS_LLVM_ALLOCAINST_H

#include "ejs-llvm.h"

namespace ejsllvm {
  extern void AllocaInst_init (ejsval exports);

  ejsval AllocaInst_new(llvm::AllocaInst* llvm_alloca);

  extern llvm::AllocaInst* AllocaInst_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_ALLOCAINST_H */
