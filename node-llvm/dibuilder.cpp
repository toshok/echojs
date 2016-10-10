#if false
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

  void DIBuilder::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("DIBuilder").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "createCompileUnit", DIBuilder::CreateCompileUnit);
    Nan::SetPrototypeMethod(ctor, "createFile", DIBuilder::CreateFile);
    Nan::SetPrototypeMethod(ctor, "createFunction", DIBuilder::CreateFunction);
    Nan::SetPrototypeMethod(ctor, "createLexicalBlock", DIBuilder::CreateLexicalBlock);
    Nan::SetPrototypeMethod(ctor, "finalize", DIBuilder::Finalize);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("DIBuilder").ToLocalChecked(), ctor_func);
  }


  NAN_METHOD(DIBuilder::New) {
    REQ_LLVM_MODULE_ARG(0, module);

    DIBuilder* dib = new DIBuilder(new llvm::DIBuilder (*module));
    dib->Wrap(info.This());

    info.GetReturnValue().Set(info.This());
  }

  NAN_METHOD(DIBuilder::CreateCompileUnit) {
    auto dib = Unwrap(info.This());
  
    REQ_UTF8_ARG(0, file);
    REQ_UTF8_ARG(1, dir);
    REQ_UTF8_ARG(2, producer);
    REQ_BOOL_ARG(3, isOptimized);
    REQ_UTF8_ARG(4, flags);
    REQ_INT_ARG(5, runtimeVersion);

    Local<v8::Value> result = DIDescriptor::Create(dib->llvm_obj->createCompileUnit(llvm::dwarf::DW_LANG_C99,
										    *file, *dir,
										    *producer,
										    isOptimized,
										    *flags,
										    runtimeVersion));
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(DIBuilder::CreateFile) {
    auto dib = Unwrap(info.This());
  
    REQ_UTF8_ARG(0, file);
    REQ_UTF8_ARG(1, dir);

    llvm::DIFile llvm_file = dib->llvm_obj->createFile(*file, *dir);

#if notyet
    std::vector<llvm::Value*> membertypes;
    membertypes.push_back (dib->llvm_obj->createBasicType ("unsigned long long", 64, 64, llvm::dwarf::DW_ATE_unsigned));

    dib->ejsValueType = dib->llvm_obj->createStructType (llvm_file, "ejsval",
							 llvm_file, 0, 64, 64, 0, llvm::DIType(),
							 dib->llvm_obj->getOrCreateArray(membertypes));

    dib->ejsValuePointerType = dib->llvm_obj->createPointerType (dib->ejsValueType, sizeof(void*)*8);
#endif

    Local<v8::Value> result = DIFile::Create(llvm_file);
    info.GetReturnValue().Set(result);
  }

  llvm::DICompositeType DIBuilder::CreateDIFunctionType(llvm::DIFile file, llvm::FunctionType *fty)
  {
    // XXX add function parameter types
    llvm::DITypeArray param_types = llvm_obj->getOrCreateTypeArray(llvm::None);
    return llvm_obj->createSubroutineType(file, param_types);
  }

  NAN_METHOD(DIBuilder::CreateFunction) {
    auto dib = Unwrap(info.This());
  
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

    
    Local<v8::Value> result = DISubprogram::Create(dib->llvm_obj->createFunction (discope,
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
    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(DIBuilder::CreateLexicalBlock) {
    auto dib = Unwrap(info.This());

    REQ_LLVM_DISCOPE_ARG(0, parentScope);
    REQ_LLVM_DIFILE_ARG(1, file);
    REQ_INT_ARG(2, line);
    REQ_INT_ARG(3, col);

    Local<v8::Value> result = DILexicalBlock::Create(dib->llvm_obj->createLexicalBlock (parentScope, file, line, col));

    info.GetReturnValue().Set(result);
  }

  NAN_METHOD(DIBuilder::Finalize) {
    auto dib = Unwrap(info.This());
  
    dib->llvm_obj->finalize();
  }

  Nan::Persistent<v8::FunctionTemplate> DIBuilder::constructor;
  Nan::Persistent<v8::Function> DIBuilder::constructor_func;


  // DIDescriptor


  void DIDescriptor::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("DIDescriptor").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "verify", DIDescriptor::Verify);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("DIDescriptor").ToLocalChecked(), ctor_func);
  }

  v8::Local<v8::Value> DIDescriptor::Create(llvm::DIDescriptor llvm_didescriptor) {
    Nan::EscapableHandleScope scope;
    Local<Object> new_instance = Nan::New(DIDescriptor::constructor_func)->NewInstance();
    DIDescriptor* new_didescriptor = new DIDescriptor(llvm_didescriptor);
    new_didescriptor->Wrap(new_instance);
    return scope.Escape(new_instance);
  }

  NAN_METHOD(DIDescriptor::New) {
    info.GetReturnValue().Set(info.This());
  }

  DIDescriptor::DIDescriptor(llvm::DIDescriptor llvm_didescriptor) : llvm_didescriptor(llvm_didescriptor) { }

  DIDescriptor::DIDescriptor() { }

  DIDescriptor::~DIDescriptor() { }

  NAN_METHOD(DIDescriptor::Verify) {
    DIDescriptor* did = ObjectWrap::Unwrap<DIDescriptor>(info.This());
    bool passed = did->llvm_didescriptor.Verify();
    info.GetReturnValue().Set(passed);
  }

  Nan::Persistent<v8::FunctionTemplate> DIDescriptor::constructor;
  Nan::Persistent<v8::Function> DIDescriptor::constructor_func;




  // DIType


  void DIType::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("DIType").ToLocalChecked());

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("DIType").ToLocalChecked(), ctor_func);
  }

  v8::Local<v8::Value> DIType::Create(llvm::DIType llvm_ditype)
  {
    Nan::EscapableHandleScope scope;
    Local<Object> new_instance = Nan::New(DIType::constructor_func)->NewInstance();
    DIType* new_ditype = new DIType(llvm_ditype);
    new_ditype->Wrap(new_instance);
    return scope.Escape(new_instance);
  }

  NAN_METHOD(DIType::New) {
    info.GetReturnValue().Set(info.This());
  }

  DIType::DIType(llvm::DIType llvm_ditype) : llvm_ditype(llvm_ditype) { }

  DIType::DIType() { }

  DIType::~DIType() { }

  Nan::Persistent<v8::FunctionTemplate> DIType::constructor;
  Nan::Persistent<v8::Function> DIType::constructor_func;

  // DIScope


  void DIScope::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(DIDescriptor::constructor));

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("DIType").ToLocalChecked());

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("DIScope").ToLocalChecked(), ctor_func);
  }

  v8::Local<v8::Value> DIScope::Create(llvm::DIScope llvm_discope) {
    Nan::EscapableHandleScope scope;
    Local<Object> new_instance = Nan::New(DIScope::constructor_func)->NewInstance();
    DIScope* new_discope = new DIScope(llvm_discope);
    new_discope->Wrap(new_instance);
    return scope.Escape(new_instance);
  }

  NAN_METHOD(DIScope::New) {
    info.GetReturnValue().Set(info.This());
  }

  DIScope::DIScope(llvm::DIScope llvm_discope) : llvm_discope(llvm_discope) { }

  DIScope::DIScope() { }

  DIScope::~DIScope() { }

  Nan::Persistent<v8::FunctionTemplate> DIScope::constructor;
  Nan::Persistent<v8::Function> DIScope::constructor_func;


  // DISubprogram


  void DISubprogram::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(DIScope::constructor));

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("DISubprogram").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "verify", DISubprogram::Verify);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("DISubprogram").ToLocalChecked(), ctor_func);
  }

  v8::Local<v8::Value> DISubprogram::Create(llvm::DISubprogram llvm_disubprogram) {
    Nan::EscapableHandleScope scope;
    Local<Object> new_instance = Nan::New(DISubprogram::constructor_func)->NewInstance();
    DISubprogram* new_disubprogram = new DISubprogram(llvm_disubprogram);
    new_disubprogram->Wrap(new_instance);
    return scope.Escape(new_instance);
  }

  NAN_METHOD(DISubprogram::New) {
    info.GetReturnValue().Set(info.This());
  }

  DISubprogram::DISubprogram(llvm::DISubprogram llvm_disubprogram) : llvm_disubprogram(llvm_disubprogram) { }

  DISubprogram::DISubprogram() { }

  DISubprogram::~DISubprogram() { }

  NAN_METHOD(DISubprogram::Verify) {
    DISubprogram* dis = ObjectWrap::Unwrap<DISubprogram>(info.This());
    bool passed = dis->llvm_disubprogram.Verify();
    info.GetReturnValue().Set(passed);
  }

  Nan::Persistent<v8::FunctionTemplate> DISubprogram::constructor;
  Nan::Persistent<v8::Function> DISubprogram::constructor_func;


  // DIFile

  void DIFile::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("DIFile").ToLocalChecked());

    Nan::SetPrototypeMethod(ctor, "verify", DIFile::Verify);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("DIFile").ToLocalChecked(), ctor_func);
  }

  v8::Local<v8::Value> DIFile::Create(llvm::DIFile llvm_difile) {
    Nan::EscapableHandleScope scope;
    Local<Object> new_instance = Nan::New(DIFile::constructor_func)->NewInstance();
    DIFile* new_difile = new DIFile(llvm_difile);
    new_difile->Wrap(new_instance);
    return scope.Escape(new_instance);
  }

  NAN_METHOD(DIFile::New) {
    info.GetReturnValue().Set(info.This());
  }

  DIFile::DIFile(llvm::DIFile llvm_difile) : llvm_difile(llvm_difile) { }

  DIFile::DIFile() { }

  DIFile::~DIFile() { }

  NAN_METHOD(DIFile::Verify) {
    DIFile* dif = ObjectWrap::Unwrap<DIFile>(info.This());
    bool passed = dif->llvm_difile.Verify();
    info.GetReturnValue().Set(passed);
  }

  Nan::Persistent<v8::FunctionTemplate> DIFile::constructor;
  Nan::Persistent<v8::Function> DIFile::constructor_func;



  // DILexicalBlock


  void DILexicalBlock::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->Inherit (Nan::New<v8::FunctionTemplate>(DIScope::constructor));

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("DILexicalBlock").ToLocalChecked());

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("DILexicalBlock").ToLocalChecked(), ctor_func);
  }

  v8::Local<v8::Value> DILexicalBlock::Create(llvm::DILexicalBlock llvm_dilexicalblock) {
    Nan::EscapableHandleScope scope;
    Local<Object> new_instance = Nan::New(DILexicalBlock::constructor_func)->NewInstance();
    DILexicalBlock* new_dilexicalblock = new DILexicalBlock(llvm_dilexicalblock);
    new_dilexicalblock->Wrap(new_instance);
    return scope.Escape(new_instance);
  }

  NAN_METHOD(DILexicalBlock::New) {
    info.GetReturnValue().Set(info.This());
  }

  DILexicalBlock::DILexicalBlock(llvm::DILexicalBlock llvm_dilexicalblock)
    : llvm_dilexicalblock(llvm_dilexicalblock) { }

  DILexicalBlock::DILexicalBlock() { }

  DILexicalBlock::~DILexicalBlock() { }

  Nan::Persistent<v8::FunctionTemplate> DILexicalBlock::constructor;
  Nan::Persistent<v8::Function> DILexicalBlock::constructor_func;

  // DebugLoc


  void DebugLoc::Init(Handle<Object> target) {
    Nan::HandleScope scope;

    Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(New);
    constructor.Reset(ctor);

    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(Nan::New("DebugLoc").ToLocalChecked());

    Nan::SetMethod(ctor, "get", DebugLoc::Get);

    Local<v8::Function> ctor_func = ctor->GetFunction();
    constructor_func.Reset(ctor_func);
    target->Set(Nan::New("DebugLoc").ToLocalChecked(), ctor_func);
  }

  v8::Local<v8::Value> DebugLoc::Create(llvm::DebugLoc llvm_debugloc) {
    Nan::EscapableHandleScope scope;
    Local<Object> new_instance = Nan::New(DebugLoc::constructor_func)->NewInstance();
    DebugLoc* new_debugloc = new DebugLoc(llvm_debugloc);
    new_debugloc->Wrap(new_instance);
    return scope.Escape(new_instance);
  }

  NAN_METHOD(DebugLoc::New) {
    info.GetReturnValue().Set(info.This());
  }

  DebugLoc::DebugLoc(llvm::DebugLoc llvm_debugloc)
    : llvm_debugloc(llvm_debugloc) { }

  DebugLoc::DebugLoc() { }

  DebugLoc::~DebugLoc() { }

  NAN_METHOD(DebugLoc::Get) {
    REQ_INT_ARG(0, line);
    REQ_INT_ARG(1, column);
    REQ_LLVM_DISCOPE_ARG(2, discope);

    info.GetReturnValue().Set(DebugLoc::Create(llvm::DebugLoc::get(line, column, discope, NULL)));
  }

  Nan::Persistent<v8::FunctionTemplate> DebugLoc::constructor;
  Nan::Persistent<v8::Function> DebugLoc::constructor_func;

}
#endif
