/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * June 30, 2010
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   Luke Wagner <lw@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef ejsvalimpl_h__
#define ejsvalimpl_h__
/*
 * Implementation details for js::Value in jsapi.h.
 */

EJS_BEGIN_DECLS

// notyet for these
#if defined(__GNUC__) || defined(__xlc__) || defined(__xlC__)
#define EJS_ALWAYS_INLINE //__attribute__((always_inline))
#else
#define EJS_ALWAYS_INLINE
#endif

#define EJS_STATIC_ASSERT(x)

#define EJS_BITS_PER_WORD 64
#include <assert.h>
#define EJS_ASSERT assert

/******************************************************************************/

/* To avoid a circular dependency, pull in the necessary pieces of jsnum.h. */

#define EJSDOUBLE_SIGNBIT (((uint64_t) 1) << 63)
#define EJSDOUBLE_EXPMASK (((uint64_t) 0x7ff) << 52)
#define EJSDOUBLE_MANTMASK ((((uint64_t) 1) << 52) - 1)
#define EJSDOUBLE_HI32_SIGNBIT   0x80000000

static EJS_ALWAYS_INLINE EJSBool
EJSDOUBLE_IS_NEGZERO(double d)
{
    union {
        struct {
#if defined(IS_LITTLE_ENDIAN) && !defined(FPU_IS_ARM_FPA)
            uint32_t lo, hi;
#else
            uint32_t hi, lo;
#endif
        } s;
        double d;
    } x;
    if (d != 0)
        return EJS_FALSE;
    x.d = d;
    return (x.s.hi & EJSDOUBLE_HI32_SIGNBIT) != 0;
}

static EJS_ALWAYS_INLINE EJSBool
EJSDOUBLE_IS_INT32(double d, int32_t* pi)
{
    if (EJSDOUBLE_IS_NEGZERO(d))
        return EJS_FALSE;
    return d == (*pi = (int32_t)d);
}

/******************************************************************************/

/*
 * Try to get ejsvals 64-bit aligned. We could almost assert that all values are
 * aligned, but MSVC and GCC occasionally break alignment.
 */
#if defined(__GNUC__) || defined(__xlc__) || defined(__xlC__)
# define EJSVAL_ALIGNMENT        __attribute__((aligned (8)))
#elif defined(_MSC_VER)
  /*
   * Structs can be aligned with MSVC, but not if they are used as parameters,
   * so we just don't try to align.
   */
# define EJSVAL_ALIGNMENT
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
# define EJSVAL_ALIGNMENT
#elif defined(__HP_cc) || defined(__HP_aCC)
# define EJSVAL_ALIGNMENT
#endif

#if EJS_BITS_PER_WORD == 64
# define EJSVAL_TAG_SHIFT 47
#endif

/*
 * We try to use enums so that printing a ejsval_layout in the debugger shows
 * nice symbolic type tags, however we can only do this when we can force the
 * underlying type of the enum to be the desired size.
 */
#if defined(__cplusplus) && !defined(__SUNPRO_CC) && !defined(__xlC__)

#if defined(_MSC_VER)
# define EJS_ENUM_HEADER(id, type)              enum id : type
# define EJS_ENUM_MEMBER(id, type, value)       id = (type)value,
# define EJS_LAST_ENUM_MEMBER(id, type, value)  id = (type)value
# define EJS_ENUM_FOOTER(id)
#else
# define EJS_ENUM_HEADER(id, type)              enum id
# define EJS_ENUM_MEMBER(id, type, value)       id = (type)value,
# define EJS_LAST_ENUM_MEMBER(id, type, value)  id = (type)value
# define EJS_ENUM_FOOTER(id)                    __attribute__((packed))
#endif

/* Remember to propagate changes to the C defines below. */
EJS_ENUM_HEADER(EJSValueType, uint8_t)
{
    EJSVAL_TYPE_DOUBLE              = 0x00,
    EJSVAL_TYPE_INT32               = 0x01,
    EJSVAL_TYPE_UNDEFINED           = 0x02,
    EJSVAL_TYPE_BOOLEAN             = 0x03,
    EJSVAL_TYPE_MAGIC               = 0x04,
    EJSVAL_TYPE_STRING              = 0x05,
    EJSVAL_TYPE_NULL                = 0x06,
    EJSVAL_TYPE_OBJECT              = 0x07,

    /* These never appear in a ejsval; they are only provided as an out-of-band value. */
    EJSVAL_TYPE_UNKNOWN             = 0x20,
    EJSVAL_TYPE_MISSING             = 0x21
} EJS_ENUM_FOOTER(EJSValueType);

EJS_STATIC_ASSERT(sizeof(EJSValueType) == 1);

#if EJS_BITS_PER_WORD == 32

