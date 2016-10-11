#ifndef EJS_LLVM_DIBUILDER_H
#define EJS_LLVM_DIBUILDER_H

#include "ejs-llvm.h"

namespace ejsllvm {
  void DIBuilder_init (ejsval exports);
  ejsval DIBuilder_new(llvm::DIBuilder* llvm_dibuilder);
  llvm::DIBuilder* DIBuilder_GetLLVMObj(ejsval val);

  void DIFile_init (ejsval exports);
  ejsval DIFile_new(llvm::DIFile* llvm_difile);
  llvm::DIFile* DIFile_GetLLVMObj(ejsval val);

  void DICompileUnit_init (ejsval exports);
  ejsval DICompileUnit_new(llvm::DICompileUnit* llvm_dicompile_unit);
  llvm::DICompileUnit* DICompileUnit_GetLLVMObj(ejsval val);

  void DIScope_init (ejsval exports);
  ejsval DIScope_new(llvm::DIScope* llvm_scope);
  llvm::DIScope* DIScope_GetLLVMObj(ejsval val);

  void DISubprogram_init (ejsval exports);
  ejsval DISubprogram_new(llvm::DISubprogram* llvm_disubprogram);
  llvm::DISubprogram* DISubprogram_GetLLVMObj(ejsval val);

  void DILexicalBlock_init (ejsval exports);
  ejsval DILexicalBlock_new(llvm::DILexicalBlock* llvm_dilexicalblock);
  llvm::DILexicalBlock* DILexicalBlock_GetLLVMObj(ejsval val);

  void DIType_init (ejsval exports);

  void DebugLoc_init (ejsval exports);
  ejsval DebugLoc_new(llvm::DebugLoc llvm_debugloc);
  llvm::DebugLoc DebugLoc_GetLLVMObj(ejsval val);
}

#endif /* EJS_LLVM_DIBUILDER_H */
