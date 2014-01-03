/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_console_h_
#define _ejs_console_h_

#include "ejs.h"
#include "ejs-value.h"

EJS_BEGIN_DECLS

ejsval _ejs_console;

void _ejs_console_init(ejsval global);

EJS_END_DECLS

#endif
