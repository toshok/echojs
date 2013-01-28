#ifndef EJS_LLVM_TYPE_H
#define EJS_LLVM_TYPE_H

#include "ejs-llvm.h"

namespace ejsllvm {
  extern void Type_init (ejsval global);

  extern ejsval Type_get_prototype();

  extern ejsval Type_new(llvm::Type* llvm_ty);
  extern llvm::Type* Type_GetLLVMObj(ejsval val);
};

#endif /* EJS_LLVM_TYPE_H */
