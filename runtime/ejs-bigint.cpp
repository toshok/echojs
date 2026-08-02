/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */
//
// The BigInt primitive over the vendored v8-bigint library (see
// docs/bigint-plan.md).  This is the C++ half of the runtime, like
// ejs-dtoa.cpp: the ejs-facing API is C (ejs-bigint.h); the digit math
// is v8::bigint over the cell's inline digits — results are computed
// directly into freshly allocated cells (the library's caller-
// preallocates convention), so there are no satellite buffers and no
// finalizers.  C-held operand pointers are pinned by the conservative
// stack scan, so allocating the result cell before reading operand
// digits is safe.

#include <math.h>
#include <string.h>

#include "ejs-bigint.h"
#include "ejs-error.h"
#include "ejs-function.h"
#include "ejs-gc.h"
#include "ejs-ops.h"
#include "ejs-string.h"
#include "ejs-symbol.h"

#include "src/bigint/bigint.h"
#include "src/bigint/bigint-inl.h"

using namespace v8::bigint;

static_assert(sizeof(digit_t) == sizeof(uint64_t), "64-bit digits expected");

// implementation-defined size limit (RangeError beyond): 2^20 64-bit
// digits = 64Mbit, matching the order of magnitude other engines allow
static const uint32_t EJS_BIGINT_MAX_DIGITS = 1 << 20;

static Processor* processor = NULL;

// ---------------------------------------------------------------- cells

static EJSBigInt*
bigint_alloc (uint32_t length)
{
    if (length > EJS_BIGINT_MAX_DIGITS)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "BigInt too big");
    EJSBigInt* rv = (EJSBigInt*)_ejs_gc_alloc (sizeof(EJSBigInt) + (size_t)length * sizeof(uint64_t),
                                               EJS_SCAN_TYPE_BIGINT);
    rv->length = length;
    rv->sign = 0;
    return rv;
}

static Digits
digits_of (EJSBigInt* bi)
{
    return Digits((digit_t*)bi->digits, bi->length);
}

// trim leading zero digits; a zero value is length 0, sign 0
static ejsval
canonicalize (EJSBigInt* bi, bool negative)
{
    Digits d = digits_of(bi);
    d.Normalize();
    bi->length = d.len();
    bi->sign = (bi->length > 0 && negative) ? 1 : 0;
    return BIGINT_TO_EJSVAL(bi);
}

static ejsval
bigint_zero ()
{
    return BIGINT_TO_EJSVAL(bigint_alloc(0));
}

ejsval
_ejs_bigint_new_from_uint64 (uint64_t value)
{
    if (value == 0) return bigint_zero();
    EJSBigInt* rv = bigint_alloc(1);
    rv->digits[0] = value;
    return BIGINT_TO_EJSVAL(rv);
}

ejsval
_ejs_bigint_new_from_int64 (int64_t value)
{
    if (value == 0) return bigint_zero();
    EJSBigInt* rv = bigint_alloc(1);
    if (value < 0) {
        rv->digits[0] = (uint64_t)(-(value + 1)) + 1; // avoids INT64_MIN UB
        rv->sign = 1;
    }
    else {
        rv->digits[0] = (uint64_t)value;
    }
    return BIGINT_TO_EJSVAL(rv);
}

// value must be integral and finite (callers check)
ejsval
_ejs_bigint_new_from_double (double value)
{
    if (value == 0) return bigint_zero();
    EJSBool neg = value < 0;
    double a = fabs(value);
    if (a < 9007199254740992.0 /* 2^53 */) {
        ejsval rv = _ejs_bigint_new_from_uint64 ((uint64_t)a);
        EJSVAL_TO_BIGINT(rv)->sign = neg ? 1 : 0;
        return rv;
    }
    int e;
    double m = frexp(a, &e);            // a = m * 2^e, m in [0.5, 1)
    uint64_t mant = (uint64_t)ldexp(m, 53); // top 53 bits
    int shift = e - 53;                 // a = mant * 2^shift, shift >= 0
    uint32_t digit_off = (uint32_t)(shift / 64);
    uint32_t bit_off = (uint32_t)(shift % 64);
    uint32_t len = digit_off + (bit_off > 11 ? 2 : 1);
    EJSBigInt* rv = bigint_alloc(len);
    memset(rv->digits, 0, (size_t)len * sizeof(uint64_t));
    rv->digits[digit_off] = mant << bit_off;
    if (bit_off > 11 && digit_off + 1 < len)
        rv->digits[digit_off + 1] = mant >> (64 - bit_off);
    return canonicalize(rv, neg);
}

