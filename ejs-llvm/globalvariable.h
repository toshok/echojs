#ifndef EJS_LLVM_GLOBALVARIABLE_H
#define EJS_LLVM_GLOBALVARIABLE_H

#include "ejs-llvm.h"

namespace ejsllvm {

void GlobalVariable_init(ejsval exports);

ejsval GlobalVariable_new(llvm::GlobalVariable *llvm_val);

llvm::GlobalVariable *GlobalVariable_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_GLOBALVARIABLE_H */
