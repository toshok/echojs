
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

#endif /* _ejs_types_h */
