#ifndef EJS_LLVM_LOADINST_H
#define EJS_LLVM_LOADINST_H

#include "ejs-llvm.h"

namespace ejsllvm {
  extern void LoadInst_init (ejsval exports);

  ejsval LoadInst_new(llvm::LoadInst* llvm_load);

  extern llvm::LoadInst* LoadInst_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_LOADINST_H */
