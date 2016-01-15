/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_stream_h_
#define _ejs_stream_h_

#include "ejs.h"
#include "ejs-value.h"

EJS_BEGIN_DECLS

extern ejsval _ejs_stream_wrapFd (int fd, EJSBool throwOnEnd);
extern ejsval _ejs_stream_wrapSysOutput(int fd);
extern void _ejs_stream_init(ejsval global);

EJS_END_DECLS

#endif
