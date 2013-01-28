#ifndef EJS_LLVM_MODULE_H
#define EJS_LLVM_MODULE_H

#include "ejs-llvm.h"

namespace ejsllvm {

  void Module_init (ejsval exports);

  llvm::Module* Module_GetLLVMObj(ejsval val);

};

#endif /* EJS_LLVM_MODULE_H */
