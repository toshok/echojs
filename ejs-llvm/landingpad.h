#ifndef EJS_LLVM_LANDINGPAD_H
#define EJS_LLVM_LANDINGPAD_H

#include "ejs-llvm.h"

extern void _ejs_llvm_LandingPad_init (ejsval exports);

ejsval _ejs_llvm_LandingPad_new(llvm::LandingPadInst* llvm_switch);

extern llvm::LandingPadInst* _ejs_llvm_LandingPad_GetLLVMObj(ejsval val);

#endif /* EJS_LLVM_LANDINGPAD_H */
