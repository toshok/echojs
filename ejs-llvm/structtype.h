#ifndef EJS_LLVM_STRUCTTYPE_H
#define EJS_LLVM_STRUCTTYPE_H

#include "ejs-llvm.h"

extern void _ejs_llvm_StructType_init (ejsval global);

extern ejsval _ejs_llvm_StructType_new(llvm::StructType* llvm_ty);
extern llvm::StructType* _ejs_llvm_StructType_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_STRUCTTYPE_H */