EJSBool
_ejs_bigint_is_zero (ejsval bi)
{
    return EJSVAL_TO_BIGINT(bi)->length == 0;
}

// ------------------------------------------------------------ conversions

double
_ejs_bigint_to_double (ejsval biv)
{
    EJSBigInt* bi = EJSVAL_TO_BIGINT(biv);
    uint32_t len = bi->length;
    if (len == 0) return 0.0;
    double sign = bi->sign ? -1.0 : 1.0;
    if (len == 1) return sign * (double)bi->digits[0];

    // round-to-nearest-even from the top 64+ bits
    uint64_t msd = bi->digits[len - 1];
    int msd_bits = 64 - __builtin_clzll(msd);
    int total_bits = (int)(len - 1) * 64 + msd_bits;
    if (total_bits > 1024) return sign * INFINITY;

    // gather the top 64 bits into `top`, note whether anything below is set
    uint64_t top;
    bool sticky = false;
    int below;                          // bits of value below `top`
    if (msd_bits == 64) {
        top = msd;
        below = (int)(len - 1) * 64;
        for (uint32_t i = 0; i + 1 < len && !sticky; i++)
            if (bi->digits[i]) sticky = true;
    }
    else {
        uint64_t next = bi->digits[len - 2];
        top = (msd << (64 - msd_bits)) | (next >> msd_bits);
        if (next << (64 - msd_bits)) sticky = true;
        below = (int)(len - 2) * 64;
        for (uint32_t i = 0; i + 2 < len && !sticky; i++)
            if (bi->digits[i]) sticky = true;
    }
    // keep 53 bits of mantissa; bit 10 down are rounded away
    uint64_t mant = top >> 11;
    uint64_t rest = top & 0x7ff;
    bool round_up = (rest > 0x400) || (rest == 0x400 && (sticky || (mant & 1)));
    if (round_up) {
        mant++;
        if (mant == (1ull << 53)) { mant >>= 1; total_bits++; if (total_bits > 1024) return sign * INFINITY; }
    }
    return sign * ldexp((double)mant, total_bits - 53);
}

ejsval
_ejs_bigint_to_ejs_string (ejsval biv, int radix)
{
    EJSBigInt* bi = EJSVAL_TO_BIGINT(biv);
    if (bi->length == 0) return _ejs_string_new_utf8("0");
    Digits X = digits_of(bi);
    uint32_t len = ToStringResultLength(X, radix, bi->sign != 0);
    char* buf = (char*)malloc(len + 1);
    uint32_t out_len = len;
    processor->ToString(buf, &out_len, X, radix, bi->sign != 0);
    ejsval rv = _ejs_string_new_utf8_len(buf, (int)out_len);
    free(buf);
    return rv;
}

// shared by StringToBigInt and the literal path.  chars/len are the
// digit characters only (sign and radix prefix already consumed).
template <typename CharT>
static ejsval
parse_digits (const CharT* chars, int len, int radix, bool negative)
{
    if (len == 0) return _ejs_undefined;
    FromStringAccumulator accumulator(EJS_BIGINT_MAX_DIGITS, processor->platform());
    const CharT* end = chars + len;
    const CharT* pos = accumulator.Parse(chars, end, (digit_t)radix);
    if (pos != end) return _ejs_undefined; // invalid character
    if (accumulator.result() == FromStringAccumulator::Result::kMaxSizeExceeded)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "BigInt too big");
    uint32_t result_len = accumulator.ResultLength();
    EJSBigInt* rv = bigint_alloc(result_len);
    RWDigits Z((digit_t*)rv->digits, result_len);
    processor->FromString(Z, &accumulator);
    return canonicalize(rv, negative);
}

ejsval
_ejs_bigint_from_digits_utf8 (const char* chars, int len, int radix)
{
    return parse_digits((const uint8_t*)chars, len, radix, false);
}

