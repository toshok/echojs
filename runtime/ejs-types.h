/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejstypes_h_
#define _ejstypes_h_

typedef unsigned char uint8_t;

typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

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

#define EJS_LIST_ATTACH(v,l) EJS_MACRO_START	\
    v->next = l;                                \
    if (l) l->prev = v;                         \
    l = v;                                      \
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

#endif /* _ejs_types_h */
