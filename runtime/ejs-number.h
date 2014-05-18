/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_number_h_
#define _ejs_number_h_

#include "ejs-object.h"

#define EJS_MAX_SAFE_INTEGER 9007199254740991LL
#define EJS_MIN_SAFE_INTEGER -9007199254740991LL

typedef struct {
    /* object header */
    EJSObject obj;

    /* number specific data */
    EJSPrimNumber number;
} EJSNumber;

EJS_BEGIN_DECLS

extern ejsval _ejs_Number;
extern ejsval _ejs_Number_prototype;
extern EJSSpecOps _ejs_Number_specops;

void _ejs_number_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_number_h */
