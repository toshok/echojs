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

// class private-name storage (#fields, #methods).  each
// private name desugars to a weakmap in the class's scope; these are the
// checked accessors the compiler emits.  `name` is a string like "#x",
// used only for error messages.
ejsval _ejs_private_field_get   (ejsval map, ejsval obj, ejsval name);
ejsval _ejs_private_field_set   (ejsval map, ejsval obj, ejsval name, ejsval value);
ejsval _ejs_private_field_init  (ejsval map, ejsval obj, ejsval value);
ejsval _ejs_private_brand_check (ejsval map, ejsval obj, ejsval name); // returns obj
ejsval _ejs_private_has         (ejsval map, ejsval obj);
ejsval _ejs_private_write_error (ejsval name) __attribute__ ((noreturn));

EJS_END_DECLS

#endif
