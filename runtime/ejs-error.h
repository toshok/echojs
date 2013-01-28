/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_error_h_
#define _ejs_error_h_

#include "ejs.h"
#include "ejs-object.h"

EJS_BEGIN_DECLS

extern ejsval _ejs_Error;
extern ejsval _ejs_error_get_prototype();

extern void _ejs_throw_typeerror(const char *message);

extern void _ejs_error_init(ejsval global);

EJS_END_DECLS

#endif
