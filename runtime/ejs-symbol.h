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

    ejsval description;

    // if lsb is 0 we haven't calculated it yet
    uint32_t hashcode;
} EJSSymbol;

EJS_BEGIN_DECLS

extern ejsval _ejs_Symbol;
extern ejsval _ejs_Symbol_prototype;
extern EJSSpecOps _ejs_Symbol_specops;

// our well known symbols
extern ejsval _ejs_Symbol_create;
extern ejsval _ejs_Symbol_hasInstance;
extern ejsval _ejs_Symbol_isConcatSpreadable;
extern ejsval _ejs_Symbol_isRegExp;
extern ejsval _ejs_Symbol_iterator;
extern ejsval _ejs_Symbol_toPrimitive;
extern ejsval _ejs_Symbol_toStringTag;
extern ejsval _ejs_Symbol_unscopables;

void _ejs_symbol_init(ejsval global);

ejsval _ejs_symbol_new (ejsval description);

uint32_t _ejs_symbol_hash (ejsval symbol);

EJS_END_DECLS

#endif
