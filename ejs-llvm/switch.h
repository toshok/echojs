#ifndef EJS_LLVM_SWITCH_H
#define EJS_LLVM_SWITCH_H

#include "ejs-llvm.h"

namespace ejsllvm {
  extern void Switch_init (ejsval exports);

  ejsval Switch_new(llvm::SwitchInst* llvm_switch);

  extern llvm::SwitchInst* Switch_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_SWITCH_H */
