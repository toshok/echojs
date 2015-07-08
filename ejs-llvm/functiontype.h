#ifndef EJS_LLVM_FUNCTIONTYPE_H
#define EJS_LLVM_FUNCTIONTYPE_H

#include "ejs-llvm.h"

namespace ejsllvm {
extern void FunctionType_init(ejsval exports);

extern ejsval FunctionType_new(llvm::FunctionType *llvm_ty);

extern llvm::FunctionType *FunctionType_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_FUNCTIONTYPE_H */
