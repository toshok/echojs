#ifndef EJS_LLVM_IRBUILDER_H
#define EJS_LLVM_IRBUILDER_H

#include "ejs-llvm.h"

namespace ejsllvm {
  void IRBuilder_init (ejsval exports);

  ejsval IRBuilder_new(llvm::IRBuilder<>* llvm_fun);

  llvm::IRBuilder<>* IRBuilder_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_IRBUILDER_H */
