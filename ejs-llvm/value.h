#ifndef EJS_LLVM_VALUE_H
#define EJS_LLVM_VALUE_H

#include "ejs-llvm.h"

namespace ejsllvm {
void Value_init(ejsval exports);

ejsval Value_new(llvm::Value *llvm_val);

llvm::Value *Value_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_VALUE_H */
