#include "ejs-llvm.h"
#include "ejs-object.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "function.h"
#include "type.h"

typedef struct {
  /* object header */
  EJSObject obj;

  /* type specific data */
  llvm::Function *fun;
} EJSLLVMFunction;
