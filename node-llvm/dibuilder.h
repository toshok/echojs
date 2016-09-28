#ifndef NODE_LLVM_DIBUILDER_H
#define NODE_LLVM_DIBUILDER_H

#if false

#include "node-llvm.h"
namespace jsllvm {


  class DIBuilder : public LLVMObjectWrap< ::llvm::DIBuilder, DIBuilder> {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Local<v8::Value> Create(llvm::DIBuilder *builder);

  private:
    typedef LLVMObjectWrap< ::llvm::DIBuilder, DIBuilder> BaseType;
    friend class LLVMObjectWrap< ::llvm::DIBuilder, DIBuilder>;

    DIBuilder(llvm::DIBuilder *llvm_dibuilder) : BaseType(llvm_dibuilder) { }
    DIBuilder() : BaseType(nullptr) { }
    virtual ~DIBuilder() { }

    ::llvm::DIType ejsValueType;
    ::llvm::DIType ejsValuePointerType;

    static NAN_METHOD(New);
    static NAN_METHOD(CreateCompileUnit);
    static NAN_METHOD(CreateFile);
    static NAN_METHOD(CreateFunction);
    static NAN_METHOD(CreateLexicalBlock);
    static NAN_METHOD(Finalize);

    llvm::DICompositeType CreateDIFunctionType(llvm::DIFile file, llvm::FunctionType *fty);

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };


  class DIDescriptor : public Nan::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Local<v8::Value> Create(llvm::DIDescriptor descriptor);

    static NAN_METHOD(New);
    static NAN_METHOD(Verify);

    static llvm::DIDescriptor GetLLVMObj (v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DIDescriptor>(value->ToObject())->llvm_didescriptor;
    }

    static Nan::Persistent<v8::FunctionTemplate> constructor;
  private:
    ::llvm::DIDescriptor llvm_didescriptor;

    DIDescriptor(llvm::DIDescriptor llvm_didescriptor);
    DIDescriptor();
    virtual ~DIDescriptor();

    static Nan::Persistent<v8::Function> constructor_func;
  };

  class DIType : public Nan::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Local<v8::Value> Create(llvm::DIType type);
    static NAN_METHOD(New);

    static llvm::DIType GetLLVMObj (v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DIType>(value->ToObject())->llvm_ditype;
    }

  private:
    ::llvm::DIType llvm_ditype;

    DIType(llvm::DIType llvm_ditype);
    DIType();
    virtual ~DIType();

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

  class DIScope : public Nan::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Local<v8::Value> Create(llvm::DIScope scope);
    static NAN_METHOD(New);

    static llvm::DIScope GetLLVMObj (v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DIScope>(value->ToObject())->llvm_discope;
    }

    static Nan::Persistent<v8::FunctionTemplate> constructor;
  private:
    ::llvm::DIScope llvm_discope;

    DIScope(llvm::DIScope llvm_discope);
    DIScope();
    virtual ~DIScope();

    static Nan::Persistent<v8::Function> constructor_func;
  };

  class DISubprogram : public Nan::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Local<v8::Value> Create(llvm::DISubprogram subprogram);

    static NAN_METHOD(New);
    static NAN_METHOD(Verify);

    static llvm::DISubprogram GetLLVMObj (v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DISubprogram>(value->ToObject())->llvm_disubprogram;
    }

  private:
    ::llvm::DISubprogram llvm_disubprogram;

    DISubprogram(llvm::DISubprogram llvm_disubprogram);
    DISubprogram();
    virtual ~DISubprogram();

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

  class DIFile : public Nan::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Local<v8::Value> Create(llvm::DIFile file);

    static NAN_METHOD(New);
    static NAN_METHOD(Verify);

    static llvm::DIFile GetLLVMObj (v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DIFile>(value->ToObject())->llvm_difile;
    }

  private:
    ::llvm::DIFile llvm_difile;

    DIFile(llvm::DIFile llvm_difile);
    DIFile();
    virtual ~DIFile();

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };

  class DILexicalBlock : public Nan::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Local<v8::Value> Create(llvm::DILexicalBlock lexical_block);
    static NAN_METHOD(New);

    static llvm::DILexicalBlock GetLLVMObj (v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DILexicalBlock>(value->ToObject())->llvm_dilexicalblock;
    }

  private:
    ::llvm::DILexicalBlock llvm_dilexicalblock;

    DILexicalBlock(llvm::DILexicalBlock llvm_dilexicalblock);
    DILexicalBlock();
    virtual ~DILexicalBlock();

    static Nan::Persistent<v8::FunctionTemplate> constructor;
    static Nan::Persistent<v8::Function> constructor_func;
  };
  class DebugLoc : public Nan::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Local<v8::Value> Create(llvm::DebugLoc debugLoc);
    static NAN_METHOD(New);

    static llvm::DebugLoc GetLLVMObj (v8::Local<v8::Value> value) {
      return Nan::ObjectWrap::Unwrap<DebugLoc>(value->ToObject())->llvm_debugloc;
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

#endif

#endif /* NODE_LLVM_DIBUILDER_H */