/* Remember to propagate changes to the C defines below. */
EJS_ENUM_HEADER(EJSValueTag, uint32_t)
{
    EJSVAL_TAG_CLEAR                = 0xFFFFFF80,
    EJSVAL_TAG_INT32                = EJSVAL_TAG_CLEAR | EJSVAL_TYPE_INT32,
    EJSVAL_TAG_UNDEFINED            = EJSVAL_TAG_CLEAR | EJSVAL_TYPE_UNDEFINED,
    EJSVAL_TAG_STRING               = EJSVAL_TAG_CLEAR | EJSVAL_TYPE_STRING,
    EJSVAL_TAG_BOOLEAN              = EJSVAL_TAG_CLEAR | EJSVAL_TYPE_BOOLEAN,
    EJSVAL_TAG_MAGIC                = EJSVAL_TAG_CLEAR | EJSVAL_TYPE_MAGIC,
    EJSVAL_TAG_NULL                 = EJSVAL_TAG_CLEAR | EJSVAL_TYPE_NULL,
    EJSVAL_TAG_OBJECT               = EJSVAL_TAG_CLEAR | EJSVAL_TYPE_OBJECT
} EJS_ENUM_FOOTER(EJSValueTag);

EJS_STATIC_ASSERT(sizeof(EJSValueTag) == 4);

#elif EJS_BITS_PER_WORD == 64

/* Remember to propagate changes to the C defines below. */
EJS_ENUM_HEADER(EJSValueTag, uint32_t)
{
    EJSVAL_TAG_MAX_DOUBLE           = 0x1FFF0,
    EJSVAL_TAG_INT32                = EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_INT32,
    EJSVAL_TAG_UNDEFINED            = EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_UNDEFINED,
    EJSVAL_TAG_STRING               = EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_STRING,
    EJSVAL_TAG_BOOLEAN              = EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_BOOLEAN,
    EJSVAL_TAG_MAGIC                = EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_MAGIC,
    EJSVAL_TAG_NULL                 = EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_NULL,
    EJSVAL_TAG_OBJECT               = EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_OBJECT
} EJS_ENUM_FOOTER(EJSValueTag);

EJS_STATIC_ASSERT(sizeof(EJSValueTag) == sizeof(uint32_t));

EJS_ENUM_HEADER(EJSValueShiftedTag, uint64_t)
{
    EJSVAL_SHIFTED_TAG_MAX_DOUBLE   = ((((uint64_t)EJSVAL_TAG_MAX_DOUBLE) << EJSVAL_TAG_SHIFT) | 0xFFFFFFFF),
    EJSVAL_SHIFTED_TAG_INT32        = (((uint64_t)EJSVAL_TAG_INT32)      << EJSVAL_TAG_SHIFT),
    EJSVAL_SHIFTED_TAG_UNDEFINED    = (((uint64_t)EJSVAL_TAG_UNDEFINED)  << EJSVAL_TAG_SHIFT),
    EJSVAL_SHIFTED_TAG_STRING       = (((uint64_t)EJSVAL_TAG_STRING)     << EJSVAL_TAG_SHIFT),
    EJSVAL_SHIFTED_TAG_BOOLEAN      = (((uint64_t)EJSVAL_TAG_BOOLEAN)    << EJSVAL_TAG_SHIFT),
    EJSVAL_SHIFTED_TAG_MAGIC        = (((uint64_t)EJSVAL_TAG_MAGIC)      << EJSVAL_TAG_SHIFT),
    EJSVAL_SHIFTED_TAG_NULL         = (((uint64_t)EJSVAL_TAG_NULL)       << EJSVAL_TAG_SHIFT),
    EJSVAL_SHIFTED_TAG_OBJECT       = (((uint64_t)EJSVAL_TAG_OBJECT)     << EJSVAL_TAG_SHIFT)
} EJS_ENUM_FOOTER(EJSValueShiftedTag);

EJS_STATIC_ASSERT(sizeof(EJSValueShiftedTag) == sizeof(uint64_t));

#endif

#else  /* defined(__cplusplus) */

typedef uint8_t EJSValueType;
#define EJSVAL_TYPE_DOUBLE            ((uint8_t)0x00)
#define EJSVAL_TYPE_INT32             ((uint8_t)0x01)
#define EJSVAL_TYPE_UNDEFINED         ((uint8_t)0x02)
#define EJSVAL_TYPE_BOOLEAN           ((uint8_t)0x03)
#define EJSVAL_TYPE_MAGIC             ((uint8_t)0x04)
#define EJSVAL_TYPE_STRING            ((uint8_t)0x05)
#define EJSVAL_TYPE_NULL              ((uint8_t)0x06)
#define EJSVAL_TYPE_OBJECT            ((uint8_t)0x07)
#define EJSVAL_TYPE_UNKNOWN           ((uint8_t)0x20)

