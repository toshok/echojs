/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#if false
#include "ejs-llvm.h"
#include "ejs-error.h"
#include "ejs-object.h"
#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-function.h"
#include "ejs-proxy.h"
#include "ejs-string.h"
#include "ejs-symbol.h"

#include "dibuilder.h"
#include "function.h"
#include "module.h"

namespace ejsllvm {

    // DIBuilder

    typedef struct {
        /* object header */
        EJSObject obj;

        /* load specific data */
        llvm::DIBuilder *llvm_dibuilder;
    } DIBuilder;


    static EJSSpecOps _ejs_DIBuilder_specops;
    static ejsval _ejs_DIBuilder_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_DIBuilder EJSVAL_ALIGNMENT;

    static EJSObject* DIBuilder_allocate()
    {
        return (EJSObject*)_ejs_gc_new(DIBuilder);
    }

    static EJS_NATIVE_FUNC(DIBuilder_impl) {
        if (EJSVAL_IS_UNDEFINED(newTarget)) {
            // called as a function
            EJS_NOT_IMPLEMENTED();
        }
        else {
            ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_DIBuilder_prototype, &_ejs_DIBuilder_specops);
            *_this = O;

            DIBuilder* O_ = (DIBuilder*)EJSVAL_TO_OBJECT(O);
            REQ_LLVM_MODULE_ARG(0, module);

            O_->llvm_dibuilder = new llvm::DIBuilder (*module);
            return *_this;
        }
    }

    static EJS_NATIVE_FUNC(DIBuilder_prototype_createCompileUnit) {
        REQ_UTF8_ARG(0, file);
        REQ_UTF8_ARG(1, dir);
        REQ_UTF8_ARG(2, producer);
        REQ_BOOL_ARG(3, isOptimized);
        REQ_UTF8_ARG(4, flags);
        REQ_INT_ARG(5, runtimeVersion);

        DIBuilder* dib = ((DIBuilder*)EJSVAL_TO_OBJECT(*_this));

        return DIDescriptor_new(dib->llvm_dibuilder->createCompileUnit(llvm::dwarf::DW_LANG_C99,
                                                                       file, dir, producer,
                                                                       isOptimized, flags,
                                                                       runtimeVersion));
    }

    static EJS_NATIVE_FUNC(DIBuilder_prototype_createFile) {
        REQ_UTF8_ARG(0, file);
        REQ_UTF8_ARG(1, dir);

        DIBuilder* dib = ((DIBuilder*)EJSVAL_TO_OBJECT(*_this));

        llvm::DIFile llvm_file = dib->llvm_dibuilder->createFile(file, dir);

        return DIFile_new(llvm_file);
    }

    static llvm::DICompositeType createDIFunctionType(llvm::DIBuilder* llvm_dibuilder,
                                                      llvm::DIFile file,
                                                      llvm::FunctionType *fty) {
        // XXX add function parameter types
        llvm::DITypeArray param_types = llvm_dibuilder->getOrCreateTypeArray(llvm::None);
        return llvm_dibuilder->createSubroutineType(file, param_types);
    }

    static EJS_NATIVE_FUNC(DIBuilder_prototype_createFunction) {
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

        DIBuilder* dib = ((DIBuilder*)EJSVAL_TO_OBJECT(*_this));

        return DISubprogram_new(dib->llvm_dibuilder->createFunction (discope,
                                                                     name,
                                                                     linkageName,
                                                                     file,
                                                                     line_no,
                                                                     createDIFunctionType(dib->llvm_dibuilder, file, fn->getFunctionType()),
                                                                     isLocalToUnit,
                                                                     isDefinition,
                                                                     scopeLine,
                                                                     flags,
                                                                     isOptimized,
                                                                     fn));
    }

    static EJS_NATIVE_FUNC(DIBuilder_prototype_createLexicalBlock) {
        REQ_LLVM_DISCOPE_ARG(0, parentScope);
        REQ_LLVM_DIFILE_ARG(1, file);
        REQ_INT_ARG(2, line);
        REQ_INT_ARG(3, col);

        DIBuilder* dib = ((DIBuilder*)EJSVAL_TO_OBJECT(*_this));

        return DILexicalBlock_new(dib->llvm_dibuilder->createLexicalBlock (parentScope, file, line, col));
    }

    static EJS_NATIVE_FUNC(DIBuilder_prototype_finalize) {
        DIBuilder* dib = ((DIBuilder*)EJSVAL_TO_OBJECT(*_this));
        dib->llvm_dibuilder->finalize();
        return _ejs_undefined;
    }

    void
    DIBuilder_init (ejsval exports)
    {
        _ejs_DIBuilder_specops = _ejs_Object_specops;
        _ejs_DIBuilder_specops.class_name = "LLVMDIBuilder";
        _ejs_DIBuilder_specops.Allocate = DIBuilder_allocate;

        _ejs_gc_add_root (&_ejs_DIBuilder_prototype);
        _ejs_DIBuilder_prototype = _ejs_object_create(_ejs_Object_prototype);

        _ejs_DIBuilder = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMDIBuilder", (EJSClosureFunc)DIBuilder_impl, _ejs_DIBuilder_prototype);

        _ejs_object_setprop_utf8 (exports,              "DIBuilder", _ejs_DIBuilder);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_DIBuilder_prototype, x, DIBuilder_prototype_##x)

        PROTO_METHOD(createCompileUnit);
        PROTO_METHOD(createFile);
        PROTO_METHOD(createFunction);
        PROTO_METHOD(createLexicalBlock);
        PROTO_METHOD(finalize);

#undef PROTO_METHOD
    }


    // DIDescriptor

    typedef struct {
        /* object header */
        EJSObject obj;

        /* load specific data */
        llvm::DIDescriptor llvm_didescriptor;
    } DIDescriptor;

    static EJSSpecOps _ejs_DIDescriptor_specops;
    static ejsval _ejs_DIDescriptor_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_DIDescriptor EJSVAL_ALIGNMENT;

    static EJSObject* DIDescriptor_allocate()
    {
        return (EJSObject*)_ejs_gc_new(DIDescriptor);
    }

    ejsval DIDescriptor_new(llvm::DIDescriptor llvm_didescriptor) {
        ejsval result = _ejs_object_new (_ejs_DIDescriptor_prototype, &_ejs_DIDescriptor_specops);
        ((DIDescriptor*)EJSVAL_TO_OBJECT(result))->llvm_didescriptor = llvm_didescriptor;
        return result;
    }

    static EJS_NATIVE_FUNC(DIDescriptor_impl) {
        EJS_NOT_IMPLEMENTED();
    }

    static EJS_NATIVE_FUNC(DIDescriptor_prototype_verify) {
        DIDescriptor* did = (DIDescriptor*)EJSVAL_TO_OBJECT(*_this);

        bool passed = did->llvm_didescriptor.Verify();

        return BOOLEAN_TO_EJSVAL(passed);
    }

    void
    DIDescriptor_init (ejsval exports)
    {
        _ejs_DIDescriptor_specops = _ejs_Object_specops;
        _ejs_DIDescriptor_specops.class_name = "LLVMDIDescriptor";
        _ejs_DIDescriptor_specops.Allocate = DIDescriptor_allocate;

        _ejs_gc_add_root (&_ejs_DIDescriptor_prototype);
        _ejs_DIDescriptor_prototype = _ejs_object_create(_ejs_Object_prototype);

        _ejs_DIDescriptor = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMDIDescriptor", (EJSClosureFunc)DIDescriptor_impl, _ejs_DIDescriptor_prototype);

        _ejs_object_setprop_utf8 (exports,              "DIDescriptor", _ejs_DIDescriptor);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_DIDescriptor_prototype, x, DIDescriptor_prototype_##x)

        PROTO_METHOD(verify);

#undef PROTO_METHOD
    }


    // DIType

    static EJSSpecOps _ejs_DIType_specops;
    static ejsval _ejs_DIType_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_DIType EJSVAL_ALIGNMENT;

    void
    DIType_init (ejsval exports)
    {
    }

    // DIScope

    typedef struct {
        /* object header */
        EJSObject obj;

        /* load specific data */
        llvm::DIScope llvm_discope;
    } DIScope;

    static EJSSpecOps _ejs_DIScope_specops;
    static ejsval _ejs_DIScope_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_DIScope EJSVAL_ALIGNMENT;

    llvm::DIScope DIScope_GetLLVMObj(ejsval val) {
        return ((DIScope*)EJSVAL_TO_OBJECT(val))->llvm_discope;
    }

    void
    DIScope_init (ejsval exports)
    {
    }

    // DISubprogram

    typedef struct {
        /* object header */
        EJSObject obj;

        /* load specific data */
        llvm::DISubprogram llvm_disubprogram;
    } DISubprogram;

    static EJSSpecOps _ejs_DISubprogram_specops;
    static ejsval _ejs_DISubprogram_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_DISubprogram EJSVAL_ALIGNMENT;

    static EJSObject* DISubprogram_allocate()
    {
        return (EJSObject*)_ejs_gc_new(DISubprogram);
    }

    ejsval DISubprogram_new(llvm::DISubprogram llvm_disubprogram) {
        ejsval result = _ejs_object_new (_ejs_DISubprogram_prototype, &_ejs_DISubprogram_specops);
        ((DISubprogram*)EJSVAL_TO_OBJECT(result))->llvm_disubprogram = llvm_disubprogram;
        return result;
    }

    llvm::DISubprogram DISubprogram_GetLLVMObj(ejsval val) {
        return ((DISubprogram*)EJSVAL_TO_OBJECT(val))->llvm_disubprogram;
    }

    static EJS_NATIVE_FUNC(DISubprogram_impl) {
        EJS_NOT_IMPLEMENTED();
    }

    static EJS_NATIVE_FUNC(DISubprogram_prototype_verify) {
        EJS_NOT_IMPLEMENTED();
    }

    void
    DISubprogram_init (ejsval exports)
    {
        _ejs_DISubprogram_specops = _ejs_Object_specops;
        _ejs_DISubprogram_specops.class_name = "LLVMDISubprogram";
        _ejs_DISubprogram_specops.Allocate = DISubprogram_allocate;

        _ejs_gc_add_root (&_ejs_DISubprogram_prototype);
        _ejs_DISubprogram_prototype = _ejs_object_create(_ejs_Object_prototype);

        _ejs_DISubprogram = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMDISubprogram", (EJSClosureFunc)DISubprogram_impl, _ejs_DISubprogram_prototype);

        _ejs_object_setprop_utf8 (exports,              "DISubprogram", _ejs_DISubprogram);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_DISubprogram_prototype, x, DISubprogram_prototype_##x)

        PROTO_METHOD(verify);

#undef PROTO_METHOD
    }

    // DIFile

    typedef struct {
        /* object header */
        EJSObject obj;

        /* load specific data */
        llvm::DIFile llvm_difile;
    } DIFile;

    static EJSSpecOps _ejs_DIFile_specops;
    static ejsval _ejs_DIFile_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_DIFile EJSVAL_ALIGNMENT;

    static EJSObject* DIFile_allocate()
    {
        return (EJSObject*)_ejs_gc_new(DIFile);
    }

    ejsval DIFile_new(llvm::DIFile llvm_difile) {
        ejsval result = _ejs_object_new (_ejs_DIFile_prototype, &_ejs_DIFile_specops);
        ((DIFile*)EJSVAL_TO_OBJECT(result))->llvm_difile = llvm_difile;
        return result;
    }

    llvm::DIFile DIFile_GetLLVMObj(ejsval val) {
        return ((DIFile*)EJSVAL_TO_OBJECT(val))->llvm_difile;
    }

    static EJS_NATIVE_FUNC(DIFile_impl) {
        EJS_NOT_IMPLEMENTED();
    }

    static EJS_NATIVE_FUNC(DIFile_prototype_verify) {
        DIFile* dif = (DIFile*)EJSVAL_TO_OBJECT(*_this);

        bool passed = dif->llvm_difile.Verify();

        return BOOLEAN_TO_EJSVAL(passed);
    }

    void
    DIFile_init (ejsval exports)
    {
        _ejs_DIFile_specops = _ejs_Object_specops;
        _ejs_DIFile_specops.class_name = "LLVMDIFile";
        _ejs_DIFile_specops.Allocate = DIFile_allocate;

        _ejs_gc_add_root (&_ejs_DIFile_prototype);
        _ejs_DIFile_prototype = _ejs_object_create(_ejs_Object_prototype);

        _ejs_DIFile = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMDIFile", (EJSClosureFunc)DIFile_impl, _ejs_DIFile_prototype);

        _ejs_object_setprop_utf8 (exports,              "DIFile", _ejs_DIFile);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_DIFile_prototype, x, DIFile_prototype_##x)

        PROTO_METHOD(verify);

#undef PROTO_METHOD
    }

    // DILexicalBlock

    typedef struct {
        /* object header */
        EJSObject obj;

        /* load specific data */
        llvm::DILexicalBlock llvm_dilexicalblock;
    } DILexicalBlock;

    static EJSSpecOps _ejs_DILexicalBlock_specops;
    static ejsval _ejs_DILexicalBlock_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_DILexicalBlock EJSVAL_ALIGNMENT;

    static EJSObject* DILexicalBlock_allocate()
    {
        return (EJSObject*)_ejs_gc_new(DILexicalBlock);
    }

    ejsval DILexicalBlock_new(llvm::DILexicalBlock llvm_dilexicalblock) {
        ejsval result = _ejs_object_new (_ejs_DILexicalBlock_prototype, &_ejs_DILexicalBlock_specops);
        ((DILexicalBlock*)EJSVAL_TO_OBJECT(result))->llvm_dilexicalblock = llvm_dilexicalblock;
        return result;
    }

    llvm::DILexicalBlock DILexicalBlock_GetLLVMObj(ejsval val) {
        return ((DILexicalBlock*)EJSVAL_TO_OBJECT(val))->llvm_dilexicalblock;
    }

    static EJS_NATIVE_FUNC(DILexicalBlock_impl) {
        EJS_NOT_IMPLEMENTED();
    }

    void
    DILexicalBlock_init (ejsval exports)
    {
        _ejs_DILexicalBlock_specops = _ejs_Object_specops;
        _ejs_DILexicalBlock_specops.class_name = "LLVMDILexicalBlock";
        _ejs_DILexicalBlock_specops.Allocate = DILexicalBlock_allocate;

        _ejs_gc_add_root (&_ejs_DILexicalBlock_prototype);
        _ejs_DILexicalBlock_prototype = _ejs_object_create(_ejs_Object_prototype);

        _ejs_DILexicalBlock = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMDILexicalBlock", (EJSClosureFunc)DILexicalBlock_impl, _ejs_DILexicalBlock_prototype);

        _ejs_object_setprop_utf8 (exports,              "DILexicalBlock", _ejs_DILexicalBlock);

#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_DILexicalBlock_prototype, x, DILexicalBlock_prototype_##x)

#undef PROTO_METHOD
    }

    // DebugLoc

    typedef struct {
        /* object header */
        EJSObject obj;

        /* load specific data */
        llvm::DebugLoc llvm_debugloc;
    } DebugLoc;

    static EJSSpecOps _ejs_DebugLoc_specops;
    static ejsval _ejs_DebugLoc_prototype EJSVAL_ALIGNMENT;
    static ejsval _ejs_DebugLoc EJSVAL_ALIGNMENT;

    ejsval DebugLoc_new(llvm::DebugLoc llvm_debugloc) {
        ejsval result = _ejs_object_new (_ejs_DebugLoc_prototype, &_ejs_DebugLoc_specops);
        ((DebugLoc*)EJSVAL_TO_OBJECT(result))->llvm_debugloc = llvm_debugloc;
        return result;
    }

    llvm::DebugLoc DebugLoc_GetLLVMObj(ejsval val) {
        return ((DebugLoc*)EJSVAL_TO_OBJECT(val))->llvm_debugloc;
    }

    static EJSObject* DebugLoc_allocate()
    {
        return (EJSObject*)_ejs_gc_new(DebugLoc);
    }

    static EJS_NATIVE_FUNC(DebugLoc_impl) {
        EJS_NOT_IMPLEMENTED();
    }

    static EJS_NATIVE_FUNC(DebugLoc_get) {
        REQ_INT_ARG(0, line);
        REQ_INT_ARG(1, column);
        REQ_LLVM_DISCOPE_ARG(2, discope);

        return DebugLoc_new(llvm::DebugLoc::get(line, column, discope, NULL));
    }

    void
    DebugLoc_init (ejsval exports)
    {
        _ejs_DebugLoc_specops = _ejs_Object_specops;
        _ejs_DebugLoc_specops.class_name = "LLVMDebugLoc";
        _ejs_DebugLoc_specops.Allocate = DebugLoc_allocate;

        _ejs_gc_add_root (&_ejs_DebugLoc_prototype);
        _ejs_DebugLoc_prototype = _ejs_object_create(_ejs_Object_prototype);

        _ejs_DebugLoc = _ejs_function_new_utf8_with_proto (_ejs_null, "LLVMDebugLoc", (EJSClosureFunc)DebugLoc_impl, _ejs_DebugLoc_prototype);

        _ejs_object_setprop_utf8 (exports,              "DebugLoc", _ejs_DebugLoc);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_DebugLoc, x, DebugLoc_##x)

        OBJ_METHOD(get);
    }


}
#endif
