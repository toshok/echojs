/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_symbol_h_
#define _ejs_symbol_h_

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* when symbols are created, do we want to store a name here? */
    // ejsval name;
} EJSSymbol;

EJS_BEGIN_DECLS

extern EJSSpecOps _ejs_Symbol_specops;

void _ejs_symbol_init(ejsval global);

ejsval _ejs_symbol_new ();

EJS_END_DECLS

#endif
