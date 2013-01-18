#ifndef EJS_LLVM_FUNCTIONTYPE_H
#define EJS_LLVM_FUNCTIONTYPE_H

#include "ejs-llvm.h"

extern void _ejs_llvm_FunctionType_init (ejsval exports);

extern ejsval _ejs_llvm_FunctionType_new(llvm::FunctionType* llvm_ty);

extern llvm::FunctionType* _ejs_llvm_FunctionType_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_FUNCTIONTYPE_H */
