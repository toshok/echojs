#ifndef EJS_LLVM_PHINODE_H
#define EJS_LLVM_PHINODE_H

#include "ejs-llvm.h"

namespace ejsllvm {
  extern void PhiNode_init (ejsval exports);

  ejsval PhiNode_new(llvm::PHINode* llvm_phi);

  extern llvm::PHINode* PhiNode_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_PHINODE_H */
