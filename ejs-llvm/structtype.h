#ifndef EJS_LLVM_STRUCTTYPE_H
#define EJS_LLVM_STRUCTTYPE_H

#include "ejs-llvm.h"

void _ejs_llvm_StructType_init (EJSValue* global);

EJSValue* _ejs_llvm_StructType_new(llvm::StructType* llvm_ty);
llvm::StructType* _ejs_llvm_StructType_getLLVMObj(EJSValue* val);

#endif /* EJS_LLVM_STRUCTTYPE_H */
