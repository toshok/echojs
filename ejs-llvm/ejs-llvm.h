#ifndef EJS_LLVM_H
#define EJS_LLVM_H

#include "ejs.h"

#include <sstream>
#include <string>

#include "llvm/DerivedTypes.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Constants.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Intrinsics.h"
#include "llvm/IRBuilder.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"

#define REQ_FUN_ARG(I, VAR)                                             \
  if (argc <= (I) || !EJSVAL_IS_FUNCTION(args[I]))			\
    printf ("in function %s\n", __PRETTY_FUNCTION__);			\
    abort();								\
  }									\
  EJSFunction* VAR = (EJSFunction*)EJSVAL_TO_OBJECT(args[I]);

#define REQ_ARRAY_ARG(I, VAR)                                           \
  if (argc <= (I) || !EJSVAL_IS_ARRAY(args[I]))				\
    abort();								\
  EJSArray* VAR = (EJSArray*)EJSVAL_TO_OBJECT(args[I]);

#define REQ_INT_ARG(I, VAR)						\
  if (argc <= (I) /*|| !args[I]->IsInt32()*/)				\
    abort();								\
  int32_t VAR = (int32_t)EJSVAL_TO_NUMBER(args[I]);

#define REQ_DOUBLE_ARG(I, VAR)						\
  if (argc <= (I) /*|| !args[I]->IsNumber()*/)				\
    abort();								\
  double VAR = EJSVAL_TO_NUMBER(args[I]);

#define REQ_BOOL_ARG(I, VAR)						\
  if (argc <= (I) /*|| !args[I]->IsBoolean()*/)				\
    abort();								\
  bool VAR = EJSVAL_TO_BOOLEAN(args[I]);

#define REQ_UTF8_ARG(I, VAR)						\
  if (argc <= (I) /*|| !args[I]->IsString()*/)				\
    abort();								\
  char* VAR = EJSVAL_TO_FLAT_STRING(args[I]);

#define REQ_LLVM_VAL_ARG(I, VAR)					\
  if (argc <= (I) /*|| !args[I]->IsObject() || !jsllvm::Value::HasInstance(args[I]) */) {	\
    printf ("in function %s\n", __PRETTY_FUNCTION__);			\
    abort();								\
  }									\
  ::llvm::Value* VAR = jsllvm::Value::GetLLVMObj(args[I]);

#define REQ_LLVM_TYPE_ARG(I, VAR)					\
  if (argc <= (I) /*|| !args[I]->IsObject() || !jsllvm::Type::HasInstance(args[I]) */) \
    abort();								\
  ::llvm::Type* VAR = _ejs_llvm_Type_getLLVMObj(args[I]);

#define REQ_LLVM_BB_ARG(I, VAR)						\
  if (argc <= (I) /* || (!args[I]->IsNull() && !args[I]->IsObject() && !jsllvm::BasicBlock::HasInstance(args[I]) */)) \
    abort();								\
  ::llvm::BasicBlock* VAR = jsllvm::BasicBlock::GetLLVMObj(EJSVAL_TO_OBJECT(args[I]));

#define REQ_LLVM_FUN_ARG(I, VAR)					\
  if (argc <= (I) /* || !args[I]->IsObject() || !jsllvm::Function::HasInstance(args[I]) */) \
    abort();								\
  ::llvm::Function* VAR = jsllvm::Function::GetLLVMObj(EJSVAL_TO_OBJECT(args[I]));

extern std::string& trim(std::string& str);

#endif /* EJS_LLVM_H */