#if EJS_BITS_PER_WORD == 32

typedef uint32_t EJSValueTag;
#define EJSVAL_TAG_CLEAR              ((uint32_t)(0xFFFFFF80))
#define EJSVAL_TAG_INT32              ((uint32_t)(EJSVAL_TAG_CLEAR | EJSVAL_TYPE_INT32))
#define EJSVAL_TAG_UNDEFINED          ((uint32_t)(EJSVAL_TAG_CLEAR | EJSVAL_TYPE_UNDEFINED))
#define EJSVAL_TAG_STRING             ((uint32_t)(EJSVAL_TAG_CLEAR | EJSVAL_TYPE_STRING))
#define EJSVAL_TAG_BOOLEAN            ((uint32_t)(EJSVAL_TAG_CLEAR | EJSVAL_TYPE_BOOLEAN))
#define EJSVAL_TAG_MAGIC              ((uint32_t)(EJSVAL_TAG_CLEAR | EJSVAL_TYPE_MAGIC))
#define EJSVAL_TAG_NULL               ((uint32_t)(EJSVAL_TAG_CLEAR | EJSVAL_TYPE_NULL))
#define EJSVAL_TAG_OBJECT             ((uint32_t)(EJSVAL_TAG_CLEAR | EJSVAL_TYPE_OBJECT))

#elif EJS_BITS_PER_WORD == 64

typedef uint32_t EJSValueTag;
#define EJSVAL_TAG_MAX_DOUBLE         ((uint32_t)(0x1FFF0))
#define EJSVAL_TAG_INT32              (uint32_t)(EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_INT32)
#define EJSVAL_TAG_UNDEFINED          (uint32_t)(EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_UNDEFINED)
#define EJSVAL_TAG_STRING             (uint32_t)(EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_STRING)
#define EJSVAL_TAG_BOOLEAN            (uint32_t)(EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_BOOLEAN)
#define EJSVAL_TAG_MAGIC              (uint32_t)(EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_MAGIC)
#define EJSVAL_TAG_NULL               (uint32_t)(EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_NULL)
#define EJSVAL_TAG_OBJECT             (uint32_t)(EJSVAL_TAG_MAX_DOUBLE | EJSVAL_TYPE_OBJECT)

typedef uint64_t EJSValueShiftedTag;
#define EJSVAL_SHIFTED_TAG_MAX_DOUBLE ((((uint64_t)EJSVAL_TAG_MAX_DOUBLE) << EJSVAL_TAG_SHIFT) | 0xFFFFFFFF)
#define EJSVAL_SHIFTED_TAG_INT32      (((uint64_t)EJSVAL_TAG_INT32)      << EJSVAL_TAG_SHIFT)
#define EJSVAL_SHIFTED_TAG_UNDEFINED  (((uint64_t)EJSVAL_TAG_UNDEFINED)  << EJSVAL_TAG_SHIFT)
#define EJSVAL_SHIFTED_TAG_STRING     (((uint64_t)EJSVAL_TAG_STRING)     << EJSVAL_TAG_SHIFT)
#define EJSVAL_SHIFTED_TAG_BOOLEAN    (((uint64_t)EJSVAL_TAG_BOOLEAN)    << EJSVAL_TAG_SHIFT)
#define EJSVAL_SHIFTED_TAG_MAGIC      (((uint64_t)EJSVAL_TAG_MAGIC)      << EJSVAL_TAG_SHIFT)
#define EJSVAL_SHIFTED_TAG_NULL       (((uint64_t)EJSVAL_TAG_NULL)       << EJSVAL_TAG_SHIFT)
#define EJSVAL_SHIFTED_TAG_OBJECT     (((uint64_t)EJSVAL_TAG_OBJECT)     << EJSVAL_TAG_SHIFT)

#endif  /* EJS_BITS_PER_WORD */
#endif  /* defined(__cplusplus) && !defined(__SUNPRO_CC) */

#define EJSVAL_LOWER_INCL_TYPE_OF_OBJ_OR_NULL_SET        EJSVAL_TYPE_NULL
#define EJSVAL_UPPER_EXCL_TYPE_OF_PRIMITIVE_SET          EJSVAL_TYPE_OBJECT
#define EJSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET             EJSVAL_TYPE_INT32
#define EJSVAL_LOWER_INCL_TYPE_OF_PTR_PAYLOAD_SET        EJSVAL_TYPE_MAGIC

#if EJS_BITS_PER_WORD == 32

#define EJSVAL_TYPE_TO_TAG(type)      ((EJSValueTag)(EJSVAL_TAG_CLEAR | (type)))