// the compiled form of a BigInt literal: parse the source digits
// (radix prefix included; numeric separators stripped here)
ejsval
_ejs_bigint_from_literal (ejsval str)
{
    EJSPrimString* flat = _ejs_string_flatten(str);
    const jschar* s = flat->data.flat;
    int len = flat->length;
    // strip '_' separators into a scratch copy
    if (len < 0)
        return _ejs_undefined;
    jschar* buf = (jschar*)calloc((size_t)len, sizeof(jschar));
    int n = 0;
    for (int i = 0; i < len; i++)
        if (s[i] != '_') buf[n++] = s[i];
    ejsval clean = _ejs_string_new_ucs2_len(buf, n);
    free(buf);
    ejsval rv = _ejs_bigint_from_string(clean);
    if (!EJSVAL_IS_BIGINT(rv))
        _ejs_throw_nativeerror_utf8 (EJS_SYNTAX_ERROR, "Invalid BigInt literal");
    return rv;
}

static bool
is_ws (jschar c)
{
    return c == 0x09 || c == 0x0A || c == 0x0B || c == 0x0C || c == 0x0D || c == 0x20
        || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028
        || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000 || c == 0xFEFF;
}

// StringToBigInt: returns undefined on a malformed string (callers
// decide between SyntaxError and loose-equality false)
ejsval
_ejs_bigint_from_string (ejsval str)
{
    EJSPrimString* flat = _ejs_string_flatten(str);
    const jschar* s = flat->data.flat;
    int len = flat->length;
    int start = 0, end = len;
    while (start < end && is_ws(s[start])) start++;
    while (end > start && is_ws(s[end - 1])) end--;
    if (start == end) return bigint_zero(); // empty/whitespace = 0n

    bool negative = false;
    int radix = 10;
    if (s[start] == '0' && start + 1 < end) {
        jschar c = s[start + 1];
        if (c == 'x' || c == 'X') { radix = 16; start += 2; }
        else if (c == 'o' || c == 'O') { radix = 8; start += 2; }
        else if (c == 'b' || c == 'B') { radix = 2; start += 2; }
    }
    if (radix == 10 && (s[start] == '+' || s[start] == '-')) {
        negative = s[start] == '-';
        start++;
    }
    return parse_digits(s + start, end - start, radix, negative);
}

// ------------------------------------------------------------- operators

ejsval
_ejs_bigint_add (ejsval xv, ejsval yv)
{
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    EJSBigInt* y = EJSVAL_TO_BIGINT(yv);
    bool same_sign = x->sign == y->sign;
    EJSBigInt* z = bigint_alloc(AddSignedResultLength(x->length, y->length, same_sign));
    x = EJSVAL_TO_BIGINT(xv); y = EJSVAL_TO_BIGINT(yv);
    RWDigits Z((digit_t*)z->digits, z->length);
    bool neg = AddSigned(Z, digits_of(x), x->sign != 0, digits_of(y), y->sign != 0);
    return canonicalize(z, neg);
}

ejsval
_ejs_bigint_sub (ejsval xv, ejsval yv)
{
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    EJSBigInt* y = EJSVAL_TO_BIGINT(yv);
    bool same_sign = x->sign == y->sign;
    EJSBigInt* z = bigint_alloc(SubtractSignedResultLength(x->length, y->length, same_sign));
    x = EJSVAL_TO_BIGINT(xv); y = EJSVAL_TO_BIGINT(yv);
    RWDigits Z((digit_t*)z->digits, z->length);
    bool neg = SubtractSigned(Z, digits_of(x), x->sign != 0, digits_of(y), y->sign != 0);
    return canonicalize(z, neg);
}

ejsval
_ejs_bigint_mul (ejsval xv, ejsval yv)
{
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    EJSBigInt* y = EJSVAL_TO_BIGINT(yv);
    if (x->length == 0 || y->length == 0) return bigint_zero();
    bool neg = (x->sign != 0) != (y->sign != 0);
    EJSBigInt* z = bigint_alloc(MultiplyResultLength(digits_of(x), digits_of(y)));
    x = EJSVAL_TO_BIGINT(xv); y = EJSVAL_TO_BIGINT(yv);
    RWDigits Z((digit_t*)z->digits, z->length);
    Digits X = digits_of(x), Y = digits_of(y);
    if (!MultiplySmall(Z, X, Y).first)
        processor->MultiplyLarge(Z, X, Y);
    return canonicalize(z, neg);
}

