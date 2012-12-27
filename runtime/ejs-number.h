/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_number_h_
#define _ejs_number_h_

#include "ejs-object.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* number specific data */
    EJSPrimNumber number;
} EJSNumber;

EJS_BEGIN_DECLS

extern ejsval _ejs_Number;
extern ejsval _ejs_Number_proto;
extern EJSSpecOps _ejs_number_specops;

EJSObject* _ejs_number_alloc_instance();

void _ejs_number_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_number_h */
