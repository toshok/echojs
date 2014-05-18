/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_xhr_h_
#define _ejs_xhr_h_

#include "ejs-object.h"

EJS_BEGIN_DECLS

extern ejsval _ejs_XMLHttpRequest;
extern ejsval _ejs_XMLHttpRequest_prototype;
extern EJSSpecOps _ejs_XMLHttpRequest_specops;

void _ejs_xmlhttprequest_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_xhr_h_ */