ejsval
_ejs_bigint_div (ejsval xv, ejsval yv)
{
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    EJSBigInt* y = EJSVAL_TO_BIGINT(yv);
    if (y->length == 0)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "Division by zero");
    if (x->length == 0) return bigint_zero();
    bool neg = (x->sign != 0) != (y->sign != 0);
    EJSBigInt* z = bigint_alloc(DivideResultLength(digits_of(x), digits_of(y)));
    x = EJSVAL_TO_BIGINT(xv); y = EJSVAL_TO_BIGINT(yv);
    RWDigits Q((digit_t*)z->digits, z->length);
    Digits A = digits_of(x), B = digits_of(y);
    if (!DivideSmall(Q, A, B).first)
        processor->DivideLarge(Q, A, B);
    return canonicalize(z, neg);
}

ejsval
_ejs_bigint_mod (ejsval xv, ejsval yv)
{
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    EJSBigInt* y = EJSVAL_TO_BIGINT(yv);
    if (y->length == 0)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "Division by zero");
    if (x->length == 0) return bigint_zero();
    bool neg = x->sign != 0; // truncated division: remainder takes the dividend's sign
    EJSBigInt* z = bigint_alloc(ModuloResultLength(digits_of(y)));
    x = EJSVAL_TO_BIGINT(xv); y = EJSVAL_TO_BIGINT(yv);
    RWDigits R((digit_t*)z->digits, z->length);
    Digits A = digits_of(x), B = digits_of(y);
    if (!ModuloSmall(R, A, B).first)
        processor->ModuloLarge(R, A, B);
    return canonicalize(z, neg);
}

ejsval
_ejs_bigint_pow (ejsval xv, ejsval yv)
{
    EJSBigInt* y = EJSVAL_TO_BIGINT(yv);
    if (y->sign)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "Exponent must be non-negative");
    if (y->length == 0) return _ejs_bigint_new_from_int64(1);
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    if (x->length == 0) return bigint_zero();
    // base 1 / -1: result is ±1 regardless of exponent size
    if (x->length == 1 && x->digits[0] == 1) {
        EJSBool odd = (y->digits[0] & 1) != 0;
        return _ejs_bigint_new_from_int64 (x->sign && odd ? -1 : 1);
    }
    if (y->length > 1)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "BigInt too big");
    uint64_t e = y->digits[0];
    // square-and-multiply on ejsvals (each op allocates; fine — pow of
    // any real size RangeErrors on the result cap long before looping
    // becomes a problem)
    ejsval result = _ejs_bigint_new_from_int64(1);
    ejsval base = xv;
    while (e > 0) {
        if (e & 1) result = _ejs_bigint_mul(result, base);
        e >>= 1;
        if (e) base = _ejs_bigint_mul(base, base);
    }
    return result;
}

ejsval
_ejs_bigint_neg (ejsval xv)
{
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    if (x->length == 0) return xv;
    int saved_length = x->length;
    EJSBigInt* z = bigint_alloc(saved_length);
    x = EJSVAL_TO_BIGINT(xv);
    memcpy(z->digits, x->digits, (size_t)saved_length * sizeof(uint64_t));
    z->length = saved_length;
    z->sign = x->sign ? 0 : 1;
    return BIGINT_TO_EJSVAL(z);
}

// ~x = -(x+1)
ejsval
_ejs_bigint_bitnot (ejsval xv)
{
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    if (x->sign) {
        // ~(-m) = m-1
        EJSBigInt* z = bigint_alloc(x->length);
        x = EJSVAL_TO_BIGINT(xv);
        RWDigits Z((digit_t*)z->digits, z->length);
        SubtractOne(Z, digits_of(x));
        return canonicalize(z, false);
    }
    // ~m = -(m+1)
    EJSBigInt* z = bigint_alloc(x->length + 1);
    x = EJSVAL_TO_BIGINT(xv);
    RWDigits Z((digit_t*)z->digits, z->length);
    AddOne(Z, digits_of(x));
    return canonicalize(z, true);
}

