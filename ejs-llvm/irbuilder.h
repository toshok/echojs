#ifndef EJS_LLVM_IRBUILDER_H
#define EJS_LLVM_IRBUILDER_H

#include "ejs-llvm.h"

void _ejs_llvm_IRBuilder_init (ejsval exports);

ejsval _ejs_llvm_IRBuilder_new(llvm::IRBuilder<>* llvm_fun);

llvm::IRBuilder<>* _ejs_llvm_IRBuilder_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_IRBUILDER_H */
