/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_map_h_
#define _ejs_map_h_

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"

typedef struct {
    /* object header */
    EJSObject obj;

    // XXX more stuff here
} EJSMap;

EJS_BEGIN_DECLS

extern ejsval _ejs_Map;
extern ejsval _ejs_Map_prototype;
extern EJSSpecOps _ejs_Map_specops;

void _ejs_map_init(ejsval global);

void _ejs_map_new ();

EJS_END_DECLS

#endif