typedef void (*bitwise_fn)(RWDigits, Digits, Digits);

static ejsval
bitwise_op (ejsval xv, ejsval yv,
            bitwise_fn pos_pos, bitwise_fn neg_neg, bitwise_fn pos_neg,
            bool rs_pos_pos, bool rs_neg_neg, bool rs_pos_neg)
{
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    EJSBigInt* y = EJSVAL_TO_BIGINT(yv);
    // max+1 covers every variant (the +1 for the two's-complement carry)
    EJSBigInt* z = bigint_alloc((x->length > y->length ? x->length : y->length) + 1);
    x = EJSVAL_TO_BIGINT(xv); y = EJSVAL_TO_BIGINT(yv);
    RWDigits Z((digit_t*)z->digits, z->length);
    Digits X = digits_of(x), Y = digits_of(y);
    bool neg;
    if (!x->sign && !y->sign) { pos_pos(Z, X, Y); neg = rs_pos_pos; }
    else if (x->sign && y->sign) { neg_neg(Z, X, Y); neg = rs_neg_neg; }
    else if (!x->sign) { pos_neg(Z, X, Y); neg = rs_pos_neg; }
    else { pos_neg(Z, Y, X); neg = rs_pos_neg; } // callers must swap
    return canonicalize(z, neg);
}

ejsval
_ejs_bigint_bitand (ejsval x, ejsval y)
{
    return bitwise_op(x, y, BitwiseAnd_PosPos, BitwiseAnd_NegNeg, BitwiseAnd_PosNeg,
                      false, true, false);
}

ejsval
_ejs_bigint_bitor (ejsval x, ejsval y)
{
    return bitwise_op(x, y, BitwiseOr_PosPos, BitwiseOr_NegNeg, BitwiseOr_PosNeg,
                      false, true, true);
}

ejsval
_ejs_bigint_bitxor (ejsval x, ejsval y)
{
    return bitwise_op(x, y, BitwiseXor_PosPos, BitwiseXor_NegNeg, BitwiseXor_PosNeg,
                      false, false, true);
}

ejsval
_ejs_bigint_shl (ejsval xv, ejsval yv)
{
    EJSBigInt* y = EJSVAL_TO_BIGINT(yv);
    if (y->sign) return _ejs_bigint_shr(xv, _ejs_bigint_neg(yv));
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    if (x->length == 0) return bigint_zero();
    if (y->length == 0) return xv;
    if (y->length > 1)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "BigInt too big");
    uint64_t shift = EJSVAL_TO_BIGINT(yv)->digits[0];
    if (shift / 64 + x->length + 1 > EJS_BIGINT_MAX_DIGITS)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "BigInt too big");
    bool neg = x->sign != 0;
    EJSBigInt* z = bigint_alloc(x->length + (uint32_t)(shift / 64) + 1);
    x = EJSVAL_TO_BIGINT(xv);
    RWDigits Z((digit_t*)z->digits, z->length);
    LeftShift(Z, digits_of(x), (digit_t)shift);
    return canonicalize(z, neg);
}

ejsval
_ejs_bigint_shr (ejsval xv, ejsval yv)
{
    EJSBigInt* y = EJSVAL_TO_BIGINT(yv);
    if (y->sign) return _ejs_bigint_shl(xv, _ejs_bigint_neg(yv));
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    if (x->length == 0) return bigint_zero();
    if (y->length == 0) return xv;
    bool neg = x->sign != 0;
    // shifting everything out: positives go to 0, negatives to -1 (the
    // arithmetic shift's sign fill)
    uint64_t x_bits = (uint64_t)x->length * 64;
    if (y->length > 1 || y->digits[0] >= x_bits) {
        if (!neg) return bigint_zero();
        return _ejs_bigint_new_from_int64(-1);
    }
    uint64_t shift = y->digits[0];
    RightShiftState state;
    uint32_t rlen = RightShift_ResultLength(digits_of(x), neg, (digit_t)shift, &state);
    EJSBigInt* z = bigint_alloc(rlen > 0 ? rlen : 1);
    x = EJSVAL_TO_BIGINT(xv);
    z->length = rlen;
    RWDigits Z((digit_t*)z->digits, rlen);
    RightShift(Z, digits_of(x), (digit_t)shift, state);
    ejsval rv = canonicalize(z, neg);
    // a negative that rounded to nothing is -1
    if (neg && EJSVAL_TO_BIGINT(rv)->length == 0)
        return _ejs_bigint_new_from_int64(-1);
    return rv;
}

