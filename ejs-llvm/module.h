#ifndef EJS_LLVM_MODULE_H
#define EJS_LLVM_MODULE_H

#include "ejs-llvm.h"

void _ejs_llvm_Module_init (ejsval exports);

llvm::Module* _ejs_llvm_Module_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_MODULE_H */
