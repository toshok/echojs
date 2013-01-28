#ifndef EJS_LLVM_CALLINVOKE_H
#define EJS_LLVM_CALLINVOKE_H

#include "ejs-llvm.h"

namespace ejsllvm {
  extern void Call_init (ejsval exports);

  ejsval Call_new(llvm::CallInst* llvm_call);

  extern llvm::CallInst* Call_GetLLVMObj(ejsval val);


  extern void Invoke_init (ejsval exports);

  ejsval Invoke_new(llvm::InvokeInst* llvm_invoke);

  extern llvm::InvokeInst* Invoke_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_CALLINVOKE_H */