// ------------------------------------------------------------- comparison

int
_ejs_bigint_cmp (ejsval xv, ejsval yv)
{
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    EJSBigInt* y = EJSVAL_TO_BIGINT(yv);
    if (x->sign != y->sign) return x->sign ? -1 : 1;
    int c = Compare(digits_of(x), digits_of(y));
    int r = c < 0 ? -1 : c > 0 ? 1 : 0;
    return x->sign ? -r : r;
}

int
_ejs_bigint_cmp_double (ejsval xv, double d)
{
    if (isnan(d)) return 2;
    EJSBigInt* x = EJSVAL_TO_BIGINT(xv);
    if (isinf(d)) return d > 0 ? -1 : 1;
    double bd = _ejs_bigint_to_double(xv);
    if (bd < d) return -1;
    if (bd > d) return 1;
    // equal after rounding: an integral d is genuinely equal only if the
    // bigint round-trips exactly; a fractional part breaks the tie
    double di = trunc(d);
    if (di != d) {
        // bd == d impossible for fractional d (bd integral); handled above
        return d > 0 ? -1 : 1;
    }
    // both integral and equal as doubles.  For |d| < 2^53 the double is
    // exact, so they are equal.  Beyond that, compare exactly via a
    // bigint round-trip of d.
    if (fabs(d) < 9007199254740992.0) return 0;
    ejsval dv = _ejs_bigint_new_from_double(d);
    return _ejs_bigint_cmp(xv, dv);
}

EJSBool
_ejs_bigint_equals_string (ejsval xv, ejsval str)
{
    ejsval parsed = _ejs_bigint_from_string(str);
    if (!EJSVAL_IS_BIGINT(parsed)) return EJS_FALSE;
    return _ejs_bigint_cmp(xv, parsed) == 0;
}

// ---------------------------------------------------------- asIntN/asUintN

ejsval
_ejs_bigint_as_uintn (uint32_t bits, ejsval biv)
{
    EJSBigInt* bi = EJSVAL_TO_BIGINT(biv);
    if (bi->length == 0) return biv;
    if (bits == 0) return bigint_zero();
    if (!bi->sign) {
        int rlen = AsUintN_Pos_ResultLength(digits_of(bi), bits);
        if (rlen < 0) return biv; // no-op
        EJSBigInt* z = bigint_alloc((uint32_t)rlen);
        bi = EJSVAL_TO_BIGINT(biv);
        z->length = (uint32_t)rlen;
        RWDigits Z((digit_t*)z->digits, (uint32_t)rlen);
        AsUintN_Pos(Z, digits_of(bi), bits);
        return canonicalize(z, false);
    }
    uint32_t rlen = AsUintN_Neg_ResultLength(bits);
    EJSBigInt* z = bigint_alloc(rlen);
    bi = EJSVAL_TO_BIGINT(biv);
    z->length = rlen;
    RWDigits Z((digit_t*)z->digits, rlen);
    AsUintN_Neg(Z, digits_of(bi), bits);
    return canonicalize(z, false);
}

ejsval
_ejs_bigint_as_intn (uint32_t bits, ejsval biv)
{
    EJSBigInt* bi = EJSVAL_TO_BIGINT(biv);
    if (bi->length == 0) return biv;
    if (bits == 0) return bigint_zero();
    int rlen = AsIntNResultLength(digits_of(bi), bi->sign != 0, bits);
    if (rlen < 0) return biv; // no-op
    EJSBigInt* z = bigint_alloc((uint32_t)rlen);
    bi = EJSVAL_TO_BIGINT(biv);
    z->length = (uint32_t)rlen;
    RWDigits Z((digit_t*)z->digits, (uint32_t)rlen);
    bool neg = AsIntN(Z, digits_of(bi), bi->sign != 0, bits);
    return canonicalize(z, neg);
}

