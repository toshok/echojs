#ifndef EJS_LLVM_VALUE_H
#define EJS_LLVM_VALUE_H

#include "ejs-llvm.h"

void _ejs_llvm_Value_init (ejsval exports);

ejsval _ejs_llvm_Value_new(llvm::Value* llvm_val);

llvm::Value* _ejs_llvm_Value_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_VALUE_H */
