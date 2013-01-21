#ifndef EJS_LLVM_CALLINVOKE_H
#define EJS_LLVM_CALLINVOKE_H

#include "ejs-llvm.h"

extern void _ejs_llvm_Call_init (ejsval exports);

ejsval _ejs_llvm_Call_new(llvm::CallInst* llvm_call);

extern llvm::CallInst* _ejs_llvm_Call_GetLLVMObj(ejsval val);


extern void _ejs_llvm_Invoke_init (ejsval exports);

ejsval _ejs_llvm_Invoke_new(llvm::InvokeInst* llvm_invoke);

extern llvm::InvokeInst* _ejs_llvm_Invoke_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_CALLINVOKE_H */
