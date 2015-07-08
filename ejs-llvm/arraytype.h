#ifndef EJS_LLVM_ARRAYTYPE_H
#define EJS_LLVM_ARRAYTYPE_H

#include "ejs-llvm.h"

namespace ejsllvm {
extern void ArrayType_init(ejsval exports);

ejsval ArrayType_new(llvm::ArrayType *lllvm_ty);

extern llvm::ArrayType *ArrayType_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_ARRAYTYPE_H */