// ------------------------------------------------------ the BigInt builtin

ejsval _ejs_BigInt EJSVAL_ALIGNMENT;
ejsval _ejs_BigInt_prototype EJSVAL_ALIGNMENT;

ejsval
_ejs_bigint_new_object (ejsval bigint_data)
{
    EJSBigIntObject* rv = _ejs_gc_new(EJSBigIntObject);
    _ejs_init_object ((EJSObject*)rv, _ejs_BigInt_prototype, &_ejs_BigInt_specops);
    rv->primBigInt = bigint_data;
    return OBJECT_TO_EJSVAL(rv);
}

// ToBigInt (7.1.13): number is a TypeError here — only the explicit
// BigInt() call converts numbers
static ejsval
ToBigInt (ejsval value)
{
    ejsval prim = ToPrimitive(value, TO_PRIM_HINT_NUMBER);
    if (EJSVAL_IS_BIGINT(prim)) return prim;
    if (EJSVAL_IS_BOOLEAN(prim)) return _ejs_bigint_new_from_int64(EJSVAL_TO_BOOLEAN(prim) ? 1 : 0);
    if (EJSVAL_IS_STRING(prim)) {
        ejsval rv = _ejs_bigint_from_string(prim);
        if (!EJSVAL_IS_BIGINT(rv))
            _ejs_throw_nativeerror_utf8 (EJS_SYNTAX_ERROR, "Cannot convert string to a BigInt");
        return rv;
    }
    _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Cannot convert value to a BigInt");
}

static EJS_NATIVE_FUNC(_ejs_BigInt_impl) {
    if (!EJSVAL_IS_UNDEFINED(newTarget))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "BigInt is not a constructor");

    ejsval value = argc > 0 ? args[0] : _ejs_undefined;
    ejsval prim = ToPrimitive(value, TO_PRIM_HINT_NUMBER);
    if (EJSVAL_IS_NUMBER(prim)) {
        double d = EJSVAL_TO_NUMBER(prim);
        if (!isfinite(d) || trunc(d) != d)
            _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "The number is not a safe integer");
        return _ejs_bigint_new_from_double(d);
    }
    return ToBigInt(prim);
}

// ToIndex (7.1.22): undefined => 0, ToIntegerOrInfinity truncation,
// RangeError only for negative or > 2^53-1.  (Bit counts past our
// digit cap RangeError later in the op itself.)
static uint64_t
ToIndexBits (ejsval v)
{
    if (EJSVAL_IS_UNDEFINED(v)) return 0;
    double d = ToDouble(v);
    if (isnan(d)) return 0;
    d = trunc(d);
    if (d < 0 || d > 9007199254740991.0)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "Invalid value for bits");
    if (d > 4294967295.0)
        _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "BigInt too big");
    return (uint64_t)d;
}

static EJS_NATIVE_FUNC(_ejs_BigInt_asIntN) {
    uint64_t bits = ToIndexBits(argc > 0 ? args[0] : _ejs_undefined);
    ejsval bi = ToBigInt(argc > 1 ? args[1] : _ejs_undefined);
    return _ejs_bigint_as_intn((uint32_t)bits, bi);
}

static EJS_NATIVE_FUNC(_ejs_BigInt_asUintN) {
    uint64_t bits = ToIndexBits(argc > 0 ? args[0] : _ejs_undefined);
    ejsval bi = ToBigInt(argc > 1 ? args[1] : _ejs_undefined);
    return _ejs_bigint_as_uintn((uint32_t)bits, bi);
}

// the [[BigIntData]] of the receiver (primitive or wrapper), else TypeError
static ejsval
thisBigIntValue (ejsval value, const char* who)
{
    if (EJSVAL_IS_BIGINT(value)) return value;
    if (EJSVAL_IS_BIGINT_OBJECT(value)) return ((EJSBigIntObject*)EJSVAL_TO_OBJECT(value))->primBigInt;
    char buf[128];
    snprintf(buf, sizeof(buf), "%s called with non-BigInt this", who);
    _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, buf);
}

