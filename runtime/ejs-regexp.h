/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_regexp_h_
#define _ejs_regexp_h_

#include "ejs-object.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* regexp specific data */
    ejsval pattern;
    ejsval flags;

    EJSBool global;
    EJSBool ignoreCase;
    EJSBool multiline;

    EJSBool sticky;
    EJSBool unicode;

    int lastIndex;

    void* compiled_pattern;
} EJSRegExp;

EJS_BEGIN_DECLS

extern ejsval _ejs_RegExp;
extern ejsval _ejs_RegExp_prototype;
extern EJSSpecOps _ejs_RegExp_specops;


void _ejs_regexp_init(ejsval global);

ejsval _ejs_regexp_new (ejsval pattern, ejsval flags);

ejsval _ejs_regexp_new_utf8(const char *pattern, const char *flags);

ejsval _ejs_regexp_replace(ejsval str, ejsval search, ejsval replace);

/* we expose this publicly because String.prototype.match needs to call it directly */
ejsval _ejs_RegExp_prototype_exec_closure;

extern EJSBool IsRegExp(ejsval argument);

EJS_END_DECLS

#endif /* _ejs_regexp_h */
