#ifndef NODE_LLVM_DIBUILDER_H
#define NODE_LLVM_DIBUILDER_H

#include "node-llvm.h"
namespace jsllvm {


  class DIBuilder : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::DIBuilder *builder);
    static v8::Handle<v8::Value> New(const v8::Arguments& args);

    static llvm::DIBuilder* GetLLVMObj (v8::Local<v8::Value> value) {
      if (value->IsNull())
	return NULL;
      return node::ObjectWrap::Unwrap<DIBuilder>(value->ToObject())->llvm_dibuilder;
    }

  private:
    ::llvm::DIBuilder* llvm_dibuilder;

    DIBuilder(llvm::DIBuilder *llvm_dibuilder);
    DIBuilder();
    virtual ~DIBuilder();

    void Initialize ();

    ::llvm::DIType ejsValueType;
    ::llvm::DIType ejsValuePointerType;

    static v8::Handle<v8::Value> CreateCompileUnit(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateFile(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateFunction(const v8::Arguments& args);
    static v8::Handle<v8::Value> CreateLexicalBlock(const v8::Arguments& args);
    static v8::Handle<v8::Value> Finalize(const v8::Arguments& args);

    llvm::DICompositeType CreateDIFunctionType(llvm::DIFile file, llvm::FunctionType *fty);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };


  class DIDescriptor : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::DIDescriptor descriptor);
    static v8::Handle<v8::Value> New(const v8::Arguments& args);

    static v8::Handle<v8::Value> Verify(const v8::Arguments& args);

    static llvm::DIDescriptor GetLLVMObj (v8::Local<v8::Value> value) {
      return node::ObjectWrap::Unwrap<DIDescriptor>(value->ToObject())->llvm_didescriptor;
    }

    static v8::Persistent<v8::FunctionTemplate> s_ct;
  private:
    ::llvm::DIDescriptor llvm_didescriptor;

    DIDescriptor(llvm::DIDescriptor llvm_didescriptor);
    DIDescriptor();
    virtual ~DIDescriptor();

    static v8::Persistent<v8::Function> s_func;
  };

  class DIType : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::DIType type);
    static v8::Handle<v8::Value> New(const v8::Arguments& args);

    static llvm::DIType GetLLVMObj (v8::Local<v8::Value> value) {
      return node::ObjectWrap::Unwrap<DIType>(value->ToObject())->llvm_ditype;
    }

  private:
    ::llvm::DIType llvm_ditype;

    DIType(llvm::DIType llvm_ditype);
    DIType();
    virtual ~DIType();

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

  class DIScope : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::DIScope scope);
    static v8::Handle<v8::Value> New(const v8::Arguments& args);

    static llvm::DIScope GetLLVMObj (v8::Local<v8::Value> value) {
      return node::ObjectWrap::Unwrap<DIScope>(value->ToObject())->llvm_discope;
    }

    static v8::Persistent<v8::FunctionTemplate> s_ct;
  private:
    ::llvm::DIScope llvm_discope;

    DIScope(llvm::DIScope llvm_discope);
    DIScope();
    virtual ~DIScope();

    static v8::Persistent<v8::Function> s_func;
  };

  class DISubprogram : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::DISubprogram subprogram);
    static v8::Handle<v8::Value> New(const v8::Arguments& args);

    static v8::Handle<v8::Value> Verify(const v8::Arguments& args);

    static llvm::DISubprogram GetLLVMObj (v8::Local<v8::Value> value) {
      return node::ObjectWrap::Unwrap<DISubprogram>(value->ToObject())->llvm_disubprogram;
    }

  private:
    ::llvm::DISubprogram llvm_disubprogram;

    DISubprogram(llvm::DISubprogram llvm_disubprogram);
    DISubprogram();
    virtual ~DISubprogram();

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

  class DIFile : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::DIFile file);
    static v8::Handle<v8::Value> New(const v8::Arguments& args);

    static v8::Handle<v8::Value> Verify(const v8::Arguments& args);

    static llvm::DIFile GetLLVMObj (v8::Local<v8::Value> value) {
      return node::ObjectWrap::Unwrap<DIFile>(value->ToObject())->llvm_difile;
    }

  private:
    ::llvm::DIFile llvm_difile;

    DIFile(llvm::DIFile llvm_difile);
    DIFile();
    virtual ~DIFile();

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

  class DILexicalBlock : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::DILexicalBlock lexical_block);
    static v8::Handle<v8::Value> New(const v8::Arguments& args);

    static llvm::DILexicalBlock GetLLVMObj (v8::Local<v8::Value> value) {
      return node::ObjectWrap::Unwrap<DILexicalBlock>(value->ToObject())->llvm_dilexicalblock;
    }

  private:
    ::llvm::DILexicalBlock llvm_dilexicalblock;

    DILexicalBlock(llvm::DILexicalBlock llvm_dilexicalblock);
    DILexicalBlock();
    virtual ~DILexicalBlock();

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

  class DebugLoc : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> target);

    static v8::Handle<v8::Value> New(llvm::DebugLoc debugLoc);
    static v8::Handle<v8::Value> New(const v8::Arguments& args);

    static llvm::DebugLoc GetLLVMObj (v8::Local<v8::Value> value) {
      return node::ObjectWrap::Unwrap<DebugLoc>(value->ToObject())->llvm_debugloc;
    }

  private:
    ::llvm::DebugLoc llvm_debugloc;

    DebugLoc(llvm::DebugLoc llvm_debugloc);
    DebugLoc();
    virtual ~DebugLoc();

    static v8::Handle<v8::Value> Get(const v8::Arguments& args);

    static v8::Persistent<v8::FunctionTemplate> s_ct;
    static v8::Persistent<v8::Function> s_func;
  };

};


#endif /* NODE_LLVM_DIBUILDER_H */
