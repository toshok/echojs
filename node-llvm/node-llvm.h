#ifndef NODE_LLVM_H
#define NODE_LLVM_H

#include <v8.h>
#include <node.h>

#include "llvm/DerivedTypes.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Support/IRBuilder.h"

#define REQ_FUN_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsFunction())                   \
    return ThrowException(Exception::TypeError(                         \
					       String::New("Argument " #I " must be a function"))); \
  Local<Function> VAR = Local<Function>::Cast(args[I]);

#define REQ_ARRAY_ARG(I, VAR)                                           \
  if (args.Length() <= (I) || !args[I]->IsArray())			\
    return ThrowException(Exception::TypeError(                         \
					       String::New("Argument " #I " must be a array"))); \
  Local<Array> VAR = Local<Array>::Cast(args[I]);

#define REQ_INT_ARG(I, VAR)                                           \
  if (args.Length() <= (I) || !args[I]->IsInt32())			\
    return ThrowException(Exception::TypeError(                         \
					       String::New("Argument " #I " must be a array"))); \
  int32_t VAR = args[I]->Int32Value();

#define REQ_DOUBLE_ARG(I, VAR)                                           \
  if (args.Length() <= (I) || !args[I]->IsNumber())			\
    return ThrowException(Exception::TypeError(                         \
					       String::New("Argument " #I " must be a number"))); \
  double VAR = args[I]->NumberValue();

#define REQ_BOOL_ARG(I, VAR)                                           \
  if (args.Length() <= (I) || !args[I]->IsBoolean())			\
    return ThrowException(Exception::TypeError(                         \
					       String::New("Argument " #I " must be a bool"))); \
  bool VAR = args[I]->BooleanValue();

#define REQ_STR_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsString())			\
    return ThrowException(Exception::TypeError(                         \
					       String::New("Argument " #I " must be a string"))); \
  Local<String> VAR = Local<String>::Cast(args[I]);

#define REQ_UTF8_ARG(I, VAR)						\
  if (args.Length() <= (I) || !args[I]->IsString())			\
    return ThrowException(Exception::TypeError(                         \
					       String::New("Argument " #I " must be a string"))); \
  Local<String> VAR##_str = Local<String>::Cast(args[I]); \
  String::Utf8Value VAR(VAR##_str);

#define REQ_LLVM_VAL_ARG(I, VAR)					\
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::Value::HasInstance(args[I]) */) \
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm Value"))); \
  ::llvm::Value* VAR = jsllvm::Value::GetLLVMObj(args[I]);

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
  if (args.Length() <= (I) || !args[I]->IsObject() /* XXX || !jsllvm::Function::HasInstance(args[I]) */) \
    return ThrowException(Exception::TypeError(				\
					       String::New("Argument " #I " must be an llvm Function"))); \
  ::llvm::Function* VAR = jsllvm::Function::GetLLVMObj(args[I]);

#endif /* NODE_LLVM_H */
