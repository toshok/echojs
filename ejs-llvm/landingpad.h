#ifndef EJS_LLVM_LANDINGPAD_H
#define EJS_LLVM_LANDINGPAD_H

#include "ejs-llvm.h"

namespace ejsllvm {
  extern void LandingPad_init (ejsval exports);

  ejsval LandingPad_new(llvm::LandingPadInst* llvm_switch);

  extern llvm::LandingPadInst* LandingPad_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_LANDINGPAD_H */
