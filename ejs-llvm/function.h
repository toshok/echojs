#ifndef EJS_LLVM_FUNCTION_H
#define EJS_LLVM_FUNCTION_H

#include "ejs-llvm.h"

void _ejs_llvm_Function_init (EJSValue* exports);

llvm::FunctionType* _ejs_llvm_Function_GetLLVMObj(EJSValue* val);

#endif /* EJS_LLVM_FUNCTION_H */
