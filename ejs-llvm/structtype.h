#ifndef EJS_LLVM_STRUCTTYPE_H
#define EJS_LLVM_STRUCTTYPE_H

#include "ejs-llvm.h"

namespace ejsllvm {

  extern void StructType_init (ejsval global);

  extern ejsval StructType_new(llvm::StructType* llvm_ty);
  extern llvm::StructType* StructType_GetLLVMObj(ejsval val);

};

#endif /* EJS_LLVM_STRUCTTYPE_H */
