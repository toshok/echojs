/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */
#ifndef _ejs_string_h_
#define _ejs_string_h_

#include "ejs-object.h"

typedef struct {
    /* object header */
    EJSObject obj;

    /* string specific data */
    ejsval primStr;
} EJSString;

EJS_BEGIN_DECLS

extern ejsval _ejs_String;
extern ejsval _ejs_String__proto__;
extern ejsval _ejs_String_prototype;
extern EJSSpecOps _ejs_string_specops;

void _ejs_string_init(ejsval global);



/* primitive strings can exist in a few different forms

   1: a statically allocated C string that isn't freed when the primitive string is destroyed.
      we use these for atoms - known strings in both the runtime and compiler.

   2: strings that appear in JS source as string literals.  these are gc allocated, and the character
      data is freed when the strnig is destroyed.

   3: ropes built up by concatenating strings together.

   4: dependent strings made by taking substrings/slices of other strings when the resulting string is "large"
*/
#define EJSVAL_TO_FLAT_STRING(v)  _ejs_string_flatten(v)->data.flat
#define EJSVAL_TO_STRING(v)       EJSVAL_TO_STRING_IMPL(v)
#define EJSVAL_TO_STRLEN(v)       EJSVAL_TO_STRING_IMPL(v)->length

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
        jschar *flat;
        struct {
            struct _EJSPrimString *left;
            struct _EJSPrimString *right;
        } rope;
    } data;
};


ejsval _ejs_string_new_utf8 (const char* str);
ejsval _ejs_string_new_utf8_len (const char* str, int len);
ejsval _ejs_string_new_ucs2 (const jschar* str);
ejsval _ejs_string_new_ucs2_len (const jschar* str, int len);

ejsval _ejs_string_concat (ejsval left, ejsval right);
ejsval _ejs_string_concatv (ejsval first, ...);
EJSPrimString* _ejs_string_flatten (ejsval str);
EJSPrimString* _ejs_primstring_flatten (EJSPrimString* primstr);

char* _ejs_string_to_utf8(EJSPrimString* primstr);

EJS_END_DECLS

#endif /* _ejs_string_h */
