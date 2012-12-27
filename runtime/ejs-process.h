/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_process_h_
#define _ejs_process_h_

#include "ejs.h"
#include "ejs-value.h"

EJS_BEGIN_DECLS

void _ejs_process_init(ejsval global, int argc, char** argv);

EJS_END_DECLS

#endif
