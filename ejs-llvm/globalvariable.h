#ifndef EJS_LLVM_VALUE_H
#define EJS_LLVM_VALUE_H

#include "ejs-llvm.h"

void _ejs_llvm_GlobalVariable_init (ejsval exports);

ejsval _ejs_llvm_GlobalVariable_new(llvm::GlobalVariable* llvm_val);

llvm::GlobalVariable* _ejs_llvm_GlobalVariable_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_VALUE_H */
