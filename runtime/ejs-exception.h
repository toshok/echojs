#ifndef _ejs_exception_h
#define _ejs_exception_h

#include "ejs-object.h"

EJS_BEGIN_DECLS

void _ejs_exception_init (void);
extern void ejs_exception_throw (EJSValue* val);

EJS_END_DECLS

#endif /* _ejs_exception_h */
