/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_promise_h_
#define _ejs_promise_h_

#include <time.h>
#include <sys/time.h>
#include "ejs-object.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* promise specific data */
    struct timeval tv;
    struct timezone tz;
} EJSPromise;


EJS_BEGIN_DECLS

extern ejsval _ejs_Promise;
extern ejsval _ejs_Promise_prototype;
extern EJSSpecOps _ejs_Promise_specops;

void _ejs_promise_init(ejsval global);

EJS_END_DECLS

#endif /* _ejs_promise_h */