#define EJSVAL_LOWER_INCL_TAG_OF_OBJ_OR_NULL_SET         EJSVAL_TAG_NULL
#define EJSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET           EJSVAL_TAG_OBJECT
#define EJSVAL_UPPER_INCL_TAG_OF_NUMBER_SET              EJSVAL_TAG_INT32
#define EJSVAL_LOWER_INCL_TAG_OF_GCTHING_SET             EJSVAL_TAG_STRING

#elif EJS_BITS_PER_WORD == 64

#define EJSVAL_PAYLOAD_MASK           0x00007FFFFFFFFFFFLL
#define EJSVAL_TAG_MASK               0xFFFF800000000000LL
#define EJSVAL_TYPE_TO_TAG(type)      ((EJSValueTag)(EJSVAL_TAG_MAX_DOUBLE | (type)))
#define EJSVAL_TYPE_TO_SHIFTED_TAG(type) (((uint64_t)EJSVAL_TYPE_TO_TAG(type)) << EJSVAL_TAG_SHIFT)

#define EJSVAL_LOWER_INCL_SHIFTED_TAG_OF_OBJ_OR_NULL_SET  EJSVAL_SHIFTED_TAG_NULL
#define EJSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET    EJSVAL_SHIFTED_TAG_OBJECT
#define EJSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET       EJSVAL_SHIFTED_TAG_UNDEFINED
#define EJSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET      EJSVAL_SHIFTED_TAG_STRING

#endif /* EJS_BITS_PER_WORD */

typedef enum EJSWhyMagic
{
    EJS_ARRAY_HOLE,               /* a hole in a dense array */
    EJS_ARGS_HOLE,                /* a hole in the args object's array */
    EJS_NATIVE_ENUMERATE,         /* indicates that a custom enumerate hook forwarded
                                  * to EJS_EnumerateState, which really means the object can be
                                  * enumerated like a native object. */
    EJS_NO_ITER_VALUE,            /* there is not a pending iterator value */
    EJS_GENERATOR_CLOSING,        /* exception value thrown when closing a generator */
    EJS_NO_CONSTANT,              /* compiler sentinel value */
    EJS_THIS_POISON,              /* used in debug builds to catch tracing errors */
    EJS_ARG_POISON,               /* used in debug builds to catch tracing errors */
    EJS_SERIALIZE_NO_NODE,        /* an empty subnode in the AST serializer */
    EJS_LAZY_ARGUMENTS,           /* lazy arguments value on the stack */
    EJS_IS_CONSTRUCTING,          /* magic value passed to natives to indicate construction */
    EJS_GENERIC_MAGIC             /* for local use */
} EJSWhyMagic;

#if defined(IS_LITTLE_ENDIAN)
# if EJS_BITS_PER_WORD == 32
typedef union ejsval_layout
{
    uint64_t asBits;
    struct {
        union {
            int32_t        i32;
            uint32_t       u32;
            EJSBool         boo;
            EJSPrimString       *str;
            EJSObject       *obj;
            void           *ptr;
            EJSWhyMagic     why;
            size_t         word;
        } payload;
        EJSValueTag tag;
    } s;
    double asDouble;
    void *asPtr;
} EJSVAL_ALIGNMENT ejsval_layout;
# elif EJS_BITS_PER_WORD == 64
typedef union ejsval_layout
{
    uint64_t asBits;
#if (!defined(_WIN64) && defined(__cplusplus))
    /* MSVC does not pack these correctly :-( */
    struct {
        uint64_t           payload47 : 47;
        EJSValueTag         tag : 17;
    } debugView;
#endif
    struct {
        union {
            int32_t        i32;
            uint32_t       u32;
            EJSWhyMagic     why;
        } payload;
    } s;
    double asDouble;
    void *asPtr;
    size_t asWord;
} EJSVAL_ALIGNMENT ejsval_layout;
# endif  /* EJS_BITS_PER_WORD */
#else   /* defined(IS_LITTLE_ENDIAN) */
# if EJS_BITS_PER_WORD == 32
typedef union ejsval_layout
{
    uint64_t asBits;
    struct {
        EJSValueTag tag;
        union {
            int32_t        i32;
            uint32_t       u32;
            EJSBool         boo;
            EJSPrimString       *str;
            EJSObject       *obj;
            void           *ptr;
            EJSWhyMagic     why;
            size_t         word;
        } payload;
    } s;
    double asDouble;
    void *asPtr;
} EJSVAL_ALIGNMENT ejsval_layout;
# elif EJS_BITS_PER_WORD == 64
typedef union ejsval_layout
{
    uint64_t asBits;
    struct {
        EJSValueTag         tag : 17;
        uint64_t           payload47 : 47;
    } debugView;
    struct {
        uint32_t           padding;
        union {
            int32_t        i32;
            uint32_t       u32;
            EJSWhyMagic     why;
        } payload;
    } s;
    double asDouble;
    void *asPtr;
    size_t asWord;
} EJSVAL_ALIGNMENT ejsval_layout;
# endif /* EJS_BITS_PER_WORD */
#endif  /* defined(IS_LITTLE_ENDIAN) */

