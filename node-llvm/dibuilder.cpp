#if notyet

#include "node-llvm.h"
#include "dibuilder.h"
#include "function.h"
#include "module.h"
#include "type.h"
#include "value.h"

using namespace node;
using namespace v8;


namespace jsllvm {

  // DIBuilder

  void DIBuilder::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("DIBuilder"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "createCompileUnit", DIBuilder::CreateCompileUnit);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "createFile", DIBuilder::CreateFile);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "createFunction", DIBuilder::CreateFunction);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "createLexicalBlock", DIBuilder::CreateLexicalBlock);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "finalize", DIBuilder::Finalize);

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("DIBuilder"),
		s_func);
  }

  v8::Handle<v8::Value> DIBuilder::New(llvm::DIBuilder *llvm_dibuilder)
  {
    HandleScope scope;
    Local<Object> new_instance = DIBuilder::s_func->NewInstance();
    DIBuilder* new_dibuilder = new DIBuilder(llvm_dibuilder);
    new_dibuilder->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> DIBuilder::New(const Arguments& args)
  {
    HandleScope scope;

    REQ_LLVM_MODULE_ARG(0, module);

    DIBuilder* dib = new DIBuilder(new llvm::DIBuilder (*module));
    dib->Wrap(args.This());

    return scope.Close(args.This());
  }

  DIBuilder::DIBuilder (llvm::DIBuilder *llvm_dibuilder)
    : llvm_dibuilder (llvm_dibuilder)
  {
    Initialize();
  }

  DIBuilder::DIBuilder()
    : llvm_dibuilder (NULL)
  {
  }


  DIBuilder::~DIBuilder()
  {
  }

  void
  DIBuilder::Initialize ()
  {
  }

  Handle<v8::Value> DIBuilder::CreateCompileUnit(const Arguments& args)
  {
    HandleScope scope;
    DIBuilder* dib = ObjectWrap::Unwrap<DIBuilder>(args.This());
  
    REQ_UTF8_ARG(0, file);
    REQ_UTF8_ARG(1, dir);
    REQ_UTF8_ARG(2, producer);
    REQ_BOOL_ARG(3, isOptimized);
    REQ_UTF8_ARG(4, flags);
    REQ_INT_ARG(5, runtimeVersion);

    Handle<v8::Value> result = DIDescriptor::New(dib->llvm_dibuilder->createCompileUnit(llvm::dwarf::DW_LANG_C99,
											*file, *dir,
											*producer,
											isOptimized,
											*flags,
											runtimeVersion));
    return scope.Close(result);
  }

  Handle<v8::Value> DIBuilder::CreateFile(const Arguments& args)
  {
    HandleScope scope;
    DIBuilder* dib = ObjectWrap::Unwrap<DIBuilder>(args.This());
  
    REQ_UTF8_ARG(0, file);
    REQ_UTF8_ARG(1, dir);

    llvm::DIFile llvm_file = dib->llvm_dibuilder->createFile(*file, *dir);

    std::vector<llvm::Value*> membertypes;
    membertypes.push_back (dib->llvm_dibuilder->createBasicType ("unsigned long long", 64, 64, llvm::dwarf::DW_ATE_unsigned));

    dib->ejsValueType = dib->llvm_dibuilder->createStructType (llvm_file, "ejsval",
							       llvm_file, 0, 64, 64, 0, llvm::DIType(),
							       dib->llvm_dibuilder->getOrCreateArray(membertypes));

    dib->ejsValuePointerType = dib->llvm_dibuilder->createPointerType (dib->ejsValueType, sizeof(void*)*8);

    Handle<v8::Value> result = DIFile::New(llvm_file);
    return scope.Close(result);
  }

  llvm::DICompositeType DIBuilder::CreateDIFunctionType(llvm::DIFile file, llvm::FunctionType *fty)
  {
    std::vector<llvm::Value*> args;
    args.push_back (ejsValueType);
    args.push_back (ejsValueType);
    args.push_back (llvm_dibuilder->createBasicType ("unsigned int", 32, 32, llvm::dwarf::DW_ATE_unsigned));
    args.push_back (ejsValuePointerType);

    return llvm_dibuilder->createSubroutineType(file, llvm_dibuilder->getOrCreateArray(args));
  }

  Handle<v8::Value> DIBuilder::CreateFunction(const Arguments& args)
  {
    HandleScope scope;
    DIBuilder* dib = ObjectWrap::Unwrap<DIBuilder>(args.This());
  
    REQ_LLVM_DISCOPE_ARG(0, discope);
    REQ_UTF8_ARG(1, name);
    REQ_UTF8_ARG(2, linkageName);
    REQ_LLVM_DIFILE_ARG(3, file);
    REQ_INT_ARG(4, line_no);
    REQ_BOOL_ARG(5, isLocalToUnit);
    REQ_BOOL_ARG(6, isDefinition);
    REQ_INT_ARG(7, scopeLine);
    REQ_INT_ARG(8, flags);
    REQ_BOOL_ARG(9, isOptimized);
    REQ_LLVM_FUN_ARG(10, fn);

    
    Handle<v8::Value> result = DISubprogram::New(dib->llvm_dibuilder->createFunction (discope,
										      *name,
										      *linkageName,
										      file,
										      line_no,
										      dib->CreateDIFunctionType(file, fn->getFunctionType()),
										      isLocalToUnit,
										      isDefinition,
										      scopeLine,
										      flags,
										      isOptimized,
										      fn));
    return scope.Close(result);
  }

  Handle<v8::Value> DIBuilder::CreateLexicalBlock(const Arguments& args)
  {
    HandleScope scope;
    DIBuilder* dib = ObjectWrap::Unwrap<DIBuilder>(args.This());

    REQ_LLVM_DISCOPE_ARG(0, parentScope);
    REQ_LLVM_DIFILE_ARG(1, file);
    REQ_INT_ARG(2, line);
    REQ_INT_ARG(3, col);

    Handle<v8::Value> result = DILexicalBlock::New(dib->llvm_dibuilder->createLexicalBlock (parentScope, file, line, col));

    return scope.Close(result);
  }

  Handle<v8::Value> DIBuilder::Finalize(const Arguments& args)
  {
    HandleScope scope;
    DIBuilder* dib = ObjectWrap::Unwrap<DIBuilder>(args.This());
  
    dib->llvm_dibuilder->finalize();
    return scope.Close(Undefined());
  }

  Persistent<FunctionTemplate> DIBuilder::s_ct;
  Persistent<v8::Function> DIBuilder::s_func;


  // DIDescriptor


  void DIDescriptor::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("DIDescriptor"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "verify", DIDescriptor::Verify);

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("DIDescriptor"),
		s_func);
  }

  v8::Handle<v8::Value> DIDescriptor::New(llvm::DIDescriptor llvm_didescriptor)
  {
    HandleScope scope;
    Local<Object> new_instance = DIDescriptor::s_func->NewInstance();
    DIDescriptor* new_didescriptor = new DIDescriptor(llvm_didescriptor);
    new_didescriptor->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> DIDescriptor::New(const Arguments& args)
  {
    HandleScope scope;
    return scope.Close(args.This());
  }

  DIDescriptor::DIDescriptor(llvm::DIDescriptor llvm_didescriptor)
    : llvm_didescriptor(llvm_didescriptor)
  {
  }

  DIDescriptor::DIDescriptor()
  {
  }

  DIDescriptor::~DIDescriptor()
  {
  }

  Handle<v8::Value> DIDescriptor::Verify(const Arguments& args)
  {
    HandleScope scope;

    DIDescriptor* did = ObjectWrap::Unwrap<DIDescriptor>(args.This());
    bool passed = did->llvm_didescriptor.Verify();

    return scope.Close(passed ? True() : False());
  }

  Persistent<FunctionTemplate> DIDescriptor::s_ct;
  Persistent<v8::Function> DIDescriptor::s_func;




  // DIType


  void DIType::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("DIType"));

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("DIType"),
		s_func);
  }

  v8::Handle<v8::Value> DIType::New(llvm::DIType llvm_ditype)
  {
    HandleScope scope;
    Local<Object> new_instance = DIType::s_func->NewInstance();
    DIType* new_ditype = new DIType(llvm_ditype);
    new_ditype->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> DIType::New(const Arguments& args)
  {
    HandleScope scope;
    return scope.Close(args.This());
  }

  DIType::DIType(llvm::DIType llvm_ditype)
    : llvm_ditype(llvm_ditype)
  {
  }

  DIType::DIType()
  {
  }

  DIType::~DIType()
  {
  }

  Persistent<FunctionTemplate> DIType::s_ct;
  Persistent<v8::Function> DIType::s_func;

  // DIScope


  void DIScope::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->Inherit(DIDescriptor::s_ct);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("DIScope"));

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("DIScope"),
		s_func);
  }

  v8::Handle<v8::Value> DIScope::New(llvm::DIScope llvm_discope)
  {
    HandleScope scope;
    Local<Object> new_instance = DIScope::s_func->NewInstance();
    DIScope* new_discope = new DIScope(llvm_discope);
    new_discope->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> DIScope::New(const Arguments& args)
  {
    HandleScope scope;
    return scope.Close(args.This());
  }

  DIScope::DIScope(llvm::DIScope llvm_discope)
    : llvm_discope(llvm_discope)
  {
  }

  DIScope::DIScope()
  {
  }

  DIScope::~DIScope()
  {
  }

  Persistent<FunctionTemplate> DIScope::s_ct;
  Persistent<v8::Function> DIScope::s_func;


  // DISubprogram


  void DISubprogram::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->Inherit(DIScope::s_ct);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("DISubprogram"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "verify", DISubprogram::Verify);

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("DISubprogram"),
		s_func);
  }

  v8::Handle<v8::Value> DISubprogram::New(llvm::DISubprogram llvm_disubprogram)
  {
    HandleScope scope;
    Local<Object> new_instance = DISubprogram::s_func->NewInstance();
    DISubprogram* new_disubprogram = new DISubprogram(llvm_disubprogram);
    new_disubprogram->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> DISubprogram::New(const Arguments& args)
  {
    HandleScope scope;
    return scope.Close(args.This());
  }

  DISubprogram::DISubprogram(llvm::DISubprogram llvm_disubprogram)
    : llvm_disubprogram(llvm_disubprogram)
  {
  }

  DISubprogram::DISubprogram()
  {
  }

  DISubprogram::~DISubprogram()
  {
  }

  Handle<v8::Value> DISubprogram::Verify(const Arguments& args)
  {
    HandleScope scope;

    DISubprogram* dis = ObjectWrap::Unwrap<DISubprogram>(args.This());
    bool passed = dis->llvm_disubprogram.Verify();

    return scope.Close(passed ? True() : False());
  }

  Persistent<FunctionTemplate> DISubprogram::s_ct;
  Persistent<v8::Function> DISubprogram::s_func;




  // DIFile


  void DIFile::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("DIFile"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "verify", DIFile::Verify);

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("DIFile"),
		s_func);
  }

  v8::Handle<v8::Value> DIFile::New(llvm::DIFile llvm_difile)
  {
    HandleScope scope;
    Local<Object> new_instance = DIFile::s_func->NewInstance();
    DIFile* new_difile = new DIFile(llvm_difile);
    new_difile->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> DIFile::New(const Arguments& args)
  {
    HandleScope scope;
    return scope.Close(args.This());
  }

  DIFile::DIFile(llvm::DIFile llvm_difile)
    : llvm_difile(llvm_difile)
  {
  }

  DIFile::DIFile()
  {
  }

  DIFile::~DIFile()
  {
  }

  Handle<v8::Value> DIFile::Verify(const Arguments& args)
  {
    HandleScope scope;

    DIFile* dif = ObjectWrap::Unwrap<DIFile>(args.This());
    bool passed = dif->llvm_difile.Verify();

    return scope.Close(passed ? True() : False());
  }

  Persistent<FunctionTemplate> DIFile::s_ct;
  Persistent<v8::Function> DIFile::s_func;



  // DILexicalBlock


  void DILexicalBlock::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->Inherit(DIScope::s_ct);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("DILexicalBlock"));

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("DILexicalBlock"),
		s_func);
  }

  v8::Handle<v8::Value> DILexicalBlock::New(llvm::DILexicalBlock llvm_dilexicalblock)
  {
    HandleScope scope;
    Local<Object> new_instance = DILexicalBlock::s_func->NewInstance();
    DILexicalBlock* new_dilexicalblock = new DILexicalBlock(llvm_dilexicalblock);
    new_dilexicalblock->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> DILexicalBlock::New(const Arguments& args)
  {
    HandleScope scope;
    return scope.Close(args.This());
  }

  DILexicalBlock::DILexicalBlock(llvm::DILexicalBlock llvm_dilexicalblock)
    : llvm_dilexicalblock(llvm_dilexicalblock)
  {
  }

  DILexicalBlock::DILexicalBlock()
  {
  }

  DILexicalBlock::~DILexicalBlock()
  {
  }

  Persistent<FunctionTemplate> DILexicalBlock::s_ct;
  Persistent<v8::Function> DILexicalBlock::s_func;

  // DebugLoc


  void DebugLoc::Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("DebugLoc"));

    NODE_SET_METHOD(s_ct, "get", DebugLoc::Get);

    s_func = Persistent<v8::Function>::New(s_ct->GetFunction());
    target->Set(String::NewSymbol("DebugLoc"),
		s_func);
  }

  v8::Handle<v8::Value> DebugLoc::New(llvm::DebugLoc llvm_debugloc)
  {
    HandleScope scope;
    Local<Object> new_instance = DebugLoc::s_func->NewInstance();
    DebugLoc* new_debugloc = new DebugLoc(llvm_debugloc);
    new_debugloc->Wrap(new_instance);
    return scope.Close(new_instance);
  }

  Handle<v8::Value> DebugLoc::New(const Arguments& args)
  {
    HandleScope scope;
    return scope.Close(args.This());
  }

  DebugLoc::DebugLoc(llvm::DebugLoc llvm_debugloc)
    : llvm_debugloc(llvm_debugloc)
  {
  }

  DebugLoc::DebugLoc()
  {
  }

  DebugLoc::~DebugLoc()
  {
  }

  Handle<v8::Value> DebugLoc::Get(const Arguments& args)
  {
    HandleScope scope;

    REQ_INT_ARG(0, line);
    REQ_INT_ARG(1, column);
    REQ_LLVM_DISCOPE_ARG(2, discope);

    return scope.Close(DebugLoc::New(llvm::DebugLoc::get(line, column, discope, NULL)));
  }

  Persistent<FunctionTemplate> DebugLoc::s_ct;
  Persistent<v8::Function> DebugLoc::s_func;

};

#endif