static EJS_NATIVE_FUNC(_ejs_BigInt_prototype_toString) {
    ejsval bi = thisBigIntValue(*_this, "BigInt.prototype.toString");
    int radix = 10;
    if (argc > 0 && !EJSVAL_IS_UNDEFINED(args[0])) {
        double r = ToDouble(args[0]);
        if (r < 2 || r > 36 || trunc(r) != r)
            _ejs_throw_nativeerror_utf8 (EJS_RANGE_ERROR, "toString() radix must be between 2 and 36");
        radix = (int)r;
    }
    return _ejs_bigint_to_ejs_string(bi, radix);
}

static EJS_NATIVE_FUNC(_ejs_BigInt_prototype_toLocaleString) {
    ejsval bi = thisBigIntValue(*_this, "BigInt.prototype.toLocaleString");
    return _ejs_bigint_to_ejs_string(bi, 10);
}

static EJS_NATIVE_FUNC(_ejs_BigInt_prototype_valueOf) {
    return thisBigIntValue(*_this, "BigInt.prototype.valueOf");
}

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_BigInt, x, _ejs_BigInt_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS(_ejs_BigInt_prototype, x, _ejs_BigInt_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

void
_ejs_bigint_init (ejsval global)
{
    processor = Processor::New(new DefaultPlatform());

    _ejs_BigInt = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_BigInt, _ejs_BigInt_impl);
    _ejs_object_setprop (global, _ejs_atom_BigInt, _ejs_BigInt);

    _ejs_gc_add_root (&_ejs_BigInt_prototype);
    _ejs_BigInt_prototype = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_define_value_property (_ejs_BigInt, _ejs_atom_prototype, _ejs_BigInt_prototype,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_NOT_CONFIGURABLE);
    _ejs_object_define_value_property (_ejs_BigInt_prototype, _ejs_atom_constructor, _ejs_BigInt,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE);

    OBJ_METHOD(asIntN);
    OBJ_METHOD(asUintN);

    PROTO_METHOD(toString);
    PROTO_METHOD(toLocaleString);
    PROTO_METHOD(valueOf);

    _ejs_object_define_value_property (_ejs_BigInt_prototype, _ejs_Symbol_toStringTag, _ejs_atom_BigInt,
                                       EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);
}

static EJSObject*
_ejs_bigint_specop_allocate ()
{
    return (EJSObject*)_ejs_gc_new (EJSBigIntObject);
}

static void
_ejs_bigint_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    scan_func(&(((EJSBigIntObject*)obj)->primBigInt));
    _ejs_Object_specops.Scan(obj, scan_func);
}

// EJS_DEFINE_CLASS's OP_INHERIT is (void*)-1, which C++ won't
// implicitly convert to the typed slots — spell the casts out
#define OP_INHERIT_AS(T) ((T)(void*)-1)
EJSSpecOps _ejs_BigInt_specops = {
    .class_name = "BigInt",
    .GetPrototypeOf = OP_INHERIT_AS(SpecOpGetPrototypeOf),
    .SetPrototypeOf = OP_INHERIT_AS(SpecOpSetPrototypeOf),
    .IsExtensible = OP_INHERIT_AS(SpecOpIsExtensible),
    .PreventExtensions = OP_INHERIT_AS(SpecOpPreventExtensions),
    .GetOwnProperty = OP_INHERIT_AS(SpecOpGetOwnProperty),
    .DefineOwnProperty = OP_INHERIT_AS(SpecOpDefineOwnProperty),
    .HasProperty = OP_INHERIT_AS(SpecOpHasProperty),
    .Get = OP_INHERIT_AS(SpecOpGet),
    .Set = OP_INHERIT_AS(SpecOpSet),
    .Delete = OP_INHERIT_AS(SpecOpDelete),
    .Enumerate = OP_INHERIT_AS(SpecOpEnumerate),
    .OwnPropertyKeys = OP_INHERIT_AS(SpecOpOwnPropertyKeys),
    .Call = OP_INHERIT_AS(SpecOpCall),
    .Construct = OP_INHERIT_AS(SpecOpConstruct),
    .Allocate = _ejs_bigint_specop_allocate,
    .Finalize = OP_INHERIT_AS(SpecOpFinalize),
    .Scan = _ejs_bigint_specop_scan,
};
