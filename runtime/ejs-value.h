/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_value_h
#define _ejs_value_h

#include "ejs.h"
#include "ejs-gc.h"

typedef double EJSPrimNumber;

/* primitive strings can exist in a few different forms

   1: a statically allocated C string that isn't freed when the primitive string is destroyed.
      we use these for atoms - known strings in both the runtime and compiler.

   2: strings that appear in JS source as string literals.  these are gc allocated, and the character
      data is freed when the strnig is destroyed.

   3: ropes built up by concatenating strings together.

   4: dependent strings made by taking substrings/slices of other strings when the resulting string is "large"
*/

typedef enum {
    EJS_STRING_FLAT      = 1,
    EJS_STRING_ROPE      = 2,
    EJS_STRING_DEPENDENT = 3
} EJSPrimStringType;

#define EJS_PRIMSTR_TYPE_MASK 0x03

#define EJS_PRIMSTR_TYPE_MASK_SHIFTED (EJS_PRIMSTR_TYPE_MASK << EJS_GC_USER_FLAGS_SHIFT)

#define EJS_PRIMSTR_SET_TYPE(s,t) (((EJSObject*)(s))->gc_header |= (t) << EJS_GC_USER_FLAGS_SHIFT)
#define EJS_PRIMSTR_CLEAR_TYPE(s) (((EJSObject*)(s))->gc_header &= ~EJS_PRIMSTR_TYPE_MASK_SHIFTED)

#define EJS_PRIMSTR_GET_TYPE(s)   ((EJSPrimStringType)((((EJSPrimString*)(s))->gc_header & EJS_PRIMSTR_TYPE_MASK_SHIFTED) >> EJS_GC_USER_FLAGS_SHIFT))

struct _EJSPrimString {
    GCObjectHeader gc_header;
    uint32_t length;
    union {
        // utf8 \0 terminated
        //    for normal strings, this points to the memory location just beyond this struct - i.e. (char*)primStringPointer + sizeof(_EJSPrimString)
        //    for atoms, this points to the statically compiled C string constant.
        char *flat;
        struct {
            struct _EJSPrimString *left;
            struct _EJSPrimString *right;
        } rope;
    } data;
};

#define EJSVAL_IS_PRIMITIVE(v) (EJSVAL_IS_NUMBER(v) || EJSVAL_IS_STRING(v) || EJSVAL_IS_BOOLEAN(v) || EJSVAL_IS_UNDEFINED(v))

#define EJSVAL_IS_OBJECT(v)    EJSVAL_IS_OBJECT_IMPL(v)
#define EJSVAL_IS_ARRAY(v)     (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_array_specops || EJSVAL_TO_OBJECT(v)->ops == &_ejs_sparsearray_specops))
#define EJSVAL_IS_FUNCTION(v)  (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_function_specops))
#define EJSVAL_IS_DATE(v)      (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_date_specops))
#define EJSVAL_IS_NUMBER_OBJECT(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_number_specops))
#define EJSVAL_IS_STRING_OBJECT(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_string_specops))
#define EJSVAL_IS_BOOLEAN_OBJECT(v) (EJSVAL_IS_OBJECT(v) && (EJSVAL_TO_OBJECT(v)->ops == &_ejs_boolean_specops))
#define EJSVAL_IS_NUMBER(v)    EJSVAL_IS_DOUBLE_IMPL(v)
#define EJSVAL_IS_STRING(v)    EJSVAL_IS_STRING_IMPL(v)
#define EJSVAL_IS_BOOLEAN(v)   EJSVAL_IS_BOOLEAN_IMPL(v)
#define EJSVAL_IS_UNDEFINED(v) EJSVAL_IS_UNDEFINED_IMPL(v)
#define EJSVAL_IS_NULL(v)      EJSVAL_IS_NULL_IMPL(v)
#define EJSVAL_IS_OBJECT_OR_NULL(v) EJSVAL_IS_OBJECT_OR_NULL_IMPL(v)

#define EJSVAL_TO_OBJECT(v)       EJSVAL_TO_OBJECT_IMPL(v)
#define EJSVAL_TO_FLAT_STRING(v)  _ejs_string_flatten(v)->data.flat
#define EJSVAL_TO_STRING(v)       EJSVAL_TO_STRING_IMPL(v)
#define EJSVAL_TO_STRLEN(v)       EJSVAL_TO_STRING_IMPL(v)->length
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
ejsval _ejs_string_concat (ejsval left, ejsval right);
ejsval _ejs_string_concatv (ejsval first, ...);
EJSPrimString* _ejs_string_flatten (ejsval str);
EJSPrimString* _ejs_primstring_flatten (EJSPrimString* primstr);

void _ejs_value_finalize(ejsval val);

typedef void (*EJSValueFunc)(ejsval value);

EJS_END_DECLS

#endif /* _ejs_value_h */
