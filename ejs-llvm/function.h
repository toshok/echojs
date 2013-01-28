#ifndef EJS_LLVM_FUNCTION_H
#define EJS_LLVM_FUNCTION_H

#include "ejs-llvm.h"

namespace ejsllvm {
  void Function_init (ejsval exports);

  ejsval Function_new(llvm::Function* llvm_fun);

  llvm::Function* Function_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_FUNCTION_H */
