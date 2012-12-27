/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_exception_h
#define _ejs_exception_h

#include "ejs-object.h"

EJS_BEGIN_DECLS

void _ejs_exception_init (void);
extern void ejs_exception_throw (ejsval val);

EJS_END_DECLS

#endif /* _ejs_exception_h */
