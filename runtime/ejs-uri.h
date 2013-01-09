/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_uri_h
#define _ejs_uri_h

#include "ejs.h"
#include "ejs-object.h"

EJS_BEGIN_DECLS

ejsval _ejs_decodeURI (ejsval env, ejsval _this, uint32_t argc, ejsval* args);
ejsval _ejs_decodeURIComponent (ejsval env, ejsval _this, uint32_t argc, ejsval* args);

ejsval _ejs_encodeURI (ejsval env, ejsval _this, uint32_t argc, ejsval* args);
ejsval _ejs_encodeURIComponent (ejsval env, ejsval _this, uint32_t argc, ejsval* args);

EJS_END_DECLS

#endif
