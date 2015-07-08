/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_process_h_
#define _ejs_process_h_

#include "ejs.h"
#include "ejs-value.h"

EJS_BEGIN_DECLS

extern ejsval _ejs_Process;

void _ejs_process_init(ejsval global, uint32_t argc, char **argv);

EJS_END_DECLS

#endif
