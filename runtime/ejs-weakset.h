/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_weakset_h_
#define _ejs_weakset_h_

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"

typedef EJSObject EJSWeakSet;

EJS_BEGIN_DECLS

extern ejsval _ejs_WeakSet;
extern ejsval _ejs_WeakSet_prototype;
extern EJSSpecOps _ejs_WeakSet_specops;

void _ejs_weakset_init(ejsval global);

ejsval _ejs_weakset_new();

EJS_END_DECLS

#endif
