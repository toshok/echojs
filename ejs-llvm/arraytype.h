#ifndef EJS_LLVM_ARRAYTYPE_H
#define EJS_LLVM_ARRAYTYPE_H

#include "ejs-llvm.h"

extern void _ejs_llvm_ArrayType_init (ejsval exports);

ejsval _ejs_llvm_ArrayType_new(llvm::ArrayType* lllvm_ty);

extern llvm::ArrayType* _ejs_llvm_ArrayType_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_ARRAYTYPE_H */
