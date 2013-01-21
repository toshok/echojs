#ifndef EJS_LLVM_FUNCTION_H
#define EJS_LLVM_FUNCTION_H

#include "ejs-llvm.h"

void _ejs_llvm_Function_init (ejsval exports);

ejsval _ejs_llvm_Function_new(llvm::Function* llvm_fun);

llvm::Function* _ejs_llvm_Function_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_FUNCTION_H */
