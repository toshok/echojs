#ifndef EJS_LLVM_TYPE_H
#define EJS_LLVM_TYPE_H

#include "ejs-llvm.h"

void _ejs_llvm_Type_init (EJSValue* global);

EJSValue* _ejs_llvm_Type_get_prototype();

EJSValue* _ejs_llvm_Type_new(llvm::Type* llvm_ty);
llvm::Type* _ejs_llvm_Type_getLLVMObj(EJSValue* val);

#endif /* EJS_LLVM_TYPE_H */
