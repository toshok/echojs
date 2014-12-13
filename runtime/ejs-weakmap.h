/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_weakmap_h_
#define _ejs_weakmap_h_

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"

EJS_BEGIN_DECLS

#if WEAK_COLLECTIONS_USE_INVERTED_REP
typedef EJSObject EJSWeakMap;
#else
#endif

extern ejsval _ejs_WeakMap;
extern ejsval _ejs_WeakMap_prototype;
extern EJSSpecOps _ejs_WeakMap_specops;

void _ejs_weakmap_init(ejsval global);

ejsval _ejs_weakmap_new ();

EJS_END_DECLS

#endif
