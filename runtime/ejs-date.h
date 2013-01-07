/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_date_h_
#define _ejs_date_h_

#include <time.h>
#include "ejs-object.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* date specific data */
    struct tm tm;
} EJSDate;


EJS_BEGIN_DECLS

extern ejsval _ejs_Date;
extern ejsval _ejs_Date_proto;
extern EJSSpecOps _ejs_date_specops;

ejsval _ejs_date_new_unix (int timestamp);

void _ejs_date_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_date_h */
