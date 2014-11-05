#ifndef EJS_LLVM_H
#define EJS_LLVM_H

#include "ejs.h"
#include "ejs-string.h"

#include <sstream>
#include <string>

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Bitcode/ReaderWriter.h"

#define ADD_STACK_ROOT(t,v,i) t v = i

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
  int64_t VAR = (int64_t)EJSVAL_TO_NUMBER(args[I]);

#define REQ_DOUBLE_ARG(I, VAR)						\
  if (argc <= (I) /*|| !args[I]->IsNumber()*/)				\
    abort();								\
  double VAR = EJSVAL_TO_NUMBER(args[I]);

#define REQ_BOOL_ARG(I, VAR)						\
  if (argc <= (I) || !EJSVAL_IS_BOOLEAN(args[I]))			\
    abort();								\
  bool VAR = EJSVAL_TO_BOOLEAN(args[I]);

#define REQ_UTF8_ARG(I, VAR)						\
  if (argc <= (I) /*|| !args[I]->IsString()*/)				\
    abort();								\
  char* VAR##_utf8 = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[I]));	\
  std::string VAR(VAR##_utf8);						\
  free(VAR##_utf8);							\

#define FALLBACK_UTF8_ARG(I, VAR, FALLBACK)				\
  std::string VAR;							\
  if (argc <= (I)) { 							\
    VAR = FALLBACK;							\
  }									\
  else {								\
    /*if (!args[I]->IsString())	*/					\
    /*  abort();		*/					\
    char* tmp = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(args[I]));		\
    VAR = tmp;								\
    free(tmp);								\
  }									\

#define FALLBACK_EMPTY_UTF8_ARG(I, VAR) FALLBACK_UTF8_ARG(I, VAR, "")

#define REQ_LLVM_VAL_ARG(I, VAR)					\
  if (argc <= (I) || !EJSVAL_IS_OBJECT(args[I]) /*|| !jsllvm::Value::HasInstance(args[I]) */) { \
    printf ("in function %s\n", __PRETTY_FUNCTION__);			\
    abort();								\
  }									\
  ::llvm::Value* VAR = Value_GetLLVMObj(args[I]);

#define REQ_LLVM_CONST_ARG(I, VAR)					\
  if (argc <= (I) /*|| !args[I]->IsObject() || !jsllvm::Constant::HasInstance(args[I]) */) \
    abort();								\
  ::llvm::Constant* VAR = static_cast< ::llvm::Constant*>(Value_GetLLVMObj(args[I]));

#define REQ_NULLABLE_LLVM_CONST_ARG(I, VAR)					\
  ::llvm::Constant* VAR;						\
  if (argc > (I) && EJSVAL_IS_NULL(args[(I)]))				\
    VAR = NULL;								\
  else if (argc <= (I) || !EJSVAL_IS_OBJECT(args[I]) /* XXX || !jsllvm::Constant::HasInstance(args[I]) */) { \
    printf ("in function %s\n", __PRETTY_FUNCTION__);			\
    abort();								\
  }									\
  else									\
    VAR = static_cast< ::llvm::Constant*>(Value_GetLLVMObj(args[I]));

#define REQ_LLVM_MODULE_ARG(I, VAR)					\
  if (argc <= (I) /*|| !args[I]->IsObject() || !jsllvm::Constant::HasInstance(args[I]) */) \
    abort();								\
  ::llvm::Module* VAR = Module_GetLLVMObj(args[I]);

#define REQ_LLVM_TYPE_ARG(I, VAR)					\
  if (argc <= (I) /*|| !args[I]->IsObject() || !jsllvm::Type::HasInstance(args[I]) */) \
    abort();								\
  ::llvm::Type* VAR = Type_GetLLVMObj(args[I]);

#define REQ_LLVM_BB_ARG(I, VAR)						\
  if (argc <= (I) /* || (!args[I]->IsNull() && !args[I]->IsObject() && !jsllvm::BasicBlock::HasInstance(args[I]) */) \
    abort();								\
  ::llvm::BasicBlock* VAR = BasicBlock_GetLLVMObj(args[I]);

#define REQ_LLVM_FUN_ARG(I, VAR)					\
  if (argc <= (I) /* || !args[I]->IsObject() || !jsllvm::Function::HasInstance(args[I]) */) \
    abort();								\
  ::llvm::Function* VAR = Function_GetLLVMObj(args[I]);

extern std::string& trim(std::string& str);

#include "ejs-llvm-atoms.h"

#endif /* EJS_LLVM_H */
