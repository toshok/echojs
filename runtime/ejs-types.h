/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejstypes_h_
#define _ejstypes_h_

typedef signed char int8_t;
typedef unsigned char uint8_t;

typedef signed short int16_t;
typedef unsigned short uint16_t;

typedef signed int int32_t;
typedef unsigned int uint32_t;

typedef signed long long int64_t;
typedef unsigned long long uint64_t;

typedef int32_t jsint;
typedef uint32_t jsuint;
typedef double jsdouble;

typedef uint16_t jschar;

typedef uint32_t GCObjectHeader;

#if defined(__GNUC__) && (__GNUC__ > 2)
# define EJS_LIKELY(x)   (__builtin_expect((x), 1))
# define EJS_UNLIKELY(x) (__builtin_expect((x), 0))
#else
# define EJS_LIKELY(x)   (x)
# define EJS_UNLIKELY(x) (x)
#endif

#define EJS_LIST_HEADER(t)                      \
    t* prev;                                    \
    t* next
#define EJS_LIST_INIT(v) EJS_MACRO_START        \
    v->prev = v->next = NULL;                   \
    EJS_MACRO_END

#define EJS_LIST_PREPEND(v,l) EJS_MACRO_START	\
    v->next = l;                                \
    if (l) l->prev = v;                         \
    l = v;                                      \
    EJS_MACRO_END

#define EJS_LIST_APPEND(t,v,l) EJS_MACRO_START      \
    if ((l) == NULL) {                              \
        EJS_LIST_PREPEND(v,l);                      \
    }                                               \
    else {                                          \
        t* end = l;                                 \
        while (end->next != NULL) end = end->next;  \
        end->next = v;                              \
        v->prev = end;                              \
        v->next = NULL;                             \
    }                                               \
    EJS_MACRO_END

#define EJS_LIST_DEATTACH(v,l) EJS_MACRO_START	\
    if (v->next) v->next->prev = v->prev;		\
    if (v->prev) v->prev->next = v->next;		\
    if (l == v) l == v->next;                   \
    EJS_LIST-INIT(v);                           \
    EJS_MACRO_END


#define EJS_SLIST_HEADER(t)                     \
    t* next
#define EJS_SLIST_INIT(v) EJS_MACRO_START       \
    v->next = NULL;                             \
    EJS_MACRO_END

#define EJS_SLIST_ATTACH(v,l) EJS_MACRO_START	\
    v->next = l;                                \
    l = v;                                      \
    EJS_MACRO_END

#define EJS_SLIST_DETACH_HEAD(v,l) EJS_MACRO_START	\
    l = l->next;                                    \
    EJS_MACRO_END

EJS_BEGIN_DECLS

extern jschar* ucs2_strdup (const jschar *str);
extern int32_t ucs2_strcmp (const jschar *s1, const jschar *s2);
extern int32_t ucs2_strlen (const jschar *str);
extern jschar* ucs2_strstr (const jschar *haystack, const jschar *needle);
extern char* ucs2_to_utf8 (const jschar *str);

EJS_END_DECLS

#endif /* _ejs_types_h */
