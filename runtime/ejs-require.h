/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_require_h
#define _ejs_require_h

#include "ejs.h"
#include "ejs-function.h"
#include "ejs-module.h"

EJS_BEGIN_DECLS

extern ejsval _ejs_require;

extern void _ejs_require_init(ejsval global);

void _ejs_module_resolve(EJSModule *mod);
extern ejsval _ejs_module_get(ejsval name);

EJS_END_DECLS

#endif /* _ejs_require_h */
