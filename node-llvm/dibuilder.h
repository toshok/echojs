#ifndef NODE_LLVM_DIBUILDER_H
#define NODE_LLVM_DIBUILDER_H

#include "node-llvm.h"
namespace jsllvm {


  class DIBuilder : public LLVMObjectWrap< ::llvm::DIBuilder, DIBuilder> {
  public:
    static NAN_MODULE_INIT(Init);

    static v8::Local<v8::Value> Create(llvm::DIBuilder *builder);

  private:
    typedef LLVMObjectWrap< ::llvm::DIBuilder, DIBuilder> BaseType;
    friend class LLVMObjectWrap< ::llvm::DIBuilder, DIBuilder>;

    DIBuilder(llvm::DIBuilder *llvm_dibuilder) : BaseType(llvm_dibuilder) { }
    DIBuilder() : BaseType(nullptr) { }
    virtual ~DIBuilder() { }

    ::llvm::DIType* ejsValueType;
    ::llvm::DIType* ejsValuePointerType;

    static NAN_METHOD(New);
    static NAN_METHOD(CreateCompileUnit);
    static NAN_METHOD(CreateFile);
    static NAN_METHOD(CreateFunction);
    static NAN_METHOD(CreateLexicalBlock);
    static NAN_METHOD(Finalize);

    llvm::DISubroutineType* CreateDIFunctionType(llvm::FunctionType *fty);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };


  class DIType : public Nan::ObjectWrap {
  public:
    static NAN_MODULE_INIT(Init);

    static v8::Local<v8::Value> Create(llvm::DIType* type);
    static NAN_METHOD(New);

    static llvm::DIType* GetLLVMObj (v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DIType>(value->ToObject(context).ToLocalChecked())->llvm_ditype;
    }

  private:
    ::llvm::DIType* llvm_ditype;

    DIType(llvm::DIType* llvm_ditype);
    DIType();
    virtual ~DIType();

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

  class DIScope : public Nan::ObjectWrap {
  public:
    static NAN_MODULE_INIT(Init);

    static v8::Local<v8::Value> Create(llvm::DIScope* scope);
    static NAN_METHOD(New);

    static llvm::DIScope* GetLLVMObj (v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DIScope>(value->ToObject(context).ToLocalChecked())->llvm_discope;
    }

    static Nan::Persistent<v8::FunctionTemplate> constructor;
  private:
    ::llvm::DIScope* llvm_discope;

    DIScope(llvm::DIScope* llvm_discope);
    DIScope();
    virtual ~DIScope();

    static Nan::Persistent<v8::Function> constructor_func;
  };

  class DISubprogram : public Nan::ObjectWrap {
  public:
    static NAN_MODULE_INIT(Init);

    static v8::Local<v8::Value> Create(llvm::DISubprogram* subprogram);

    static NAN_METHOD(New);

    static llvm::DISubprogram* GetLLVMObj (v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DISubprogram>(value->ToObject(context).ToLocalChecked())->llvm_disubprogram;
    }

  private:
    ::llvm::DISubprogram* llvm_disubprogram;

    DISubprogram(llvm::DISubprogram* llvm_disubprogram);
    DISubprogram();
    virtual ~DISubprogram();

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

  class DICompileUnit : public Nan::ObjectWrap {
  public:
    static NAN_MODULE_INIT(Init);

    static v8::Local<v8::Value> Create(llvm::DICompileUnit* file);

    static NAN_METHOD(New);
    static NAN_METHOD(Verify);

    static llvm::DICompileUnit* GetLLVMObj (v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DICompileUnit>(value->ToObject(context).ToLocalChecked())->llvm_dicompileunit;
    }

  private:
    ::llvm::DICompileUnit* llvm_dicompileunit;

    DICompileUnit(llvm::DICompileUnit* llvm_dicompileunit);
    DICompileUnit();
    virtual ~DICompileUnit();

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

  class DIFile : public Nan::ObjectWrap {
  public:
    static NAN_MODULE_INIT(Init);

    static v8::Local<v8::Value> Create(llvm::DIFile* file);

    static NAN_METHOD(New);

    static llvm::DIFile* GetLLVMObj (v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DIFile>(value->ToObject(context).ToLocalChecked())->llvm_difile;
    }

  private:
    ::llvm::DIFile* llvm_difile;

    DIFile(llvm::DIFile* llvm_difile);
    DIFile();
    virtual ~DIFile();

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

  class DILexicalBlock : public Nan::ObjectWrap {
  public:
    static NAN_MODULE_INIT(Init);

    static v8::Local<v8::Value> Create(llvm::DILexicalBlock* lexical_block);
    static NAN_METHOD(New);

    static llvm::DILexicalBlock* GetLLVMObj (v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DILexicalBlock>(value->ToObject(context).ToLocalChecked())->llvm_dilexicalblock;
    }

  private:
    ::llvm::DILexicalBlock* llvm_dilexicalblock;

    DILexicalBlock(llvm::DILexicalBlock* llvm_dilexicalblock);
    DILexicalBlock();
    virtual ~DILexicalBlock();

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };
  class DebugLoc : public Nan::ObjectWrap {
  public:
    static NAN_MODULE_INIT(Init);

    static v8::Local<v8::Value> Create(llvm::DebugLoc debugLoc);
    static NAN_METHOD(New);

    static llvm::DebugLoc GetLLVMObj (v8::Local<v8::Context> context, v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DebugLoc>(value->ToObject(context).ToLocalChecked())->llvm_debugloc;
    }

  private:
    ::llvm::DebugLoc llvm_debugloc;

    DebugLoc(llvm::DebugLoc llvm_debugloc);
    DebugLoc();
    virtual ~DebugLoc();

    static NAN_METHOD(Get);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

}

#endif /* NODE_LLVM_DIBUILDER_H */

