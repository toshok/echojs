#ifndef NODE_LLVM_H
#define NODE_LLVM_H

#include <v8.h>
#include <node.h>

#include <sstream>
#include <string>

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Bitcode/ReaderWriter.h"

#define REQ_FUN_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsFunction()) {			\
    char buf[256];							\
    snprintf(buf, 256, "Argument " #I " must be a function at %s:%d\n", __FILE__, __LINE__); \
    return ThrowException(Exception::TypeError(String::New(buf)));	\
  }									\
  Local<Function> VAR = Local<Function>::Cast(args[I]);

#define REQ_ARRAY_ARG(I, VAR)                                           \
  if (args.Length() <= (I) || !args[I]->IsArray()) {			\
    char buf[256];							\
    snprintf(buf, 256, "Argument " #I " must be an array at %s:%d\n", __FILE__, __LINE__); \
    return ThrowException(Exception::TypeError(String::New(buf)));	\
  }									\
  Local<Array> VAR = Local<Array>::Cast(args[I]);

#define REQ_INT_ARG(I, VAR)						\
  if (args.Length() <= (I) || !args[I]->IsNumber()) {			\
    char buf[256];							\
    snprintf(buf, 256, "Argument " #I " must be an integer at %s:%d\n", __FILE__, __LINE__); \
    return ThrowException(Exception::TypeError(String::New(buf)));	\
  }									\
  int64_t VAR = (int64_t)args[I]->NumberValue();


#define REQ_DOUBLE_ARG(I, VAR)                                           \
  if (args.Length() <= (I) || !args[I]->IsNumber()) {			\
    char buf[256];							\
    snprintf(buf, 256, "Argument " #I " must be a number at %s:%d\n", __FILE__, __LINE__); \
    return ThrowException(Exception::TypeError(String::New(buf)));	\
  }									\
  double VAR = args[I]->NumberValue();

#define REQ_BOOL_ARG(I, VAR)                                           \
  if (args.Length() <= (I) || !args[I]->IsBoolean()) {			\
    char buf[256];							\
    snprintf(buf, 256, "Argument " #I " must be a bool at %s:%d\n", __FILE__, __LINE__); \
    return ThrowException(Exception::TypeError(String::New(buf)));	\
  }									\
  bool VAR = args[I]->BooleanValue();

#define REQ_UTF8_ARG(I, VAR)						\
  if (args.Length() <= (I) || !args[I]->IsString()) {			\
    char buf[256];							\
    snprintf(buf, 256, "Argument " #I " must be a string at %s:%d\n", __FILE__, __LINE__); \
    return ThrowException(Exception::TypeError(String::New(buf)));	\
  }									\
  Local<String> VAR##_str = Local<String>::Cast(args[I]);		\
  String::Utf8Value VAR(VAR##_str);

#define FALLBACK_UTF8_ARG(I, VAR, FALLBACK)				\
  Local<String> VAR##_str;						\
  if (args.Length() <= (I)) {						\
    VAR##_str = String::New("");					\
  }									\
  else {								\
    if (!args[I]->IsString()) {						\
      char buf[256];							\
      snprintf(buf, 256, "Argument " #I " must be a string at %s:%d\n", __FILE__, __LINE__); \
      return ThrowException(Exception::TypeError(String::New(buf)));	\
    }									\
    VAR##_str = Local<String>::Cast(args[I]);				\
  }									\
  String::Utf8Value VAR(VAR##_str);

#define FALLBACK_EMPTY_UTF8_ARG(I, VAR) FALLBACK_UTF8_ARG(I, VAR, "")

#define REQ_LLVM_VAL_ARG(I, VAR)					\
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::Value::HasInstance(args[I]) */) {	\
    printf ("in function %s\n", __PRETTY_FUNCTION__);			\
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm Value"))); \
  }									\
  ::llvm::Value* VAR = jsllvm::Value::GetLLVMObj(args[I]);

#define REQ_LLVM_CONST_ARG(I, VAR)					\
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::Constant::HasInstance(args[I]) */) {	\
    printf ("in function %s\n", __PRETTY_FUNCTION__);			\
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm Constant"))); \
  }									\
  ::llvm::Constant* VAR = static_cast< ::llvm::Constant*>(jsllvm::Value::GetLLVMObj(args[I]));

#define REQ_NULLABLE_LLVM_CONST_ARG(I, VAR)					\
  ::llvm::Constant* VAR;						\
  if (args.Length() > (I) && args[I]->IsNull())				\
    VAR = NULL;								\
  else if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::Constant::HasInstance(args[I]) */) { \
    printf ("in function %s\n", __PRETTY_FUNCTION__);			\
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm Constant"))); \
  }									\
  else									\
    VAR = static_cast< ::llvm::Constant*>(jsllvm::Value::GetLLVMObj(args[I]));

#define REQ_LLVM_CONST_INT_ARG(I, VAR)					\
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::Constant::HasInstance(args[I]) */) {	\
    printf ("in function %s\n", __PRETTY_FUNCTION__);			\
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm ConstantInt"))); \
  }									\
  ::llvm::ConstantInt* VAR = static_cast< ::llvm::ConstantInt*>(jsllvm::Value::GetLLVMObj(args[I]));

#define REQ_LLVM_MODULE_ARG(I, VAR)					\
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::Constant::HasInstance(args[I]) */) {	\
    printf ("in function %s\n", __PRETTY_FUNCTION__);			\
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm Module"))); \
  }									\
  ::llvm::Module* VAR = jsllvm::Module::GetLLVMObj(args[I]);

#define REQ_LLVM_TYPE_ARG(I, VAR)					\
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::Type::HasInstance(args[I]) */) \
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm Type"))); \
  ::llvm::Type* VAR = jsllvm::Type::GetLLVMObj(args[I]);

#define REQ_LLVM_BB_ARG(I, VAR)						\
  if (args.Length() <= (I) || (!args[I]->IsNull() && !args[I]->IsObject() /* XXX && !jsllvm::BasicBlock::HasInstance(args[I]) */)) \
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm BasicBlock"))); \
  ::llvm::BasicBlock* VAR = jsllvm::BasicBlock::GetLLVMObj(args[I]);

#define REQ_LLVM_FUN_ARG(I, VAR) \
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::Function::HasInstance(args[I]) */) {  \
    printf ("in function %s\n", __PRETTY_FUNCTION__);			\
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm Function"))); \
  }									\
  ::llvm::Function* VAR = jsllvm::Function::GetLLVMObj(args[I]);

#define REQ_LLVM_DIFILE_ARG(I, VAR) \
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::DIFile::HasInstance(args[I]) */) \
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm DIFile"))); \
  ::llvm::DIFile VAR = jsllvm::DIFile::GetLLVMObj(args[I]);

#define REQ_LLVM_DISCOPE_ARG(I, VAR) \
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::DIScope::HasInstance(args[I]) */) \
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm DIScope"))); \
  ::llvm::DIScope VAR = jsllvm::DIScope::GetLLVMObj(args[I]);

#define REQ_LLVM_DEBUGLOC_ARG(I, VAR) \
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::DIScope::HasInstance(args[I]) */) \
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm DebugLoc"))); \
  ::llvm::DebugLoc VAR = jsllvm::DebugLoc::GetLLVMObj(args[I]);

#define REQ_LLVM_MDNODE_ARG(I, VAR)					\
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::MDNode::HasInstance(args[I]) */) {	\
    printf ("in function %s\n", __PRETTY_FUNCTION__);			\
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm MDNode"))); \
  }									\
  ::llvm::MDNode* VAR = jsllvm::MDNode::GetLLVMObj(args[I]);

extern std::string& trim(std::string& str);

#endif /* NODE_LLVM_H */
