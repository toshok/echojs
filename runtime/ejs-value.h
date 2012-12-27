/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_value_h
#define _ejs_value_h

#include "ejs.h"

typedef double EJSPrimNumber;

struct _EJSPrimString {
    int len;
    char data[1]; // utf8 \0 terminated
};

#define EJSVAL_IS_PRIMITIVE(v) (EJSVAL_IS_NUMBER(v) || EJSVAL_IS_STRING(v) || EJSVAL_IS_BOOLEAN(v) || EJSVAL_IS_UNDEFINED(v))

#define EJSVAL_TAG(v) (((GCObjectHeader*)v)->tag & 0xffff)
#define EJSVAL_SET_TAG(v,t) ((GCObjectHeader*)v)->tag = (((GCObjectHeader*)v)->tag & ~0xffff) | t

#define EJSVAL_IS_OBJECT(v)    EJSVAL_IS_OBJECT_IMPL(v)
#define EJSVAL_IS_ARRAY(v)     (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->proto.asBits == _ejs_Array_proto.asBits))
#define EJSVAL_IS_FUNCTION(v)  (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->proto.asBits == _ejs_Function_proto.asBits))
#define EJSVAL_IS_NUMBER(v)    EJSVAL_IS_DOUBLE_IMPL(v)
#define EJSVAL_IS_STRING(v)    EJSVAL_IS_STRING_IMPL(v)
#define EJSVAL_IS_BOOLEAN(v)   EJSVAL_IS_BOOLEAN_IMPL(v)
#define EJSVAL_IS_UNDEFINED(v) EJSVAL_IS_UNDEFINED_IMPL(v)
#define EJSVAL_IS_NULL(v)      EJSVAL_IS_NULL_IMPL(v)

#define EJSVAL_TO_OBJECT(v)       EJSVAL_TO_OBJECT_IMPL(v)
#define EJSVAL_TO_STRING(v)       EJSVAL_TO_STRING_IMPL(v)->data
#define EJSVAL_TO_STRLEN(v)       EJSVAL_TO_STRING_IMPL(v)->len
#define EJSVAL_TO_NUMBER(v)       v.asDouble
#define EJSVAL_TO_BOOLEAN(v)      EJSVAL_TO_BOOLEAN_IMPL(v)
#define EJSVAL_TO_FUNC(v)         ((EJSFunction*)EJSVAL_TO_OBJECT_IMPL(v))->func
#define EJSVAL_TO_ENV(v)          ((EJSFunction*)EJSVAL_TO_OBJECT_IMPL(v))->env

#define OBJECT_TO_EJSVAL(v)       OBJECT_TO_EJSVAL_IMPL(v)
#define BOOLEAN_TO_EJSVAL(v)      BOOLEAN_TO_EJSVAL_IMPL(v)
#define NUMBER_TO_EJSVAL(v)       DOUBLE_TO_EJSVAL_IMPL(v)
#define STRING_TO_EJSVAL(v)       STRING_TO_EJSVAL_IMPL(v)

#define EJSVAL_EQ(v1,v2)          ((v1).asBits == (v2).asBits)

#define EJS_NUMBER_FORMAT "%g"

EJS_BEGIN_DECLS

void _ejs_dump_value (ejsval val);

ejsval _ejs_number_new (double value);
ejsval _ejs_string_new_utf8 (const char* str);
ejsval _ejs_string_new_utf8_len (const char* str, int len);

void _ejs_value_finalize(ejsval val);

typedef void (*EJSValueFunc)(ejsval value);

EJS_END_DECLS

#endif /* _ejs_value_h */