EJS_STATIC_ASSERT(sizeof(ejsval_layout) == 8);

#if EJS_BITS_PER_WORD == 32

#define STATIC_BUILD_EJSVAL(tag, payload) { .asBits = (((uint64_t)(uint32_t)tag) << 32) | payload }

/*
 * N.B. GCC, in some but not all cases, chooses to emit signed comparison of
 * EJSValueTag even though its underlying type has been forced to be uint32_t.
 * Thus, all comparisons should explicitly cast operands to uint32_t.
 */

static EJS_ALWAYS_INLINE ejsval_layout
BUILD_EJSVAL(EJSValueTag tag, uint32_t payload)
{
    ejsval_layout l;
    l.asBits = (((uint64_t)(uint32_t)tag) << 32) | payload;
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_DOUBLE_IMPL(ejsval_layout l)
{
    return (uint32_t)l.s.tag <= (uint32_t)EJSVAL_TAG_CLEAR;
}

static EJS_ALWAYS_INLINE ejsval_layout
DOUBLE_TO_EJSVAL_IMPL(double d)
{
    ejsval_layout l;
    l.asDouble = d;
    EJS_ASSERT(EJSVAL_IS_DOUBLE_IMPL(l));
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_INT32_IMPL(ejsval_layout l)
{
    return l.s.tag == EJSVAL_TAG_INT32;
}

static EJS_ALWAYS_INLINE int32_t
EJSVAL_TO_INT32_IMPL(ejsval_layout l)
{
    return l.s.payload.i32;
}

static EJS_ALWAYS_INLINE ejsval_layout
INT32_TO_EJSVAL_IMPL(int32_t i)
{
    ejsval_layout l;
    l.s.tag = EJSVAL_TAG_INT32;
    l.s.payload.i32 = i;
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_NUMBER_IMPL(ejsval_layout l)
{
    EJSValueTag tag = l.s.tag;
    EJS_ASSERT(tag != EJSVAL_TAG_CLEAR);
    return (uint32_t)tag <= (uint32_t)EJSVAL_UPPER_INCL_TAG_OF_NUMBER_SET;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_UNDEFINED_IMPL(ejsval_layout l)
{
    return l.s.tag == EJSVAL_TAG_UNDEFINED;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_STRING_IMPL(ejsval_layout l)
{
    return l.s.tag == EJSVAL_TAG_STRING;
}

static EJS_ALWAYS_INLINE ejsval_layout
STRING_TO_EJSVAL_IMPL(EJSPrimString *str)
{
    ejsval_layout l;
    EJS_ASSERT(str);
    l.s.tag = EJSVAL_TAG_STRING;
    l.s.payload.str = str;
    return l;
}

static EJS_ALWAYS_INLINE EJSPrimString *
EJSVAL_TO_STRING_IMPL(ejsval_layout l)
{
    return l.s.payload.str;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_BOOLEAN_IMPL(ejsval_layout l)
{
    return l.s.tag == EJSVAL_TAG_BOOLEAN;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_TO_BOOLEAN_IMPL(ejsval_layout l)
{
    return l.s.payload.boo;
}

static EJS_ALWAYS_INLINE ejsval_layout
BOOLEAN_TO_EJSVAL_IMPL(EJSBool b)
{
    ejsval_layout l;
    EJS_ASSERT(b == EJS_TRUE || b == EJS_FALSE);
    l.s.tag = EJSVAL_TAG_BOOLEAN;
    l.s.payload.boo = b;
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_MAGIC_IMPL(ejsval_layout l)
{
    return l.s.tag == EJSVAL_TAG_MAGIC;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_OBJECT_IMPL(ejsval_layout l)
{
    return l.s.tag == EJSVAL_TAG_OBJECT;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_PRIMITIVE_IMPL(ejsval_layout l)
{
    return (uint32_t)l.s.tag < (uint32_t)EJSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_OBJECT_OR_NULL_IMPL(ejsval_layout l)
{
    EJS_ASSERT((uint32_t)l.s.tag <= (uint32_t)EJSVAL_TAG_OBJECT);
    return (uint32_t)l.s.tag >= (uint32_t)EJSVAL_LOWER_INCL_TAG_OF_OBJ_OR_NULL_SET;
}

static EJS_ALWAYS_INLINE EJSObject *
EJSVAL_TO_OBJECT_IMPL(ejsval_layout l)
{
    return l.s.payload.obj;
}

static EJS_ALWAYS_INLINE ejsval_layout
OBJECT_TO_EJSVAL_IMPL(EJSObject *obj)
{
    ejsval_layout l;
    EJS_ASSERT(obj);
    l.s.tag = EJSVAL_TAG_OBJECT;
    l.s.payload.obj = obj;
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_NULL_IMPL(ejsval_layout l)
{
    return l.s.tag == EJSVAL_TAG_NULL;
}

static EJS_ALWAYS_INLINE ejsval_layout
PRIVATE_PTR_TO_EJSVAL_IMPL(const void *ptr)
{
    ejsval_layout l;
    EJS_ASSERT(((uint32_t)ptr & 1) == 0);
    l.s.tag = (EJSValueTag)0;
    l.s.payload.ptr = ptr;
    EJS_ASSERT(EJSVAL_IS_DOUBLE_IMPL(l));
    return l;
}

static EJS_ALWAYS_INLINE void *
EJSVAL_TO_PRIVATE_PTR_IMPL(ejsval_layout l)
{
    return l.s.payload.ptr;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_GCTHING_IMPL(ejsval_layout l)
{
    /* gcc sometimes generates signed < without explicit casts. */
    return (uint32_t)l.s.tag >= (uint32_t)EJSVAL_LOWER_INCL_TAG_OF_GCTHING_SET;
}

static EJS_ALWAYS_INLINE void *
EJSVAL_TO_GCTHING_IMPL(ejsval_layout l)
{
    return l.s.payload.ptr;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_TRACEABLE_IMPL(ejsval_layout l)
{
    return l.s.tag == EJSVAL_TAG_STRING || l.s.tag == EJSVAL_TAG_OBJECT;
}

static EJS_ALWAYS_INLINE uint32_t
EJSVAL_TRACE_KIND_IMPL(ejsval_layout l)
{
    return (uint32_t)(EJSBool)EJSVAL_IS_STRING_IMPL(l);
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_SPECIFIC_INT32_IMPL(ejsval_layout l, int32_t i32)
{
    return l.s.tag == EJSVAL_TAG_INT32 && l.s.payload.i32 == i32;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_SPECIFIC_BOOLEAN(ejsval_layout l, EJSBool b)
{
    return (l.s.tag == EJSVAL_TAG_BOOLEAN) && (l.s.payload.boo == b);
}

static EJS_ALWAYS_INLINE ejsval_layout
MAGIC_TO_EJSVAL_IMPL(EJSWhyMagic why)
{
    ejsval_layout l;
    l.s.tag = EJSVAL_TAG_MAGIC;
    l.s.payload.why = why;
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_SAME_TYPE_IMPL(ejsval_layout lhs, ejsval_layout rhs)
{
    EJSValueTag ltag = lhs.s.tag, rtag = rhs.s.tag;
    return ltag == rtag || (ltag < EJSVAL_TAG_CLEAR && rtag < EJSVAL_TAG_CLEAR);
}

static EJS_ALWAYS_INLINE ejsval_layout
PRIVATE_UINT32_TO_EJSVAL_IMPL(uint32_t ui)
{
    ejsval_layout l;
    l.s.tag = (EJSValueTag)0;
    l.s.payload.u32 = ui;
    EJS_ASSERT(EJSVAL_IS_DOUBLE_IMPL(l));
    return l;
}

static EJS_ALWAYS_INLINE uint32_t
EJSVAL_TO_PRIVATE_UINT32_IMPL(ejsval_layout l)
{
    return l.s.payload.u32;
}

static EJS_ALWAYS_INLINE EJSValueType
EJSVAL_EXTRACT_NON_DOUBLE_TYPE_IMPL(ejsval_layout l)
{
    uint32_t type = l.s.tag & 0xF;
    EJS_ASSERT(type > EJSVAL_TYPE_DOUBLE);
    return (EJSValueType)type;
}

#elif EJS_BITS_PER_WORD == 64

#define STATIC_BUILD_EJSVAL(tag, v) { .asBits = (((uint64_t)(uint32_t)tag) << EJSVAL_TAG_SHIFT) | v }
#define STATIC_BUILD_DOUBLE_EJSVAL(v) { .asDouble = v }
#define STATIC_BUILD_BOOLEAN_EJSVAL(b) { .asBits = ((uint64_t)(uint32_t)b) | EJSVAL_SHIFTED_TAG_BOOLEAN }

static EJS_ALWAYS_INLINE EJSValueTag
EJSVAL_TO_TAG(ejsval_layout l)
{
    return (EJSValueTag)((uint64_t)l.asBits >> EJSVAL_TAG_SHIFT);
}

static EJS_ALWAYS_INLINE ejsval_layout
BUILD_EJSVAL(EJSValueTag tag, uint64_t payload)
{
    ejsval_layout l;
    l.asBits = (((uint64_t)(uint32_t)tag) << EJSVAL_TAG_SHIFT) | payload;
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_DOUBLE_IMPL(ejsval_layout l)
{
    return l.asBits <= EJSVAL_SHIFTED_TAG_MAX_DOUBLE;
}

static EJS_ALWAYS_INLINE ejsval_layout
DOUBLE_TO_EJSVAL_IMPL(double d)
{
    ejsval_layout l;
    l.asDouble = d;
    EJS_ASSERT(l.asBits <= EJSVAL_SHIFTED_TAG_MAX_DOUBLE);
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_INT32_IMPL(ejsval_layout l)
{
    return (uint32_t)(l.asBits >> EJSVAL_TAG_SHIFT) == EJSVAL_TAG_INT32;
}

static EJS_ALWAYS_INLINE int32_t
EJSVAL_TO_INT32_IMPL(ejsval_layout l)
{
    return (int32_t)l.asBits;
}

static EJS_ALWAYS_INLINE ejsval_layout
INT32_TO_EJSVAL_IMPL(int32_t i32)
{
    ejsval_layout l;
    l.asBits = ((uint64_t)(uint32_t)i32) | EJSVAL_SHIFTED_TAG_INT32;
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_NUMBER_IMPL(ejsval_layout l)
{
    return l.asBits < EJSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_UNDEFINED_IMPL(ejsval_layout l)
{
    return l.asBits == EJSVAL_SHIFTED_TAG_UNDEFINED;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_STRING_IMPL(ejsval_layout l)
{
    return (uint32_t)(l.asBits >> EJSVAL_TAG_SHIFT) == EJSVAL_TAG_STRING;
}

static EJS_ALWAYS_INLINE ejsval_layout
STRING_TO_EJSVAL_IMPL(EJSPrimString *str)
{
    ejsval_layout l;
    uint64_t strBits = (uint64_t)str;
    EJS_ASSERT(str);
    EJS_ASSERT((strBits >> EJSVAL_TAG_SHIFT) == 0);
    l.asBits = strBits | EJSVAL_SHIFTED_TAG_STRING;
    return l;
}

static EJS_ALWAYS_INLINE EJSPrimString *
EJSVAL_TO_STRING_IMPL(ejsval_layout l)
{
    return (EJSPrimString *)(l.asBits & EJSVAL_PAYLOAD_MASK);
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_BOOLEAN_IMPL(ejsval_layout l)
{
    return (uint32_t)(l.asBits >> EJSVAL_TAG_SHIFT) == EJSVAL_TAG_BOOLEAN;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_TO_BOOLEAN_IMPL(ejsval_layout l)
{
    return (EJSBool)l.asBits;
}

static EJS_ALWAYS_INLINE ejsval_layout
BOOLEAN_TO_EJSVAL_IMPL(EJSBool b)
{
    ejsval_layout l;
    EJS_ASSERT(b == EJS_TRUE || b == EJS_FALSE);
    l.asBits = ((uint64_t)(uint32_t)b) | EJSVAL_SHIFTED_TAG_BOOLEAN;
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_MAGIC_IMPL(ejsval_layout l)
{
    return (l.asBits >> EJSVAL_TAG_SHIFT) == EJSVAL_TAG_MAGIC;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_PRIMITIVE_IMPL(ejsval_layout l)
{
    return l.asBits < EJSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_OBJECT_IMPL(ejsval_layout l)
{
    EJS_ASSERT((l.asBits >> EJSVAL_TAG_SHIFT) <= EJSVAL_SHIFTED_TAG_OBJECT);
    return l.asBits >= EJSVAL_SHIFTED_TAG_OBJECT;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_OBJECT_OR_NULL_IMPL(ejsval_layout l)
{
    EJS_ASSERT((l.asBits >> EJSVAL_TAG_SHIFT) <= EJSVAL_TAG_OBJECT);
    return l.asBits >= EJSVAL_LOWER_INCL_SHIFTED_TAG_OF_OBJ_OR_NULL_SET;
}

static EJS_ALWAYS_INLINE EJSObject *
EJSVAL_TO_OBJECT_IMPL(ejsval_layout l)
{
    uint64_t ptrBits = l.asBits & EJSVAL_PAYLOAD_MASK;
    //EJS_ASSERT((ptrBits & 0x7) == 0);
    return (EJSObject *)ptrBits;
}

static EJS_ALWAYS_INLINE ejsval_layout
OBJECT_TO_EJSVAL_IMPL(EJSObject *obj)
{
    ejsval_layout l;
    uint64_t objBits = (uint64_t)obj;
    EJS_ASSERT(obj);
    EJS_ASSERT((objBits >> EJSVAL_TAG_SHIFT) == 0);
    l.asBits = objBits | EJSVAL_SHIFTED_TAG_OBJECT;
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_NULL_IMPL(ejsval_layout l)
{
    return l.asBits == EJSVAL_SHIFTED_TAG_NULL;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_GCTHING_IMPL(ejsval_layout l)
{
    return l.asBits >= EJSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET;
}

static EJS_ALWAYS_INLINE void *
EJSVAL_TO_GCTHING_IMPL(ejsval_layout l)
{
    uint64_t ptrBits = l.asBits & EJSVAL_PAYLOAD_MASK;
    //EJS_ASSERT((ptrBits & 0x7) == 0);
    return (void *)ptrBits;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_TRACEABLE_IMPL(ejsval_layout l)
{
    return EJSVAL_IS_GCTHING_IMPL(l) && !EJSVAL_IS_NULL_IMPL(l);
}

static EJS_ALWAYS_INLINE uint32_t
EJSVAL_TRACE_KIND_IMPL(ejsval_layout l)
{
    return (uint32_t)(EJSBool)!(EJSVAL_IS_OBJECT_IMPL(l));
}

static EJS_ALWAYS_INLINE ejsval_layout
PRIVATE_PTR_TO_EJSVAL_IMPL(const void *ptr)
{
    ejsval_layout l;
    uint64_t ptrBits = (uint64_t)ptr;
    EJS_ASSERT((ptrBits & 1) == 0);
    l.asBits = ptrBits >> 1;
    EJS_ASSERT(EJSVAL_IS_DOUBLE_IMPL(l));
    return l;
}

static EJS_ALWAYS_INLINE void *
EJSVAL_TO_PRIVATE_PTR_IMPL(ejsval_layout l)
{
    EJS_ASSERT((l.asBits & 0x8000000000000000LL) == 0);
    return (void *)(l.asBits << 1);
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_SPECIFIC_INT32_IMPL(ejsval_layout l, int32_t i32)
{
    return l.asBits == (((uint64_t)(uint32_t)i32) | EJSVAL_SHIFTED_TAG_INT32);
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_IS_SPECIFIC_BOOLEAN(ejsval_layout l, EJSBool b)
{
    return l.asBits == (((uint64_t)(uint32_t)b) | EJSVAL_SHIFTED_TAG_BOOLEAN);
}

static EJS_ALWAYS_INLINE ejsval_layout
MAGIC_TO_EJSVAL_IMPL(EJSWhyMagic why)
{
    ejsval_layout l;
    l.asBits = ((uint64_t)(uint32_t)why) | EJSVAL_SHIFTED_TAG_MAGIC;
    return l;
}

static EJS_ALWAYS_INLINE EJSBool
EJSVAL_SAME_TYPE_IMPL(ejsval_layout lhs, ejsval_layout rhs)
{
    uint64_t lbits = lhs.asBits, rbits = rhs.asBits;
    return (lbits <= EJSVAL_SHIFTED_TAG_MAX_DOUBLE && rbits <= EJSVAL_SHIFTED_TAG_MAX_DOUBLE) ||
           (((lbits ^ rbits) & 0xFFFF800000000000LL) == 0);
}

static EJS_ALWAYS_INLINE ejsval_layout
PRIVATE_UINT32_TO_EJSVAL_IMPL(uint32_t ui)
{
    ejsval_layout l;
    l.asBits = (uint64_t)ui;
    EJS_ASSERT(EJSVAL_IS_DOUBLE_IMPL(l));
    return l;
}

static EJS_ALWAYS_INLINE uint32_t
EJSVAL_TO_PRIVATE_UINT32_IMPL(ejsval_layout l)
{
    EJS_ASSERT((l.asBits >> 32) == 0);
    return (uint32_t)l.asBits;
}

static EJS_ALWAYS_INLINE EJSValueType
EJSVAL_EXTRACT_NON_DOUBLE_TYPE_IMPL(ejsval_layout l)
{
   uint64_t type = (l.asBits >> EJSVAL_TAG_SHIFT) & 0xF;
   EJS_ASSERT(type > EJSVAL_TYPE_DOUBLE);
   return (EJSValueType)type;
}

#endif  /* EJS_BITS_PER_WORD */

static EJS_ALWAYS_INLINE double
EJS_CANONICALIZE_NAN(double d)
{
    if (EJS_UNLIKELY(d != d)) {
        ejsval_layout l;
        l.asBits = 0x7FF8000000000000LL;
        return l.asDouble;
    }
    return d;
}

EJS_END_DECLS

typedef ejsval_layout ejsval;

#endif /* ejsvalimpl_h__ */
