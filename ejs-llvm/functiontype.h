#ifndef EJS_LLVM_FUNCTIONTYPE_H
#define EJS_LLVM_FUNCTIONTYPE_H

#include "ejs-llvm.h"

void _ejs_llvm_FunctionType_init (EJSValue* exports);

llvm::FunctionType* _ejs_llvm_FunctionType_GetLLVMObj(EJSValue* val);

#endif /* EJS_LLVM_FUNCTIONTYPE_H */
