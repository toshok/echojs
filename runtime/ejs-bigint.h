/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#ifndef _ejs_bigint_h_
#define _ejs_bigint_h_

#include "ejs.h"
#include "ejs-value.h"
#include "ejs-object.h"

// The BigInt primitive: a GC leaf cell with the sign and the 64-bit
// magnitude digits inline (little-endian) — relocatable, no finalizer,
// no satellite allocation.  Zero is length 0 (sign never set).
// Digit layout matches the vendored v8-bigint library's digit_t, so
// operations run directly over the cell's digits.
struct _EJSBigInt {
    GCObjectHeader gc_header;
    uint32_t length;
    uint32_t sign; // 1 = negative
    uint64_t digits[];
};

#define EJSVAL_IS_BIGINT_OBJECT(v) (EJSVAL_IS_OBJECT(v) && EJSVAL_TO_OBJECT(v)->ops == &_ejs_BigInt_specops)

// wrapper object (ToObject(bigint) / method receivers)
typedef struct {
    /* object header */
    EJSObject obj;

    ejsval primBigInt;
} EJSBigIntObject;

EJS_BEGIN_DECLS

extern ejsval _ejs_BigInt;
extern ejsval _ejs_BigInt_prototype;
extern EJSSpecOps _ejs_BigInt_specops;

void _ejs_bigint_init(ejsval global);

// creation
ejsval _ejs_bigint_new_from_int64 (int64_t value);
ejsval _ejs_bigint_new_from_uint64 (uint64_t value);
// value must be integral and finite (the caller checks; RangeError otherwise)
ejsval _ejs_bigint_new_from_double (double value);
// StringToBigInt: whitespace/0x/0o/0b handling per spec.  On parse
// failure: returns undefined (loose-eq wants NaN-ish semantics; BigInt()
// and the literal path turn it into a SyntaxError).
ejsval _ejs_bigint_from_string (ejsval str);
// literal path: raw digit chars (no prefix, no sign) in the given radix
ejsval _ejs_bigint_from_digits_utf8 (const char* chars, int len, int radix);
// %bigintFromLiteral: the literal's source digits as an ejs string
// (prefix included, numeric separators allowed); SyntaxError on garbage
ejsval _ejs_bigint_from_literal (ejsval str);

ejsval _ejs_bigint_new_object (ejsval bigint_data);

// conversions
ejsval _ejs_bigint_to_ejs_string (ejsval bi, int radix);
double _ejs_bigint_to_double (ejsval bi);
EJSBool _ejs_bigint_is_zero (ejsval bi);

// operators (all operands must already be bigints; callers enforce the
// no-implicit-mixing TypeErrors)
ejsval _ejs_bigint_add (ejsval x, ejsval y);
ejsval _ejs_bigint_sub (ejsval x, ejsval y);
ejsval _ejs_bigint_mul (ejsval x, ejsval y);
ejsval _ejs_bigint_div (ejsval x, ejsval y); // RangeError on /0
ejsval _ejs_bigint_mod (ejsval x, ejsval y); // RangeError on %0
ejsval _ejs_bigint_pow (ejsval x, ejsval y); // RangeError on negative exponent
ejsval _ejs_bigint_neg (ejsval x);
ejsval _ejs_bigint_bitnot (ejsval x);
ejsval _ejs_bigint_bitand (ejsval x, ejsval y);
ejsval _ejs_bigint_bitor (ejsval x, ejsval y);
ejsval _ejs_bigint_bitxor (ejsval x, ejsval y);
ejsval _ejs_bigint_shl (ejsval x, ejsval y);
ejsval _ejs_bigint_shr (ejsval x, ejsval y); // signed (there is no >>> for bigints)

// comparison
int _ejs_bigint_cmp (ejsval x, ejsval y); // -1/0/1
// -1/0/1, or 2 for unordered (NaN)
int _ejs_bigint_cmp_double (ejsval x, double d);
EJSBool _ejs_bigint_equals_string (ejsval x, ejsval str);

// BigInt.asIntN/asUintN cores
ejsval _ejs_bigint_as_intn (uint32_t bits, ejsval bi);
ejsval _ejs_bigint_as_uintn (uint32_t bits, ejsval bi);

EJS_END_DECLS

#endif /* _ejs_bigint_h_ */
