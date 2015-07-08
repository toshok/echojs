#ifndef NODE_LLVM_FUNCTION_H
#define NODE_LLVM_FUNCTION_H

#include "node-llvm.h"
#include "type.h"
namespace jsllvm {

class Function : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(::llvm::Function *llvm_fun);

    static llvm::Function *GetLLVMObj(v8::Local<v8::Value> value) {
        if (value->IsNull())
            return NULL;
        return node::ObjectWrap::Unwrap<Function>(value->ToObject())->llvm_fun;
    }

    llvm::Function *LLVMObj() { return llvm_fun; }

  private:
    ::llvm::Function *llvm_fun;

    Function(::llvm::Function *llvm_fun);
    Function();
    virtual ~Function();

    static v8::Handle<v8::Value> New(const v8::Arguments &args);
    static v8::Handle<v8::Value> Dump(const v8::Arguments &args);
    static v8::Handle<v8::Value>
    SetDoesNotAccessMemory(const v8::Arguments &args);
    static v8::Handle<v8::Value> SetOnlyReadsMemory(const v8::Arguments &args);
    static v8::Handle<v8::Value> SetDoesNotThrow(const v8::Arguments &args);
    static v8::Handle<v8::Value> SetGC(const v8::Arguments &args);
    static v8::Handle<v8::Value> SetExternalLinkage(const v8::Arguments &args);
    static v8::Handle<v8::Value> SetInternalLinkage(const v8::Arguments &args);
    static v8::Handle<v8::Value> ToString(const v8::Arguments &args);

    static v8::Handle<v8::Value> SetStructRet(const v8::Arguments &args);
    static v8::Handle<v8::Value> HasStructRetAttr(const v8::Arguments &args);

    static v8::Handle<v8::Value> GetName(v8::Local<v8::String> property,
                                         const v8::AccessorInfo &info);
    static v8::Handle<v8::Value> GetReturnType(v8::Local<v8::String> property,
                                               const v8::AccessorInfo &info);
    static v8::Handle<v8::Value> GetArgs(v8::Local<v8::String> property,
                                         const v8::AccessorInfo &info);
    static v8::Handle<v8::Value> GetArgSize(v8::Local<v8::String> property,
                                            const v8::AccessorInfo &info);
    static v8::Handle<v8::Value> GetType(v8::Local<v8::String> property,
                                         const v8::AccessorInfo &info);
    static v8::Handle<v8::Value> GetDoesNotThrow(v8::Local<v8::String> property,
                                                 const v8::AccessorInfo &info);
    static v8::Handle<v8::Value>
    GetDoesNotAccessMemory(v8::Local<v8::String> property,
                           const v8::AccessorInfo &info);
    static v8::Handle<v8::Value>
    GetOnlyReadsMemory(v8::Local<v8::String> property,
                       const v8::AccessorInfo &info);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
};
};

#endif /* NODE_LLVM_FUNCTION_H */
