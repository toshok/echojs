
#ifndef _ejs_require_h
#define _ejs_require_h

#include "ejs.h"
#include "ejs-function.h"

typedef struct {
  const char* name;
  EJSClosureFunc func;
  EJSValue *cached_exports;
} EJSRequire;

extern EJSValue* _ejs_require;

extern void _ejs_require_init(EJSValue* global);

#endif /* _ejs_require_h */
