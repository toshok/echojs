
#ifndef _ejs_require_h
#define _ejs_require_h

#include "ejs.h"
#include "ejs-function.h"

typedef struct {
  const char* name;
  EJSClosureFunc func;
  EJSValue *cached_exports;
} EJSRequire;

typedef EJSValue* (*ExternalModuleEntry) (EJSValue *exports);

typedef struct {
  const char* name;
  ExternalModuleEntry func;
  EJSValue *cached_exports;
} EJSExternalModuleRequire;

EJS_BEGIN_DECLS

extern EJSValue* _ejs_require;

extern void _ejs_require_init(EJSValue* global);

EJS_END_DECLS

#endif /* _ejs_require_h */
