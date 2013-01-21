#ifndef EJS_LLVM_TYPE_H
#define EJS_LLVM_TYPE_H

#include "ejs-llvm.h"

extern void _ejs_llvm_Type_init (ejsval global);

extern ejsval _ejs_llvm_Type_get_prototype();

extern ejsval _ejs_llvm_Type_new(llvm::Type* llvm_ty);
extern llvm::Type* _ejs_llvm_Type_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_TYPE_H */
