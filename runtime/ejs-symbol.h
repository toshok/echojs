/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_symbol_h_
#define _ejs_symbol_h_

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"

#define EJSVAL_IS_SYMBOL_OBJ(v) (EJSVAL_IS_OBJECT(v) && EJSVAL_TO_OBJECT(v)->ops == &_ejs_Symbol_specops)
#define EJSVAL_TO_SYMBOL_OBJ(v) ((EJSSymbol*)EJSVAL_TO_OBJECT(v))

// true if an ejsval is either a primitive symbol or a symbol object
#define EJSVAL_IS_SYMBOL_TYPE(v) (EJSVAL_IS_SYMBOL(v) || EJSVAL_IS_SYMBOL_OBJ(v))

typedef struct {
    /* object header */
    EJSObject obj;

    ejsval primSymbol;
} EJSSymbol;

struct _EJSPrimSymbol {
    GCObjectHeader gc_header;

    ejsval description;
    
    // if lsb is 0 we haven't calculated it yet
    uint32_t hashcode;
};

EJS_BEGIN_DECLS

extern ejsval _ejs_Symbol;
extern ejsval _ejs_Symbol_prototype;
extern EJSSpecOps _ejs_Symbol_specops;

// our well known symbols
extern ejsval _ejs_Symbol_hasInstance;
extern ejsval _ejs_Symbol_isConcatSpreadable;
extern ejsval _ejs_Symbol_species;
extern ejsval _ejs_Symbol_iterator;
extern ejsval _ejs_Symbol_toPrimitive;
extern ejsval _ejs_Symbol_toStringTag;
extern ejsval _ejs_Symbol_unscopables;

extern ejsval _ejs_Symbol_match;
extern ejsval _ejs_Symbol_replace;
extern ejsval _ejs_Symbol_split;
extern ejsval _ejs_Symbol_search;

void _ejs_symbol_init(ejsval global);

ejsval _ejs_symbol_new (ejsval description);

uint32_t _ejs_symbol_hash (ejsval symbol);

EJS_END_DECLS

#endif
