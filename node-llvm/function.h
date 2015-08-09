#ifndef NODE_LLVM_FUNCTION_H
#define NODE_LLVM_FUNCTION_H

#include "node-llvm.h"
#include "type.h"
namespace jsllvm {


  class Function : public LLVMObjectWrap< ::llvm::Function, Function> {
  public:
    static void Init(v8::Handle<v8::Object> target);

  private:
    typedef LLVMObjectWrap< ::llvm::Function, Function> BaseType;
    friend class LLVMObjectWrap< ::llvm::Function, Function>;

    Function(::llvm::Function *llvm_fun) : BaseType(llvm_fun) { }
    Function() : BaseType(nullptr) { }
    virtual ~Function() { }

    static NAN_METHOD(New);
    static NAN_METHOD(Dump);
    static NAN_METHOD(SetDoesNotAccessMemory);
    static NAN_METHOD(SetOnlyReadsMemory);
    static NAN_METHOD(SetDoesNotThrow);
    static NAN_METHOD(SetGC);
    static NAN_METHOD(SetExternalLinkage);
    static NAN_METHOD(SetInternalLinkage);
    static NAN_METHOD(ToString);

    static NAN_METHOD(SetStructRet);
    static NAN_METHOD(HasStructRetAttr);

    static NAN_GETTER(GetName);
    static NAN_GETTER(GetReturnType);
    static NAN_GETTER(GetArgs);
    static NAN_GETTER(GetArgSize);
    static NAN_GETTER(GetType);
    static NAN_GETTER(GetDoesNotThrow);
    static NAN_GETTER(GetDoesNotAccessMemory);
    static NAN_GETTER(GetOnlyReadsMemory);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

};

#endif /* NODE_LLVM_FUNCTION_H */
