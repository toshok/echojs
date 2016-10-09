#ifndef EJS_LLVM_CONSTANT_H
#define EJS_LLVM_CONSTANT_H

#include "ejs-llvm.h"

namespace ejsllvm {

  extern void Constant_init (ejsval exports);

  ejsval Constant_new(llvm::Constant* llvm_const);

  llvm::Constant* Constant_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_CONSTANT_H */
