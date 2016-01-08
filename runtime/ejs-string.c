/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>

#include "ejs-generator.h"
#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-string.h"
#include "ejs-function.h"
#include "ejs-regexp.h"
#include "ejs-ops.h"
#include "ejs-string.h"
#include "ejs-error.h"
#include "ejs-symbol.h"
#include "ejs-number.h"

// length below which we eschew creating a rope and just create a flat string containing both primstrs
#define FLAT_ROPE_THRESHOLD 32

// length below which we eschew creating a dependent string and just create a flat string containing the slice
#define FLAT_DEP_THRESHOLD 32

int32_t
ucs2_strcmp (const jschar *s1, const jschar *s2)
{
    if (s1 == s2) return 0;

    while (*s1 == *s2++) {
        if (*s1++ == 0)
            return 0;
    }

    return ((int32_t)*s1) - ((int32_t)*(s2 - 1));
}

jschar*
ucs2_strdup (const jschar *str)
{
    int32_t len = ucs2_strlen(str);
    jschar* result = (jschar*)malloc(sizeof(jschar) * (len + 1));
    memmove (result, str, len * sizeof(jschar));
    result[len] = 0;
    return result;
}

int32_t
ucs2_strlen (const jschar *str)
{
    int32_t rv = 0;
    while (*str++) rv++;
    return rv;
}

uint32_t
ucs2_hash (const jschar* str, int32_t hash, int length)
{
    const char* p = (const char*)str;

    while (length >= 0) {
        hash = (hash << 5) - hash * (*p++);
        hash = (hash << 5) - hash + (*p++);
        length --;
    }

    return (uint32_t)hash;
}

jschar*
ucs2_strstr (const jschar *haystack,
             const jschar *needle)
{
    const jschar *p = haystack;

    while (*p) {
        const jschar *next_candidate = NULL;

        if (*p == *needle) {
            const jschar *p2 = p+1;
            const jschar *n = needle+1;

            if (!next_candidate && *p2 == *needle)
                next_candidate = p2;

            while (*n) {
                if (*n != *p2)
                    break;
                n++;
                p2++;
                if (!next_candidate && *p2 == *needle)
                    next_candidate = p2;
            }
            if (*n == 0)
                return (jschar*)p;

            if (next_candidate)
                p = next_candidate;
            else
                p++;
            continue;
        }
        else {
            if (next_candidate)
                p = next_candidate;
            else
                p++;
        }
    }

    return NULL;
}

jschar*
ucs2_strrstr (const jschar *haystack,
              const jschar *needle)
{
    int haystack_len = ucs2_strlen(haystack);
    int needle_len   = ucs2_strlen(needle);

    if (needle_len > haystack_len)
        return NULL;

    const jschar* p        = haystack + haystack_len - 1;
    const jschar* needle_p = needle   + needle_len   - 1;

    while (p >= haystack) {
        const jschar *next_candidate = NULL;

        if (*p == *needle_p) {
            const jschar *p2 = p-1;
            const jschar *n  = needle_p-1;

            if (!next_candidate && *p2 == *needle_p)
                next_candidate = p2;

            while (n >= needle) {
                if (*n != *p2)
                    break;
                n--;
                p2--;
                if (!next_candidate && *p2 == *needle_p)
                    next_candidate = p2;
            }
            if (n < needle)
                return (jschar*)p2+1;

            if (next_candidate)
                p = next_candidate;
            else
                p = p2-1;
            continue;
        }
        else {
            if (next_candidate)
                p = next_candidate;
            else
                p--;
        }
    }

    return NULL;
}


static jschar
utf8_to_ucs2 (const unsigned char * input, const unsigned char ** end_ptr)
{
    *end_ptr = input;
    if (input[0] == 0)
        return -1;
    if (input[0] < 0x80) {
        * end_ptr = input + 1;
        return input[0];
    }
    if ((input[0] & 0xE0) == 0xE0) {
        if (input[1] == 0 || input[2] == 0)
            return -1;
        * end_ptr = input + 3;
        return
            (input[0] & 0x0F)<<12 |
            (input[1] & 0x3F)<<6  |
            (input[2] & 0x3F);
    }
    if ((input[0] & 0xC0) == 0xC0) {
        if (input[1] == 0)
            return -1;
        * end_ptr = input + 2;
        return
            (input[0] & 0x1F)<<6  |
            (input[1] & 0x3F);
    }
    return -1;
}

static int
utf16_to_utf8_char (const jschar* utf16, char* utf8, int *utf16_adv)
{
    jschar ucs2;

    ucs2 = *utf16;
    *utf16_adv = 1;

    if (ucs2 < 0x80) {
        utf8[0] = (char)ucs2;
        return 1;
    }
    if (ucs2 >= 0x80  && ucs2 < 0x800) {
        utf8[0] = (ucs2 >> 6)   | 0xC0;
        utf8[1] = (ucs2 & 0x3F) | 0x80;
        return 2;
    }
    if (ucs2 >= 0x800 && ucs2 <= 0xFFFF) {
        if (ucs2 >= 0xD800 && ucs2 <= 0xDFFF) {
            // surrogate pair
            jschar ucs2_2 = *(utf16 + 1);
            uint32_t combined = 0x10000 + (((ucs2 - 0xD800) << 10) | (ucs2_2 - 0xDC00));

            utf8[0] = 0xF0 | (combined >> 18);
            utf8[1] = 0x80 | ((combined >> 12) & 0x3F);
            utf8[2] = 0x80 | ((combined >> 6) & 0x3F);
            utf8[3] = 0x80 | ((combined & 0x3F));

            *utf16_adv = 2;
            return 4;
        }

        utf8[0] = ((ucs2 >> 12)       ) | 0xE0;
        utf8[1] = ((ucs2 >> 6 ) & 0x3F) | 0x80;
        utf8[2] = ((ucs2      ) & 0x3F) | 0x80;
        return 3;
    }
    EJS_NOT_IMPLEMENTED();
}

int
ucs2_to_utf8_char (jschar ucs2, char *utf8)
{
    if (ucs2 < 0x80) {
        utf8[0] = (char)ucs2;
        return 1;
    }
    if (ucs2 >= 0x80  && ucs2 < 0x800) {
        utf8[0] = (ucs2 >> 6)   | 0xC0;
        utf8[1] = (ucs2 & 0x3F) | 0x80;
        return 2;
    }
    if (ucs2 >= 0x800 && ucs2 < 0xFFFF) {
        if (ucs2 >= 0xD800 && ucs2 <= 0xDFFF) {
            EJS_NOT_IMPLEMENTED();
        }

        utf8[0] = ((ucs2 >> 12)       ) | 0xE0;
        utf8[1] = ((ucs2 >> 6 ) & 0x3F) | 0x80;
        utf8[2] = ((ucs2      ) & 0x3F) | 0x80;
        return 3;
    }
    EJS_NOT_IMPLEMENTED();
}

char*
ucs2_to_utf8 (const jschar *str)
{
    int len = ucs2_strlen(str);
    char *conservative = (char*)calloc (sizeof(char), len * 4 + 1);

    const jschar *p = str;
    char *c = conservative;
    int utf16_adv;

    while (*p) {
        int adv = utf16_to_utf8_char (p, c, &utf16_adv);
        if (adv < 1) {
            // more here XXX
            break;
        }
        c += adv;

        p += utf16_adv;
        len --;
        if (len == 0)
            break;
    }

    *c = 0;
    return conservative;
}

char*
ucs2_to_utf8_buf (const jschar *str, char* buf, size_t buf_size)
{
    int len = ucs2_strlen(str);
    if (len > buf_size) // easy check right off the bat
        return NULL;

    const jschar *p = str;
    char *c = buf;

    while (*p) {
        int adv = ucs2_to_utf8_char (*p, c);
        if (adv < 1) {
            // more here XXX
            break;
        }
        c += adv;
        if (c - buf > buf_size)
            return NULL;

        p++;
        len --;
        if (len == 0)
            break;
    }

    *c = 0;
    return buf;
}


ejsval _ejs_String EJSVAL_ALIGNMENT;
ejsval _ejs_String__proto__ EJSVAL_ALIGNMENT;
ejsval _ejs_String_prototype EJSVAL_ALIGNMENT;

static ejsval thisStringValue(ejsval value) {
    // 1. If Type(value) is String, return value.
    if (EJSVAL_IS_STRING(value)) return value;

    // 2. If Type(value) is Object and value has a [[StringData]] internal slot, then
    // a. Assert: value’s [[StringData]] internal slot is a String value.
    // b. Return the value of value’s [[StringData]] internal slot.
    if (EJSVAL_IS_STRING_OBJECT(value)) {
        return ((EJSString*)EJSVAL_TO_OBJECT(value))->primStr;
    }

    // 3. Throw a TypeError exception.
    _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "value is not a string");
}


// ES2015, June 2015
// 21.1.1.1 String ( value )
static EJS_NATIVE_FUNC(_ejs_String_impl) {
    ejsval s;

    // 1. If no arguments were passed to this function invocation, let s be "".
    if (argc == 0)
        s = _ejs_atom_empty;
    // 2. Else,
    else {
        // a. If NewTarget is undefined and Type(value) is Symbol, return SymbolDescriptiveString(value).
        if (EJSVAL_IS_UNDEFINED(newTarget) && EJSVAL_IS_SYMBOL(args[0]))
            EJS_NOT_IMPLEMENTED();

        // b. Let s be ToString(value).
        s = ToString(args[0]);
    }
    // 3. ReturnIfAbrupt(s).

    // 4. If NewTarget is undefined, return s.
    if (EJSVAL_IS_UNDEFINED(newTarget))
        return s;

    // 5. Return StringCreate(s, GetPrototypeFromConstructor(NewTarget, "%StringPrototype%")).
    ejsval O = OrdinaryCreateFromConstructor(newTarget, _ejs_String_prototype, &_ejs_String_specops);
    *_this = O;

    EJSString* O_ = (EJSString*)EJSVAL_TO_OBJECT(O);
    O_->primStr = s;

    return O;
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_toString) {
    ejsval s = thisStringValue(*_this);
    return s;
}

// ES6 21.1.3.14.1
ejsval
GetReplaceSubstitution(ejsval matched, ejsval string, int position, ejsval captures, ejsval replacement)
{
    // 1. Assert: Type(matched) is String.
    EJS_ASSERT(EJSVAL_IS_STRING_TYPE(matched));

    // 2. Let matchLength be the number of code units in matched.
    int matchLength = EJSVAL_TO_STRLEN(matched);

    // 3. Assert: Type(string) is String.
    EJS_ASSERT(EJSVAL_IS_STRING_TYPE(string));

    // 4. Let stringLength be the number of code units in string.
    int stringLength = EJSVAL_TO_STRLEN(string);

    // 5. Assert: position is a nonnegative integer.
    // 6. Assert: position ≤ stringLength.
    EJS_ASSERT(position <= stringLength);

    // 7. Assert: captures is a possibly empty List of Strings.
    EJS_ASSERT(EJSVAL_IS_ARRAY(captures));

    // 8. Assert:Type(replacement) is String
    EJS_ASSERT(EJSVAL_IS_STRING_TYPE(replacement));

    // 9. Let tailPos be position + matchLength.
    int tailPos = position + matchLength;

    // 10. Let m be the number of elements in captures.
    int m = EJS_ARRAY_LEN(captures);

    // 11. Let result be a String value derived from replacement by copying code unit elements from
    //     replacement to result while performing replacements as specified in Table 42. These $ replacements
    //     are done left-to-right, and, once such a replacement is performed, the new replacement text is not
    //     subject to further replacements.
    jschar* replacement_p = EJSVAL_TO_FLAT_STRING(replacement);
    int replacement_len = EJSVAL_TO_STRLEN(replacement);

    int result_len = 0;

    // 2 passes.  first pass calculates the length, the second pass builds the string
    for (int i = 0; i < replacement_len; i ++) {
        if (replacement_p[i] == '$') {
            switch (replacement_p[i+1]) {
            case '$': // $$
                result_len ++;
                i ++;
                break;
            case '&':
                result_len += matchLength;
                i ++;
                break;
            case '\'':
                if (tailPos < stringLength)
                    result_len += stringLength - tailPos;
                i ++;
                break;
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
                int n = replacement_p[i+1] - '0';
                if (i < replacement_len - 2 && isdigit(replacement_p[i+2])) {
                    n = n * 10 + (replacement_p[i+2] - '0');
                    if (n == 0 || n > m)
                        ; // empty string goes here, so result_len doesn't change
                    else {
                        ejsval captured_str = Get(captures, NUMBER_TO_EJSVAL(n));
                        if (!EJSVAL_IS_UNDEFINED(captured_str))
                            result_len += EJSVAL_TO_STRLEN(captured_str);
                    }
                }
                else {
                    if (n == 0)
                        result_len ++;
                }
                break;
            }
            default:
                result_len ++;
            }
        }
        else
            result_len ++;
    }

    jschar* result_buf = (jschar*)malloc(sizeof(jschar) * (result_len + 1));
    jschar* result_p = result_buf;

    for (int i = 0; i < replacement_len; i ++) {
        if (replacement_p[i] == '$') {
            switch (replacement_p[i+1]) {
            case '$': // $$
                *result_p++ = '$';
                i ++;
                break;
            case '&':
                memmove (result_p, EJSVAL_TO_FLAT_STRING(matched), matchLength*sizeof(jschar));
                result_p += matchLength;
                i ++;
                break;
            case '\'':
                if (tailPos < stringLength) {
                    memmove(result_p, EJSVAL_TO_FLAT_STRING(string) + tailPos, (stringLength - tailPos) * sizeof(jschar));
                    result_p += stringLength - tailPos;
                }
                i ++;
                break;
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
                int n = replacement_p[i+1] - '0';
                if (i < replacement_len - 2 && isdigit(replacement_p[i+2])) {
                    n = n * 10 + (replacement_p[i+2] - '0');
                    if (n == 0 || n > m) {
                        ; // empty string goes here, so result doesn't change
                        // but skip over the number
                        i++;
                        if (n > 10)
                            i++;
                        break;
                    }
                }
                else {
                    if (n == 0) {
                        *result_p++ = '$';
                        *result_p++ = '0';
                        break;
                    }
                }

                ejsval captured_str = EJS_DENSE_ARRAY_ELEMENTS(captures)[n-1];
                if (!EJSVAL_IS_UNDEFINED(captured_str)) {
                    memmove(result_p, EJSVAL_TO_FLAT_STRING(captured_str), EJSVAL_TO_STRLEN(captured_str) * sizeof(jschar));
                    result_p += EJSVAL_TO_STRLEN(captured_str);
                }

                i++;
                if (n > 10)
                    i++;

                break;
            }
            default:
                *result_p++ = '$';
            }
        }
        else
            *result_p++ = replacement_p[i];
    }
    *result_p = 0;

    ejsval result = _ejs_string_new_ucs2(result_buf);
    free(result_buf);

    // 12. Return result.
    return result;
}

// ES6 21.1.3.14
// String.prototype.replace (searchValue, replaceValue )
static EJS_NATIVE_FUNC(_ejs_String_prototype_replace) {
    ejsval searchValue = _ejs_undefined;
    if (argc > 0) searchValue = args[0];

    ejsval replaceValue = _ejs_undefined;
    if (argc > 1) replaceValue = args[1];

    // 1. Let O be RequireObjectCoercible(this value).
    ejsval O = *_this;

    // 2. Let string be ToString(O).
    // 3. ReturnIfAbrupt(string).
    ejsval string = ToString(O);
    
    // 4. Let replacer be GetMethod(searchValue, @@replace).
    // 5. ReturnIfAbrupt(replacer).
    ejsval replacer = GetMethod(searchValue, _ejs_Symbol_replace);

    // 6. If replacer is not undefined, then
    if (!EJSVAL_IS_UNDEFINED(replacer)) {
        //    a. Return Call(replacer, searchValue, «string, replaceValue»).
        ejsval call_args[2] = { string, replaceValue };
        return _ejs_invoke_closure(replacer, &searchValue, 2, call_args, _ejs_undefined);
    }
    // 7. Let searchString be ToString(searchValue).
    // 8. ReturnIfAbrupt(searchString).
    ejsval searchString = ToString(searchValue);

    // 9. Let functionalReplace be IsCallable(replaceValue).
    EJSBool functionalReplace = EJSVAL_IS_FUNCTION(replaceValue);

    // 10. If functionReplace is false, then
    if (!functionalReplace) {
        // a. Let replaceValue be ToString(replaceValue).
        // b. ReturnIfAbrupt(replaceValue).
        replaceValue = ToString(replaceValue);
    }
    // 11. Search string for the first occurrence of searchString and let pos be the index within string
    //     of the first code unit of the matched substring and let matched be searchString. If no occurrences of
    //     searchString were found, return string.

    jschar* string_cstr = EJSVAL_TO_FLAT_STRING(string);

    jschar* p = ucs2_strstr(string_cstr, EJSVAL_TO_FLAT_STRING(searchString));
    if (!p)
        return string;

    ejsval matched = searchString;
    int pos = p - string_cstr;

    ejsval replStr;

    // 12. If functionalReplace is true, then
    if (functionalReplace) {
        // a. Let replValue be Call(replaceValue, undefined,«matched, pos, and string»).
        ejsval call_args[3] = { matched, NUMBER_TO_EJSVAL(pos), string };
        ejsval undef_this = _ejs_undefined;
        ejsval replValue = _ejs_invoke_closure(replaceValue, &undef_this, 3, call_args, _ejs_undefined);

        // b. Let replStr be ToString(replValue).
        // c. ReturnIfAbrupt(replStr).
        replStr = ToString(replValue);
    }
    // 13. Else,
    else {
        // a. Let captures be an empty List.
        ejsval captures = _ejs_array_new(0, EJS_FALSE);
        // b. Let replStr be GetReplaceSubstitution(matched, string, pos, captures, replaceValue).
        replStr = GetReplaceSubstitution(matched, string, pos, captures, replaceValue);
    }
    // 14. Let tailPos be pos + the number of code units in matched.
    int tailPos = pos + EJSVAL_TO_STRLEN(matched);

    // 15. Let newString be the String formed by concatenating the first pos code units of string, replStr, and
    //     the trailing substring of string starting at index tailPos. If pos is 0, the first element of the
    //     concatenation will be the empty String.
    ejsval newString = _ejs_string_concatv(pos == 0 ? _ejs_atom_empty : _ejs_string_new_substring(string, 0, pos),
                                           replStr,
                                           tailPos == EJSVAL_TO_STRLEN(string)-1 ? _ejs_atom_empty :  _ejs_string_new_substring(string, tailPos, EJSVAL_TO_STRLEN(string)-tailPos),
                                           _ejs_undefined);

    // 16. Return newString.
    return newString;
}

jschar
_ejs_string_ucs2_at (EJSPrimString* primstr, uint32_t offset)
{
    switch (EJS_PRIMSTR_GET_TYPE(primstr)) {
    case EJS_STRING_DEPENDENT:
        return _ejs_string_ucs2_at (primstr->data.dependent.dep, offset + primstr->data.dependent.off);
    case EJS_STRING_ROPE:
        if (offset < primstr->data.rope.left->length) {
            return _ejs_string_ucs2_at (primstr->data.rope.left, offset);
        }
        else {
            return _ejs_string_ucs2_at (primstr->data.rope.right, offset - primstr->data.rope.left->length);
        }
    case EJS_STRING_FLAT:
        // the character is in this flat string
        return primstr->data.flat[offset];
    default:
        EJS_NOT_IMPLEMENTED();
    }
}


static EJS_NATIVE_FUNC(_ejs_String_prototype_charAt) {
    ejsval primStr;

    if (EJSVAL_IS_STRING(*_this)) {
        primStr = *_this;
    }
    else {
        EJSString *str = (EJSString*)EJSVAL_TO_OBJECT(*_this);
        primStr = str->primStr;
    }

    int idx = 0;
    if (argc > 0 && EJSVAL_IS_NUMBER(args[0])) {
        idx = (int)EJSVAL_TO_NUMBER(args[0]);
    }

    if (idx < 0 || idx >= EJSVAL_TO_STRLEN(primStr))
        return _ejs_atom_empty;

    jschar c = _ejs_string_ucs2_at(EJSVAL_TO_STRING(primStr), idx);
    return _ejs_string_new_ucs2_len (&c, 1);
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_charCodeAt) {
    ejsval primStr;

    if (EJSVAL_IS_STRING(*_this)) {
        primStr = *_this;
    }
    else {
        EJSString *str = (EJSString*)EJSVAL_TO_OBJECT(*_this);
        primStr = str->primStr;
    }

    int idx = 0;
    if (argc > 0 && EJSVAL_IS_NUMBER(args[0])) {
        idx = (int)EJSVAL_TO_NUMBER(args[0]);
    }

    if (idx < 0 || idx >= EJSVAL_TO_STRLEN(primStr))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL (_ejs_string_ucs2_at(EJSVAL_TO_STRING(primStr), idx));
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_concat) {
    EJS_NOT_IMPLEMENTED();
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_indexOf) {
    int idx = -1;
    if (argc == 0)
        return NUMBER_TO_EJSVAL(idx);

    ejsval haystack = ToString(*_this);
    jschar* haystack_cstr;
    if (EJSVAL_IS_STRING(haystack)) {
        haystack_cstr = EJSVAL_TO_FLAT_STRING(haystack);
    }
    else {
        haystack_cstr = EJSVAL_TO_FLAT_STRING(((EJSString*)EJSVAL_TO_OBJECT(haystack))->primStr);
    }

    ejsval needle = ToString(args[0]);
    jschar *needle_cstr;
    if (EJSVAL_IS_STRING(needle)) {
        needle_cstr = EJSVAL_TO_FLAT_STRING(needle);
    }
    else {
        needle_cstr = EJSVAL_TO_FLAT_STRING(((EJSString*)EJSVAL_TO_OBJECT(needle))->primStr);
    }
  
    jschar* p = ucs2_strstr(haystack_cstr, needle_cstr);
    if (p == NULL)
        return NUMBER_TO_EJSVAL(idx);

    return NUMBER_TO_EJSVAL (p - haystack_cstr);
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_lastIndexOf) {
    int idx = -1;
    if (argc == 0)
        return NUMBER_TO_EJSVAL(idx);

    ejsval haystack = ToString(*_this);
    jschar* haystack_cstr;
    if (EJSVAL_IS_STRING(haystack)) {
        haystack_cstr = EJSVAL_TO_FLAT_STRING(haystack);
    }
    else {
        haystack_cstr = EJSVAL_TO_FLAT_STRING(((EJSString*)EJSVAL_TO_OBJECT(haystack))->primStr);
    }

    ejsval needle = ToString(args[0]);
    jschar *needle_cstr;
    if (EJSVAL_IS_STRING(needle)) {
        needle_cstr = EJSVAL_TO_FLAT_STRING(needle);
    }
    else {
        needle_cstr = EJSVAL_TO_FLAT_STRING(((EJSString*)EJSVAL_TO_OBJECT(needle))->primStr);
    }
  
    jschar* p = ucs2_strrstr(haystack_cstr, needle_cstr);
    if (p == NULL)
        return NUMBER_TO_EJSVAL(idx);

    return NUMBER_TO_EJSVAL (p - haystack_cstr);
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_localeCompare) {
    EJS_NOT_IMPLEMENTED();
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_match) {
    ejsval regexp = _ejs_undefined;
    if (argc > 0) regexp = args[0];

    // 1. Let O be RequireObjectCoercible(this value).
    ejsval O = *_this;

    // 2. Let S be ToString(O).
    // 3. ReturnIfAbrupt(S).
    ejsval S = ToString(O);

    // 4. Let matcher be GetMethod(O, @@match).
    // 5. ReturnIfAbrupt(matcher).
    ejsval matcher = GetMethod(regexp, _ejs_Symbol_match);

    // 6. If matcher is not undefined, then
    if (!EJSVAL_IS_UNDEFINED(matcher))
        // a. Return Call(matcher, regexp, «S»).
        return _ejs_invoke_closure(matcher, &regexp, 1, &S, _ejs_undefined);
    
    // 7. Let rx be the result of the abstract operation RegExpCreate(regexp, undefined) (see 21.2.3.3)
    ejsval rx = _ejs_regexp_new(regexp, _ejs_undefined);

    // 8. Return Invoke(rx, @@match, «S»).
    return _ejs_invoke_closure (Get(rx, _ejs_Symbol_match), &rx, 1, &S, _ejs_undefined);
}

// ECMA262: 21.1.3.15 String.prototype.search ( regexp )
static EJS_NATIVE_FUNC(_ejs_String_prototype_search) {
    ejsval regexp = _ejs_undefined;
    if (argc > 0) regexp = args[0];

    // 1. Let O be RequireObjectCoercible(this value).
    ejsval O = *_this;

    // 2. Let string be ToString(O).
    // 3. ReturnIfAbrupt(string).
    ejsval string = ToString(O);

    // 4. Let searcher be GetMethod(regexp, @@search).
    // 5. ReturnIfAbrupt(searcher).
    ejsval searcher = GetMethod(regexp, _ejs_Symbol_search);

    // 6. If searcher is not undefined , then,
    if (!EJSVAL_IS_UNDEFINED(searcher))
        //    a. Return Call(searcher, regexp, «string»)
        return _ejs_invoke_closure(searcher, &regexp, 1, &string, _ejs_undefined);

    // 7. Let rx be RegExpCreate(regexp, undefined) (see 21.2.3.3).
    // 8. ReturnIfAbrupt(rx).
    ejsval rx = _ejs_regexp_new(regexp, _ejs_undefined);

    // 9. Return Invoke(rx, @@search, «string»).
    return _ejs_invoke_closure (Get(rx, _ejs_Symbol_search), &rx, 1, &string, _ejs_undefined);
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_substring) {
    /* 1. Call CheckObjectCoercible passing the this value as its argument. */
    if (EJSVAL_IS_NULL_OR_UNDEFINED(*_this))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "String.prototype.subString called on null or undefined");

    ejsval start = _ejs_undefined;
    ejsval end = _ejs_undefined;

    if (argc > 0) start = args[0];
    if (argc > 1) end = args[1];

    /* 2. Let S be the result of calling ToString, giving it the this value as its argument. */
    ejsval S = ToString(*_this);

    /* 3. Let len be the number of characters in S. */
    int len = EJSVAL_TO_STRLEN(S);

    /* 4. Let intStart be ToInteger(start). */
    int32_t intStart = ToInteger(start);

    /* 5. If end is undefined, let intEnd be len; else let intEnd be ToInteger(end). */
    int32_t intEnd = (EJSVAL_IS_UNDEFINED(end)) ? len : ToInteger(end);
        
    /* 6. Let finalStart be min(max(intStart, 0), len). */
    int32_t finalStart = MIN(MAX(intStart, 0), len);

    /* 7. Let finalEnd be min(max(intEnd, 0), len). */
    int32_t finalEnd = MIN(MAX(intEnd, 0), len);

    /* 8. Let from be min(finalStart, finalEnd). */
    int32_t from = MIN(finalStart, finalEnd);

    /* 9. Let to be max(finalStart, finalEnd). */
    int32_t to = MAX(finalStart, finalEnd);

    /* 10. Return a String whose length is to - from, containing characters from S, namely the characters with indices  */
    /*     from through to-1, in ascending order. */
    return _ejs_string_new_substring (S, from, to-from);
}

// ECMA262: B.2.3
static EJS_NATIVE_FUNC(_ejs_String_prototype_substr) {
    ejsval start = _ejs_undefined;
    ejsval length = _ejs_undefined;

    if (argc > 0) start = args[0];
    if (argc > 1) length = args[1];

    /* 1. Call ToString, giving it the this value as its argument. */
    ejsval Result1 = ToString(*_this);

    /* 2. Call ToInteger(start). */
    int32_t Result2 = ToInteger(start);

    /* 3. If length is undefined, use +inf; otherwise call ToInteger(length). */
    int32_t Result3;
    if (EJSVAL_IS_UNDEFINED(length)) {
        EJS_NOT_IMPLEMENTED();
    }
    else {
        Result3 = ToInteger(length);
    }

    /* 4. Compute the number of characters in Result(1). */
    int32_t Result4 = EJSVAL_TO_STRLEN(Result1);

    /* 5. If Result(2) is positive or zero, use Result(2); else use max(Result(4)+Result(2),0). */
    int32_t Result5 = Result2 >= 0 ? Result2 : MAX(Result4+Result2, 0);
        
    /* 6. Compute min(max(Result(3),0), Result(4)–Result(5)). */
    int32_t Result6 = MIN(MAX(Result3, 0), Result4 - Result5);
    
    /* 7. If Result(6) <= 0, return the empty String "". */
    if (Result6 <= 0) return _ejs_atom_empty;

    /* 8. Return a String containing Result(6) consecutive characters from Result(1) beginning with the character at  */
    /*    position Result(5). */
    return _ejs_string_new_substring (Result1, Result5, Result6);
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_toLowerCase) {
    /* 1. Call CheckObjectCoercible passing the this value as its argument. */
    /* 2. Let S be the result of calling ToString, giving it the this value as its argument. */
    ejsval S = ToString(*_this);
    char* sstr = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(S));

    /* 3. Let L be a String where each character of L is either the Unicode lowercase equivalent of the corresponding  */
    /*    character of S or the actual corresponding character of S if no Unicode lowercase equivalent exists. */
    char* p = sstr;
    while (*p) {
        *p = tolower(*p);
        p++;
    }

    ejsval L = _ejs_string_new_utf8(sstr);
    free(sstr);

    /* 4. Return L. */
    return L;
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_toLocaleLowerCase) {
    EJS_NOT_IMPLEMENTED();
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_toUpperCase) {
    /* 1. Call CheckObjectCoercible passing the this value as its argument. */
    /* 2. Let S be the result of calling ToString, giving it the this value as its argument. */
    ejsval S = ToString(*_this);
    char* sstr = ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(S));

    /* 3. Let L be a String where each character of L is either the Unicode lowercase equivalent of the corresponding  */
    /*    character of S or the actual corresponding character of S if no Unicode lowercase equivalent exists. */
    char* p = sstr;
    while (*p) {
        *p = toupper(*p);
        p++;
    }

    ejsval L = _ejs_string_new_utf8(sstr);
    free(sstr);

    /* 4. Return L. */
    return L;
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_toLocaleUpperCase) {
    EJS_NOT_IMPLEMENTED();
}

static EJSBool
IsWhitespace(jschar c) {

    switch (c) {
        // ES6 11.3: Line Terminators
    case 0x000A: // U+000A Line Feed
    case 0x000D: // U+000D Carriage Return
    case 0x2028: // U+2028 Line separator
    case 0x2029: // U+2029 Paragraph separator

        // ES6 11.2: White Space
    case 0x0009: // U+0009 Character Tabulation
    case 0x000B: // U+000B LINE TABULATION
    case 0x000C: // U+000C Form Feed (ff)
    case 0x0020: // U+0020 Space
    case 0x00A0: // U+00A0 No-break space
    case 0xFEFF: // U+FEFF ZERO wIDTH nO-bREAK SPACE

        // Unicode Category Zs
    // up in 11.2 case 0x0020: // U+0020 SPACE
    // up in 11.2 case 0x00A0: // U+00A0 NO-BREAK SPACE
    case 0x1680: // U+1680 OGHAM SPACE MARK
    case 0x2000: // U+2000 EN QUAD
    case 0x2001: // U+2001 EM QUAD
    case 0x2002: // U+2002 EN SPACE
    case 0x2003: // U+2003 EM SPACE
    case 0x2004: // U+2004 THREE-PER-EM SPACE
    case 0x2005: // U+2005 FOUR-PER-EM SPACE
    case 0x2006: // U+2006 SIX-PER-EM SPACE
    case 0x2007: // U+2007 FIGURE SPACE
    case 0x2008: // U+2008 PUNCTUATION SPACE
    case 0x2009: // U+2009 THIN SPACE
    case 0x200A: // U+200A HAIR SPACE
    case 0x202F: // U+202F NARROW NO-BREAK SPACE
    case 0x205F: // U+205F MEDIUM MATHEMATICAL SPACE
    case 0x3000: // U+3000 IDEOGRAPHIC SPACE
        return EJS_TRUE;
    default:
        return EJS_FALSE;
    }
}

// ES6 21.1.3.25
// String.prototype.trim ()
static EJS_NATIVE_FUNC(_ejs_String_prototype_trim) {
    // 1. Let O be RequireObjectCoercible(this value).
    if (EJSVAL_IS_UNDEFINED(*_this) || EJSVAL_IS_NULL(*_this)) {
    }
    ejsval O = *_this;

    // 2. Let S be ToString(O).
    // 3. ReturnIfAbrupt(S).
    ejsval S = ToString(O);

    // 4. Let T be a String value that is a copy of S with both
    //    leading and trailing white space removed. The definition of
    //    white space is the union of WhiteSpace and
    //    LineTerminator. When determining whether a Unicode code
    //    point is in Unicode general category “Zs”, code unit
    //    sequences are interpreted as UTF-16 encoded code point
    //    sequences as specified in 6.1.4.
    EJSPrimString* flat = _ejs_string_flatten(S);

    int leading = 0;
    while (leading < flat->length && IsWhitespace(flat->data.flat[leading])) {
        leading++;
    }
    if (leading == flat->length) return _ejs_atom_empty;

    int trailing = 0;
    while (IsWhitespace(flat->data.flat[flat->length - 1 - trailing])) {
        trailing ++;
    }

    if (leading == 0 && trailing == 0)
        return S;

    ejsval T = _ejs_string_new_substring(S, leading, flat->length - leading - trailing);

    // 5. Return T.
    return T;
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_valueOf) {
    // 1. Let s be thisStringValue(this value).
    ejsval s = thisStringValue(*_this);

    // 2. Return s.
    return s;
}

// ECMA262: 15.5.4.14
typedef enum {
    MATCH_RESULT_FAILURE,
    MATCH_RESULT_SUCCESS
} MatchResultType;

typedef struct {
    MatchResultType type;
    int endIndex;
    ejsval captures;
} MatchResultState;

static MatchResultState
SplitMatch(ejsval S, int q, ejsval R)
{
    /* 1. If R is a RegExp object (its [[Class]] is "RegExp"), then */
    if (EJSVAL_IS_REGEXP(R)) {
        /*    a. Call the [[Match]] internal method of R giving it the arguments S and q, and return the MatchResult  */
        /*       result. */
        EJS_NOT_IMPLEMENTED();
    }
        
    /* 2. Type(R) must be String. Let r be the number of characters in R. */
    int r = EJSVAL_TO_STRLEN(R);

    /* 3. Let s be the number of characters in S. */
    int s = EJSVAL_TO_STRLEN(S);

    /* 4. If q+r > s then return the MatchResult failure. */
    if (q + r > s) {
        MatchResultState rv = { MATCH_RESULT_FAILURE };
        return rv;
    }

    /* 5. If there exists an integer i between 0 (inclusive) and r (exclusive) such that the character at position q+i of S */
    /*    is different from the character at position i of R, then return failure. */
    jschar* sstr = EJSVAL_TO_FLAT_STRING(S);
    jschar* rstr = EJSVAL_TO_FLAT_STRING(R);
    for (int i = 0; i < r; i ++) {
        if (sstr[q+i] != rstr[i]) {
            MatchResultState rv = { MATCH_RESULT_FAILURE };
            return rv;
        }
    }

    /* 6. Let cap be an empty array of captures (see 15.10.2.1). */
    ejsval cap = _ejs_array_new(0, EJS_FALSE);
    /* 7. Return the State (q+r, cap). (see 15.10.2.1) */
    MatchResultState rv = { MATCH_RESULT_SUCCESS, q+r, cap };
    return rv;
}

// ES2015, June 2015
// 21.1.3.17 String.prototype.split ( separator, limit )
static EJS_NATIVE_FUNC(_ejs_String_prototype_split) {
    ejsval separator = _ejs_undefined;
    if (argc > 0) separator = args[0];

    ejsval limit = _ejs_undefined;
    if (argc > 1) limit = args[1];

    // 1. Let O be RequireObjectCoercible(this value).
    // 2. ReturnIfAbrupt(O).
    ejsval O = *_this;

    // 3. If separator is neither undefined nor null, then
    if (!EJSVAL_IS_NULL_OR_UNDEFINED(separator)) {
        // a. Let splitter be GetMethod(separator, @@split).
        // b. ReturnIfAbrupt(splitter).
        ejsval splitter = GetMethod(separator, _ejs_Symbol_split);

        // c. If splitter is not undefined, then,
        if (!EJSVAL_IS_UNDEFINED(splitter)) {
            //    i. Return Call(splitter, separator, «O, limit»).
            ejsval args[2] = { O, limit };
            return _ejs_invoke_closure(splitter, &separator, 2, args, _ejs_undefined);
        }
    }

    // 4. Let S be ToString(O).
    // 5. ReturnIfAbrupt(S).
    ejsval S = ToString(O);

    // 6. Let A be ArrayCreate(0).
    ejsval A = _ejs_array_new(0, EJS_FALSE);
    // 7. Let lengthA be 0.
    int lengthA = 0;


    // 8. If limit is undefined, let lim = 2^53-1; else let lim = ToLength(limit).
    // 9. ReturnIfAbrupt(lim)
    int64_t lim = EJSVAL_IS_UNDEFINED(limit) ? EJS_MAX_SAFE_INTEGER : ToLength(limit);

    // 10. Let s be the number of elements in S.
    int s = EJSVAL_TO_STRLEN(S);

    // 11. Let p = 0.
    int p = 0;

    // 12. Let R be ToString(separator).
    // 13. ReturnIfAbrupt(R).
    ejsval R = ToString(separator);

    // 14. If lim = 0, return A.
    if (lim == 0)
        return A;

    // 15. If separator is undefined, then
    if (EJSVAL_IS_UNDEFINED(separator)) {
        //     a. Call CreateDataProperty(A, "0", S).
        //     b. Assert: The above call will never result in an abrupt completion.
        _ejs_array_push_dense(A, 1, &S);
        //     c. Return A.
        return A;
    }
    // 17. If s = 0, then
    if (s == 0) {
        //     a. Let z be the result of SplitMatch(S, 0, R).
        MatchResultState z = SplitMatch(S, 0, R);
        //     b. If z is not false, return A.
        if (z.type != MATCH_RESULT_FAILURE)
            return A;
        //     c. Call CreateDataProperty(A, "0", S).
        //     d. Assert: The above call will never result in an abrupt completion.
        _ejs_array_push_dense(A, 1, &S);
        //     e. Return A.
        return A;
    }
    // 17. Let q = p.
    int q = p;

    // 18. Repeat, while q != s
    while (q != s) {
        // a. Let e be the result of SplitMatch(S, q, R).
        MatchResultState e = SplitMatch(S, q, R);
        // b. If e is false, then let q = q+1.
        if (e.type == MATCH_RESULT_FAILURE)
            q = q + 1;
        // c. Else e is an integer index into S,
        else {
            //    i. If e = p, then let q = q+1.
            if (e.endIndex == p) {
                q = q + 1;
            }
            //    ii. Else e != p,
            else {
                //  1. Let T be a String value equal to the substring of S consisting of the code units at indices p (inclusive) through q (exclusive).
                ejsval T = _ejs_string_new_substring (S, p, q-p);
                //  2. Call CreateDataProperty(A, ToString(lengthA), T).
                //  3. Assert: The above call will never result in an abrupt completion.
                _ejs_array_push_dense(A, 1, &T);
                //  4. Increment lengthA by 1.
                lengthA ++;
                //  5. If lengthA = lim, return A.
                if (lengthA == lim) return A;
                //  6. Let p = e.
                p = e.endIndex;
                //  7. Let q = p.
                q = p;
            }
        }
    }

    // 19. Let T be a String value equal to the substring of S consisting of the code units at indices p (inclusive) through s (exclusive).
    ejsval T = _ejs_string_new_substring (S, p, s-p);
    // 20. Call CreateDataProperty(A, ToString(lengthA), T).
    // 21. Assert: The above call will never result in an abrupt completion.
    _ejs_array_push_dense(A, 1, &T);
    // 22. Return A.
    return A;
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_slice) {
    // assert argc >= 1

    ejsval start = _ejs_undefined;
    ejsval end = _ejs_undefined;

    if (argc > 0) start = args[0];
    if (argc > 1) end = args[1];

    // Call CheckObjectCoercible passing the this value as its argument.
    // Let S be the result of calling ToString, giving it the this value as its argument.
    ejsval S = ToString(*_this);
    // Let len be the number of characters in S.
    int len = EJSVAL_TO_STRLEN(S);
    // Let intStart be ToInteger(start).
    int intStart = EJSVAL_IS_UNDEFINED(start) ? 0 : ToInteger(start);

    // If end is undefined, let intEnd be len; else let intEnd be ToInteger(end).
    int intEnd = EJSVAL_IS_UNDEFINED(end) ? len : ToInteger(end);

    // If intStart is negative, let from be max(len + intStart,0); else let from be min(intStart, len).
    int from = intStart < 0 ? MAX(len + intStart, 0) : MIN(intStart, len);

    // If intEnd is negative, let to be max(len + intEnd,0); else let to be min(intEnd, len).
    int to = intEnd < 0 ? MAX(len + intEnd, 0) : MIN(intEnd, len);

    // Let span be max(to – from,0).
    int span = MAX(to - from, 0);

    // Return a String containing span consecutive characters from S beginning with the character at position from.
    return _ejs_string_new_substring (S, from, span);
}

static EJS_NATIVE_FUNC(_ejs_String_fromCharCode) {
    int length = argc;
    jschar* buf = (jschar*)malloc(sizeof(jschar) * (length+1));
    for (int i = 0; i < argc; i ++) {
        buf[i] = ToUint16(args[i]);
    }
    buf[length] = 0;
    ejsval rv = _ejs_string_new_ucs2(buf);
    free (buf);
    return rv;
}

// ECMA262: 10.1.1 Static Semantics: UTF-16Encoding
static int
codepoint_to_codeunits(int64_t codepoint, jschar* units)
{
    // 1. Assert: 0 ≤ cp ≤ 0x10FFFF. 
    EJS_ASSERT (codepoint >= 0 && codepoint <= 0x10FFFF);

    // 2. If cp ≤ 65535, then return cp.
    if (codepoint <= 65535) {
        *units = (jschar)codepoint;
        return 1;
    }
    
    // 3. Let cu1 be floor((cp – 65536) / 1024) + 55296. NOTE 55296 is 0xD800. 
    *units = (jschar)(floor((codepoint - 0x10000) / 1024) + 0xD800);
    // 4. Let cu2 be ((cp – 65536) modulo 1024) + 56320. NOTE 56320 is 0xDC00.
    *(units+1) = (jschar)((codepoint - 0x10000) % 1024) + 0xDC00;
    // 5. Return the code unit sequence consisting of cu1 followed by cu2.
    return 2;
}
  
// ECMA262: 21.1.2.2 String.fromCodePoint ( ...codePoints ) 
static EJS_NATIVE_FUNC(_ejs_String_fromCodePoint) {
    if (argc == 0)
        return _ejs_atom_empty;

    // 1. Assert: codePoints is a well-formed rest parameter object
    // 2. Let length be the result of Get(codePoints, "length"). 
    uint32_t length = argc;

    // 3. Let elements be a new List. 
    jschar* elements = (jschar*)malloc(sizeof(jschar) * 2*length); // a safe maximum
    jschar* el = elements;

    // 4. Let nextIndex be 0. 
    uint32_t nextIndex = 0;
    // 5. Repeat while nextIndex < length 
    while (nextIndex < length) {
        //    a. Let next be the result of Get(codePoints, ToString(nextIndex)). 
        ejsval next = args[nextIndex];
        //    b. Let nextCP be ToNumber(next).
        ejsval nextCP = ToNumber(next);
        //    c. ReturnIfAbrupt(nextCP). 
        //    d. If SameValue(nextCP, ToInteger(nextCP)) is false, then throw a RangeError exception. 
        if (ToDouble(nextCP) != ToInteger(nextCP)) {
            free (elements);
            ejsval msg = _ejs_string_concat(_ejs_string_new_utf8("Invalid code point: "), ToString(nextCP));
            _ejs_throw_nativeerror(EJS_RANGE_ERROR, msg);
        }
        int64_t nextCP_ = ToInteger(nextCP);

        //    e. If nextCP < 0 or nextCP > 0x10FFFF, then throw a RangeError exception.
        if (nextCP_ < 0 || nextCP_ > 0x10FFFF) {
            free (elements);
            ejsval msg = _ejs_string_concat(_ejs_string_new_utf8("Invalid code point: "), ToString(nextCP));
            _ejs_throw_nativeerror(EJS_RANGE_ERROR, msg);
        }

        //    f. Append the elements of the UTF-16Encoding (10.1.1) of nextCP to the end of elements.
        el += codepoint_to_codeunits(nextCP_, el);

        //    g. Let nextIndex be nextIndex + 1. 
        nextIndex ++;
    }
    // 6. Return the String value whose elements are, in order, the elements in the List elements. If length is 0, the empty string is returned. 
    ejsval rv = _ejs_string_new_ucs2_len (elements, el - elements);
    free(elements);
    return rv;
}

// ECMA262: 21.1.2.4 String.raw ( callsite, ...substitutions )
static EJS_NATIVE_FUNC(_ejs_String_raw) {
    ejsval callsite = _ejs_undefined;
    if (argc > 0) callsite = args[0];

    // 1. Let substitutions be a List consisting of all of the arguments passed to this function, starting with the second argument. If fewer than two arguments were passed, the List is empty.
    ejsval* substitutions = NULL;
    if (argc > 1) substitutions = args + 1;

    // 2. Let numberOfSubstitutions be the numer of elements in substitutions.
    uint32_t numberOfSubstitutions = 0;
    if (argc > 1) numberOfSubstitutions = argc - 1;

    // 3. Let cooked be ToObject(callsite).
    // 4. ReturnIfAbrupt(cooked).
    ejsval cooked = ToObject(callsite);

    // 5. Let rawValue be the result of Get(cooked, "raw").
    ejsval rawValue = Get(cooked, _ejs_atom_raw);

    // 6. Let raw be ToObject(rawValue).
    // 7. ReturnIfAbrupt(raw).
    ejsval raw = ToObject(rawValue);

    // 8. Let len be the result of Get(raw, "length").
    ejsval len = Get(raw, _ejs_atom_length);

    // 9. Let literalSegments be ToLength(len).
    // 10. ReturnIfAbrupt(literalSegments).
    int64_t literalSegments = ToInteger(len);

    // 11. If literalSegments ≤ 0, then return the empty string.
    if (literalSegments <= 0)
        return _ejs_atom_empty;

    // 12. Let stringElements be a new List.
    ejsval stringElements = _ejs_array_new(0, EJS_FALSE);
    // 13. Let nextIndex be 0.
    int nextIndex = 0;

    // 14. Repeat 
    while (EJS_TRUE) {
        //     a. Let nextKey be ToString(nextIndex).
        ejsval nextKey = ToString(NUMBER_TO_EJSVAL(nextIndex));
        //     b. Let next be the result of Get(raw, nextKey)
        ejsval next = Get(raw, nextKey);

        //     c. Let nextSeg be ToString(next).
        //     d. ReturnIfAbrupt(nextSeg).
        ejsval nextSeg = ToString(next);

        //     e. Append in order the code unit elements of nextSeg to the end of stringElements.
        _ejs_array_push_dense (stringElements, 1, &nextSeg);

        //     f. If nextIndex + 1 = literalSegments, then
        if (nextIndex + 1 == literalSegments)
        //        i. Return the string value whose elements are, in order, the elements in the List stringElements. If stringElements has no elements, the empty string is returned.
            return _ejs_array_join(stringElements, _ejs_atom_empty);

        //     g. If nextIndex< numberOfSubstitutions, then let next be substitutions[nextIndex].
        if (nextIndex < numberOfSubstitutions)
            next = substitutions[nextIndex];
        //     h. Else, let next is the empty String.
        else
            next = _ejs_atom_empty;

        //     i. Let nextSub be ToString(next).
        //     j. ReturnIfAbrupt(nextSub).
        ejsval nextSub = ToString(next);

        //     k. Append in order the code unit elements of nextSub to the end of stringElements.
        _ejs_array_push_dense (stringElements, 1, &nextSub);

        //     l. Let nextIndex be nextIndex + 1
        nextIndex ++;
    }
}

// ES2015, June 2015
// 21.1.3.18 String.prototype.startsWith ( searchString [, position ] )
static EJS_NATIVE_FUNC(_ejs_String_prototype_startsWith) {
    ejsval searchString = _ejs_undefined;
    ejsval position = _ejs_undefined;

    if (argc > 0)
        searchString = args[0];
    if (argc > 1)
        position = args[1];

    // 1. Let O be RequireObjectCoercible(this value).
    ejsval O = ToObject(*_this);
    if (!EJSVAL_IS_OBJECT(O) && !EJSVAL_IS_NULL(O)) {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "1"); // XXX
    }
    // 2. Let S be ToString(O).
    // 3. ReturnIfAbrupt(S).
    ejsval S = ToString(O);

    // 4. Let isRegExp be IsRegExp(searchString).
    // 5. ReturnIfAbrupt(isRegExp).
    EJSBool isRegExp = IsRegExp(searchString);

    // 6. If isRegExp is true, throw a TypeError exception.
    if (isRegExp)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "2"); // XXX

    // 7. Let searchStr be ToString(searchString).
    // 8. ReturnIfAbrupt(searchString).
    ejsval searchStr = ToString(searchString);

    // 9. Let pos be ToInteger(position). (If position is undefined, this step produces the value 0).
    // 10. ReturnIfAbrupt(pos).
    int64_t pos = ToInteger(position);

    // 11. Let len be the number of elements in S.
    uint32_t len = EJSVAL_TO_STRLEN(S);

    // 12. Let start be min(max(pos, 0), len).
    int64_t start = MIN(MAX(pos, 0), len);

    // 13. Let searchLength be the number of elements in searchStr.
    uint32_t searchLength = EJSVAL_TO_STRLEN(searchStr);

    // 14. If searchLength+start is greater than len, return false.
    if (searchLength + start > len)
        return _ejs_false;

    // 15. If the sequence of elements of S starting at start of length searchLength is the same as the full element sequence of searchStr, return true.
    // 16. Otherwise, return false.

    // XXX toshok this would be nicer if we had a string iterator object or
    // something, which could contain traversal state across a rope.
    // as it is now, let's just flatten both strings (ugh) and walk

    EJSPrimString* prim_S = _ejs_string_flatten(S);
    EJSPrimString* prim_searchStr = _ejs_string_flatten(searchStr);
    for (int i = 0; i < searchLength; i ++) {
        if (prim_S->data.flat[i + start] != prim_searchStr->data.flat[i])
            return _ejs_false;
    }
    
    
    return _ejs_true;
}

// ES2015, June 2015
// 21.1.3.6 String.prototype.endsWith ( searchString [ , endPosition] )
static EJS_NATIVE_FUNC(_ejs_String_prototype_endsWith) {
    ejsval searchString = _ejs_undefined;
    ejsval endPosition = _ejs_undefined;
    if (argc > 0)
        searchString = args[0];
    if (argc > 1)
        endPosition = args[1];

    // 1. Let O be RequireObjectCoercible(this value).
    ejsval O = ToObject(*_this);
    if (!EJSVAL_IS_OBJECT(O) && !EJSVAL_IS_NULL(O)) {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "1"); // XXX
    }
    // 2. Let S be ToString(O).
    // 3. ReturnIfAbrupt(S).
    ejsval S = ToString(O);

    // 4. Let isRegExp be IsRegExp(searchString).
    // 5. ReturnIfAbrupt(isRegExp).
    EJSBool isRegExp = IsRegExp(searchString);

    // 6. If isRegExp is true, throw a TypeError exception.
    if (isRegExp)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "2"); // XXX

    // 7. Let searchStr be ToString(searchString).
    // 8. ReturnIfAbrupt(searchStr).
    ejsval searchStr = ToString(searchString);

    // 9. Let len be the number of elements in S.
    uint32_t len = EJSVAL_TO_STRLEN(S);

    // 10. If endPosition is undefined, let pos be len, else let pos be ToInteger(endPosition).
    // 11. ReturnIfAbrupt(pos).
    int64_t pos;
    if (EJSVAL_IS_UNDEFINED(endPosition))
        pos = len;
    else
        pos = ToInteger(endPosition);

    // 12. Let end be min(max(pos, 0), len).
    uint32_t end = MIN(MAX(pos, 0), len);

    // 13. Let searchLength be the number of elements in searchStr.
    uint32_t searchLength = EJSVAL_TO_STRLEN(searchStr);

    // 14. Let start be end - searchLength.
    int64_t start = (int64_t)end - (int64_t)searchLength;

    // 15. If start is less than 0, return false.
    if (start < 0)
        return _ejs_false;

    // 16. If the sequence of elements of S starting at start of length searchLength is the same as the full element sequence of searchStr, return true.
    // 17. Otherwise, return false.

    // XXX toshok this would be nicer if we had a string iterator object or
    // something, which could contain traversal state across a rope.
    // as it is now, let's just flatten both strings (ugh) and walk
    EJSPrimString* prim_S = _ejs_string_flatten(S);
    EJSPrimString* prim_searchStr = _ejs_string_flatten(searchStr);
    for (int i = 0; i < searchLength; i ++) {
        if (prim_S->data.flat[i + start] != prim_searchStr->data.flat[i])
            return _ejs_false;
    }
    return _ejs_true;
}

// ECMA262: 21.1.3.3 String.prototype.codePointAt ( pos ) 
// NOTE Returns a nonnegative integer Number less than 1114112 (0x110000) that is the UTF-16 encoded code 
// point value starting at the string element at position pos in the String resulting from converting this object to a String. If 
// there is no element at that position, the result is undefined. If a valid UTF-16 surrogate pair does not begin at pos, the 
// result is the code unit at pos.
static EJS_NATIVE_FUNC(_ejs_String_prototype_codePointAt) {
    // When the codePointAt method is called with one argument pos, the following steps are taken: 
    ejsval pos = _ejs_undefined;
    if (argc > 0)
        pos = args[0];

    // 1. Let O be CheckObjectCoercible(this value). 
    ejsval O = ToObject(*_this);
    if (!EJSVAL_IS_OBJECT(O) && !EJSVAL_IS_NULL(O)) {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "1"); // XXX
    }

    // 2. Let S be ToString(O).
    // 3. ReturnIfAbrupt(S). 
    ejsval S = ToString(O);

    // 4. Let position be ToInteger(pos). 
    int64_t position = ToInteger(pos);

    // 5. ReturnIfAbrupt(position). 

    // 6. Let size be the number of elements in S. 
    uint32_t size = EJSVAL_TO_STRLEN(S);

    // 7. If position < 0 or position ≥ size, return undefined. 
    if (position < 0 || position >= size)
        return _ejs_undefined;

    // 8. Let first be the code unit value of the element at index position in the String S. 
    uint64_t first = _ejs_string_ucs2_at (EJSVAL_TO_STRING(S), position);

    // 9. If first < 0xD800 or first > 0xDBFF or position+1 = size, then return first. 
    if (first < 0xD800 || first > 0xDBFF || position+1 == size)
        return NUMBER_TO_EJSVAL(first);

    // 10. Let second be the code unit value of the element at index position+1 in the String S. 
    uint64_t second = _ejs_string_ucs2_at (EJSVAL_TO_STRING(S), position+1);

    // 11. If second < 0xDC00 or second > 0xDFFF, then return first. 
    if (second < 0xDC00 || second > 0xDFFF)
        return NUMBER_TO_EJSVAL(first);

    // 12. Return ((first – 0xD800) × 1024) + (second – 0xDC00) + 0x10000. 
    return NUMBER_TO_EJSVAL(((first - 0xD800) * 1024) + (second - 0xDC00) + 0x10000);
}

// ECMA262: 21.1.3.13 String.prototype.repeat ( count )
static EJS_NATIVE_FUNC(_ejs_String_prototype_repeat) {
    ejsval count = _ejs_undefined;
    if (argc > 0)
        count = args[0];

    // 1. Let O be CheckObjectCoercible(this value).
    ejsval O = ToObject(*_this);
    if (!EJSVAL_IS_OBJECT(O) && !EJSVAL_IS_NULL(O)) {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "1"); // XXX
    }

    // 2. Let S be ToString(O).
    ejsval S = ToString(O);

    // 3. ReturnIfAbrupt(S).
    // 4. Let n be the result of calling ToInteger(count).
    // 5. ReturnIfAbrupt(n).
    double n_ = ToDouble(count);

    // 7. If n is +∞, then throw a RangeError exception.
    if (isinf(n_))
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "2"); // XXX

    int64_t n = (int64_t)n_;

    // 6. If n < 0, then throw a RangeError exception.
    if (n < 0)
        _ejs_throw_nativeerror_utf8(EJS_RANGE_ERROR, "3"); // XXX

    // 8. Let T be a String value that is made from n copies of S appended together. If n is 0, T is the empty String.
    if (n == 0)
        return _ejs_atom_empty;

    ejsval T = S;
    n--;
    while (n > 0) {
        T = _ejs_string_concat(T, S);
        n--;
    }

    // 9. Return T.
    return T;
}

// ES2015, June 2015
// 21.1.3.7 String.prototype.includes ( searchString [ , position ] )
static EJS_NATIVE_FUNC(_ejs_String_prototype_includes) {
    ejsval searchString = _ejs_undefined;
    ejsval position = _ejs_undefined;
    if (argc > 0)
        searchString = args[0];
    if (argc > 1)
        position = args[1];

    // 1. Let O be RequireObjectCoercible(this value).
    ejsval O = ToObject(*_this);
    if (!EJSVAL_IS_OBJECT(O) && !EJSVAL_IS_NULL(O)) {
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "1"); // XXX
    }

    // 2. Let S be ToString(O). 
    // 3. ReturnIfAbrupt(S). 
    ejsval S = ToString(O);

    // 4. Let isRegExp be IsRegExp(searchString).
    // 5. ReturnIfAbrupt(isRegExp).
    EJSBool isRegExp = IsRegExp(searchString);
    
    // 6. If isRegExp is true, throw a TypeError exception.
    if (isRegExp)
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "2"); // XXX

    // 7. Let searchStr be ToString(searchString).
    // 8. ReturnIfAbrupt(searchStr).
    ejsval searchStr = ToString(searchString);

    // 9. Let pos be ToInteger(position). (If position is undefined, this step produces the value 0).
    // 10. ReturnIfAbrupt(pos).
    int64_t pos = ToInteger(position);

    // 11. Let len be the number of elements in S.
    uint32_t len = EJSVAL_TO_STRLEN(S);

    // 12. Let start be min(max(pos, 0), len).
    int64_t start = MIN(MAX(pos, 0), len);

    // 13. Let searchLen be the number of elements in searchStr.
    // 14. If there exists any integer k not smaller than start such that k + searchLen is not greater than len, and for all nonnegative integers j less than searchLen, the code unit at index k+j of S is the same as the code unit at index j of searchStr, return true; but if there is no such integer k, return false.

    EJSPrimString* prim_S = _ejs_string_flatten(S);
    EJSPrimString* prim_searchStr = _ejs_string_flatten(searchStr);

    return ucs2_strstr(prim_S->data.flat + start, prim_searchStr->data.flat) ? _ejs_true : _ejs_false;
}

static EJS_NATIVE_FUNC(_ejs_String_prototype_iterator) {
    /* 1. Let O be CheckObjectCoercible(this value). */
    ejsval O = *_this;

    if (!EJSVAL_IS_STRING(O) && !EJSVAL_IS_OBJECT(O) && !EJSVAL_IS_NULL(O))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "1");

    /* 2. Let S be ToString(O). */
    /* 3. ReturnIfAbrupt(S). */
    ejsval S = ToString(O);

    /* 4. Return the result of calling the CreateStringIterator abstract operation with argument S. */
    return _ejs_string_iterator_new (S);
}

/* 21.1.5.1 CreateStringIterator Abstract Operation */
ejsval
_ejs_string_iterator_new (ejsval string)
{
    /* 1. Let s be the result of calling ToString(string). */
    /* 2. ReturnIfAbrupt(s). */
    ejsval s = ToString(string);

    /* 3. Let iterator be the result of ObjectCreate(%StringIteratorPrototype%,
     * ([[IteratedStringt]], [[StringIteratorNextIndex]] )). */
    EJSStringIterator *iterator = _ejs_gc_new (EJSStringIterator);
    _ejs_init_object ((EJSObject*)iterator, _ejs_StringIterator_prototype, &_ejs_StringIterator_specops);

    /* 4. Set iterator’s [[IteratedString]] internal slot to s. */
    iterator->iterated = s;

    /* 5. Set iterator’s [[StringIteratorNextIndex]] internal slot to 0. */
    iterator->next_index = 0;

    /* 6. Return iterator. */
    return OBJECT_TO_EJSVAL(iterator);
}


ejsval _ejs_StringIterator_prototype EJSVAL_ALIGNMENT;
ejsval _ejs_StringIterator EJSVAL_ALIGNMENT;

static EJS_NATIVE_FUNC(_ejs_StringIterator_impl) {
    return *_this;
}

/* 21.1.5.2.1 %StringIteratorPrototype%.next () */
static EJS_NATIVE_FUNC(_ejs_StringIterator_prototype_next) {
    /* 1. Let O be the this value. */
    ejsval O = *_this;

    /* 2. If Type(O) is not Object, throw a TypeError exception. */
    if (!EJSVAL_IS_OBJECT(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".next called on non-object");

    /* 3. If O does not have all of the internal slots of an String Iterator Instance (21.1.5.3),
     * throw a TypeError exception. */
    if (!EJSVAL_IS_STRINGITERATOR(O))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, ".next called on non-StringIterator instance");

    EJSStringIterator *OObj = (EJSStringIterator*) EJSVAL_TO_OBJECT(O);

    /* 4. Let s be the value of the [[IteratedString]] internal slot of O. */
    ejsval s = OObj->iterated;

    /* 5. If s is undefined, then return CreateIterResultObject(undefined, true). */
    if (EJSVAL_IS_UNDEFINED(s))
        return _ejs_create_iter_result (_ejs_undefined, _ejs_true);

    ejsval sPrimStr;
    if (EJSVAL_IS_STRING(s))
        sPrimStr = s;
    else {
        EJSString *str = (EJSString*)EJSVAL_TO_OBJECT(s);
        sPrimStr = str->primStr;
    }

    /* 6. Let position be the value of the [[StringIteratorNextIndex]] internal slot of O. */
    uint32_t position = OObj->next_index;

    /* 7. Let len be the number of elements in s. */
    uint32_t len = EJSVAL_TO_STRLEN(s);

    /* 8. If position ≥ len, then */
    if (position >= len) {
        /* a. Set the value of the [[IteratedString]] internal slot of O to undefined. */
        OObj->iterated = _ejs_undefined;

        /* b. Return CreateIterResultObject(undefined, true). */
        return _ejs_create_iter_result (_ejs_undefined, _ejs_true);
    }

    jschar chars[2];

    /* 9. Let first be the code unit value of the element at index position in s. */
    chars[0] = _ejs_string_ucs2_at(EJSVAL_TO_STRING(sPrimStr), position);

    ejsval resultString;
    uint32_t resultSize = 1;

    // 10. If first < 0xD800 or first > 0xDBFF or position+1 = len then let resultString be the string consisting of the single code unit first.
    if (chars[0] < 0xD800 || chars[0] > 0xDBFF || position+1 == len)
        resultString = _ejs_string_new_ucs2_len(chars, 1);
    // 11. Else,
    else {
        //      a. Let second be the code unit value of the element at index position+1 in the String S.
        chars[1] = _ejs_string_ucs2_at(EJSVAL_TO_STRING(sPrimStr), position + 1);
        //      b. If second < 0xDC00 or second > 0xDFFF, then let resultString be the string consisting
        //         of the single code unit first.
        if (chars[1] < 0xDc00 || chars[1] > 0xDFFF)
            resultString = _ejs_string_new_ucs2_len(chars, 1);
        //      c. Else, let resultString be the string consisting of the code unit first followed by
        //          the code unit second. */
        else {
            resultString = _ejs_string_new_ucs2_len(chars, 2);
            resultSize = 2;
        }
    }

    /* 12. Let resultSize be the number of code units in resultString. */
    // done above.

    /* 13. Set the value of the [[StringIteratorNextIndex]] internal slot of O to position+ resultSize. */
    OObj->next_index = position + resultSize;

    /* 14. Return CreateIterResultObject(resultString, false). */
    return _ejs_create_iter_result (resultString, _ejs_false);
}

static void
_ejs_string_init_proto()
{
    _ejs_gc_add_root (&_ejs_String__proto__);
    _ejs_gc_add_root (&_ejs_String_prototype);

    EJSFunction* __proto__ = _ejs_gc_new(EJSFunction);
    _ejs_init_object ((EJSObject*)__proto__, _ejs_Object_prototype, &_ejs_Function_specops);
    __proto__->func = _ejs_Function_empty;
    __proto__->env = _ejs_null;
    _ejs_String__proto__ = OBJECT_TO_EJSVAL(__proto__);

    EJSString* prototype = (EJSString*)_ejs_gc_new(EJSString);
    _ejs_init_object ((EJSObject*)prototype, _ejs_null, &_ejs_String_specops);
    prototype->primStr = _ejs_atom_empty;
    _ejs_String_prototype = OBJECT_TO_EJSVAL(prototype);

    _ejs_object_define_value_property (OBJECT_TO_EJSVAL(__proto__), _ejs_atom_name, _ejs_atom_empty, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
}

void
_ejs_string_init(ejsval global)
{
    _ejs_string_init_proto();
  
    _ejs_String = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_String, _ejs_String_impl);
    _ejs_object_setprop (global, _ejs_atom_String, _ejs_String);

    _ejs_object_setprop (_ejs_String,       _ejs_atom_prototype,  _ejs_String_prototype);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_String, x, _ejs_String_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_String_prototype, x, _ejs_String_prototype_##x)
#define PROTO_METHOD_LEN(x,l) EJS_INSTALL_ATOM_FUNCTION_LEN_FLAGS (_ejs_String_prototype, x, _ejs_String_prototype_##x, l, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)

    PROTO_METHOD(charAt);
    PROTO_METHOD(charCodeAt);
    PROTO_METHOD(codePointAt);
    PROTO_METHOD(concat);
    PROTO_METHOD_LEN(includes, 1);
    PROTO_METHOD(endsWith);
    PROTO_METHOD(indexOf);
    PROTO_METHOD(lastIndexOf);
    PROTO_METHOD(localeCompare);
    PROTO_METHOD(match);
    PROTO_METHOD(repeat);
    PROTO_METHOD(replace);
    PROTO_METHOD(search);
    PROTO_METHOD(slice);
    PROTO_METHOD(split);
    PROTO_METHOD(startsWith);
    PROTO_METHOD(substr);
    PROTO_METHOD(substring);
    PROTO_METHOD(toLocaleLowerCase);
    PROTO_METHOD(toLocaleUpperCase);
    PROTO_METHOD(toLowerCase);
    PROTO_METHOD(toString);
    PROTO_METHOD(toUpperCase);
    PROTO_METHOD(trim);
    PROTO_METHOD(valueOf);

    OBJ_METHOD(fromCharCode);
    OBJ_METHOD(fromCodePoint);
    OBJ_METHOD(raw);

    ejsval _iterator = _ejs_function_new_native (_ejs_null, _ejs_Symbol_iterator, _ejs_String_prototype_iterator);
    _ejs_object_define_value_property (_ejs_String_prototype, _ejs_Symbol_iterator, _iterator, EJS_PROP_NOT_ENUMERABLE);

    _ejs_object_define_value_property (_ejs_String_prototype, _ejs_atom_constructor, _ejs_String,
                                        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#undef OBJ_METHOD
#undef PROTO_METHOD


    _ejs_StringIterator = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_String, _ejs_StringIterator_impl);

    _ejs_gc_add_root (&_ejs_StringIterator_prototype);
    _ejs_StringIterator_prototype = _ejs_string_iterator_new(_ejs_String_prototype);
    EJSVAL_TO_OBJECT(_ejs_StringIterator_prototype)->proto = _ejs_Iterator_prototype;
    _ejs_object_define_value_property (_ejs_StringIterator, _ejs_atom_prototype, _ejs_StringIterator_prototype,
                                        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property (_ejs_StringIterator_prototype, _ejs_atom_constructor, _ejs_StringIterator,
                                        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE);

#define PROTO_ITER_METHOD(x) EJS_INSTALL_ATOM_FUNCTION_FLAGS (_ejs_StringIterator_prototype, x, _ejs_StringIterator_prototype_##x, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_WRITABLE | EJS_PROP_CONFIGURABLE)
    PROTO_ITER_METHOD(next);
#undef PROTO_ITER_METHOD

}

static ejsval
_ejs_string_specop_get (ejsval obj, ejsval propertyName, ejsval receiver)
{
    // check if propertyName is an integer, or a string that we can convert to an int
    EJSBool is_index = EJS_FALSE;
    int idx = 0;
    if (EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;
            is_index = EJS_TRUE;
        }
    }

    EJSString* estr = (EJSString*)EJSVAL_TO_OBJECT(obj);
    if (is_index) {
        if (idx < 0 || idx >= EJSVAL_TO_STRLEN(estr->primStr))
            return _ejs_undefined;
        jschar c = _ejs_string_ucs2_at (EJSVAL_TO_STRING(estr->primStr), idx);
        return _ejs_string_new_ucs2_len (&c, 1);
    }

    // we also handle the length getter here
    if (EJSVAL_IS_STRING(propertyName) && !ucs2_strcmp (_ejs_ucs2_length, EJSVAL_TO_FLAT_STRING(propertyName))) {
        return NUMBER_TO_EJSVAL (EJSVAL_TO_STRLEN(estr->primStr));
    }

    // otherwise we fallback to the object implementation
    return _ejs_Object_specops.Get (obj, propertyName, receiver);
}

static EJSObject*
_ejs_string_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSString);
}

static void
_ejs_string_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSString* ejss = (EJSString*)obj;
    scan_func (ejss->primStr);
    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(String,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 _ejs_string_specop_get,
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 _ejs_string_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 _ejs_string_specop_scan
                 )



static void
_ejs_string_iterator_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSStringIterator* iter = (EJSStringIterator*)obj;
    scan_func(iter->iterated);
    _ejs_Object_specops.Scan (obj, scan_func);
}

EJS_DEFINE_CLASS(StringIterator,
                 OP_INHERIT, // [[GetPrototypeOf]]
                 OP_INHERIT, // [[SetPrototypeOf]]
                 OP_INHERIT, // [[IsExtensible]]
                 OP_INHERIT, // [[PreventExtensions]]
                 OP_INHERIT, // [[GetOwnProperty]]
                 OP_INHERIT, // [[DefineOwnProperty]]
                 OP_INHERIT, // [[HasProperty]]
                 OP_INHERIT, // [[Get]]
                 OP_INHERIT, // [[Set]]
                 OP_INHERIT, // [[Delete]]
                 OP_INHERIT, // [[Enumerate]]
                 OP_INHERIT, // [[OwnPropertyKeys]]
                 OP_INHERIT, // [[Call]]
                 OP_INHERIT, // [[Construct]]
                 OP_INHERIT, // allocate.  shouldn't ever be used
                 OP_INHERIT, // finalize.  also shouldn't ever be used
                 _ejs_string_iterator_specop_scan
                 )

/// EJSPrimString's

ejsval
_ejs_string_new_utf8 (const char* str)
{
    // XXX assume str is ascii for now
    int str_len = strlen(str);
    size_t value_size = EJS_PRIMSTR_FLAT_ALLOC_SIZE + sizeof(jschar) * (str_len + 1);
    EJSBool ool_buffer = EJS_FALSE;

    if (value_size > 2048) {
        value_size = EJS_PRIMSTR_FLAT_ALLOC_SIZE;
        ool_buffer = EJS_TRUE;
    }

    EJSPrimString* rv = _ejs_gc_new_primstr(value_size);
    EJS_PRIMSTR_SET_TYPE(rv, EJS_STRING_FLAT);
    if (ool_buffer) {
        EJS_PRIMSTR_SET_HAS_OOL_BUFFER(rv);
        rv->data.flat = (jschar*)malloc(sizeof(jschar) * (str_len + 1));
    }
    else {
        rv->data.flat = (jschar*)((char*)rv + EJS_PRIMSTR_FLAT_ALLOC_SIZE);
    }
    jschar *p = rv->data.flat;
    const unsigned char *stru = (const unsigned char*)str;
    while (*stru) {
        jschar c = utf8_to_ucs2 (stru, &stru);
        if (c == (jschar)-1) {
            break;
        }
        *p++ = c;
    }
    *p = 0;
    rv->length = p - rv->data.flat;
    return STRING_TO_EJSVAL(rv);
}

ejsval
_ejs_string_new_utf8_len (const char* str, int len)
{
    size_t value_size = EJS_PRIMSTR_FLAT_ALLOC_SIZE + sizeof(jschar) * (len + 1);
    EJSBool ool_buffer = EJS_FALSE;

    if (value_size > 2048) {
        value_size = EJS_PRIMSTR_FLAT_ALLOC_SIZE;
        ool_buffer = EJS_TRUE;
    }

    EJSPrimString* rv = _ejs_gc_new_primstr(value_size);
    EJS_PRIMSTR_SET_TYPE(rv, EJS_STRING_FLAT);
    if (ool_buffer) {
        EJS_PRIMSTR_SET_HAS_OOL_BUFFER(rv);
        rv->data.flat = (jschar*)malloc(sizeof(jschar) * (len + 1));
    }
    else {
        rv->data.flat = (jschar*)((char*)rv + EJS_PRIMSTR_FLAT_ALLOC_SIZE);
    }
    jschar *p = rv->data.flat;
    const unsigned char *stru = (const unsigned char*)str;
    while (len > 0) {
        jschar c = utf8_to_ucs2 (stru, &stru);
        if (c == (jschar)-1) {
            break;
        }
        *p++ = c;
        len--;
    }
    *p = 0;
    rv->length = p - rv->data.flat;
    return STRING_TO_EJSVAL(rv);
}

ejsval
_ejs_string_new_ucs2 (const jschar* str)
{
    int str_len = ucs2_strlen(str);
    size_t value_size = EJS_PRIMSTR_FLAT_ALLOC_SIZE + sizeof(jschar) * (str_len + 1);
    EJSBool ool_buffer = EJS_FALSE;

    if (value_size > 2048) {
        value_size = EJS_PRIMSTR_FLAT_ALLOC_SIZE;
        ool_buffer = EJS_TRUE;
    }

    EJSPrimString* rv = _ejs_gc_new_primstr(value_size);
    EJS_PRIMSTR_SET_TYPE(rv, EJS_STRING_FLAT);
    rv->length = str_len;
    if (ool_buffer) {
        EJS_PRIMSTR_SET_HAS_OOL_BUFFER(rv);
        rv->data.flat = (jschar*)malloc(sizeof(jschar) * (str_len + 1));
    }
    else {
        rv->data.flat = (jschar*)((char*)rv + EJS_PRIMSTR_FLAT_ALLOC_SIZE);
    }
    memmove (rv->data.flat, str, str_len * sizeof(jschar));
    rv->data.flat[str_len] = 0;
    return STRING_TO_EJSVAL(rv);
}

ejsval
_ejs_string_new_ucs2_len (const jschar* str, int len)
{
    size_t value_size = EJS_PRIMSTR_FLAT_ALLOC_SIZE + sizeof(jschar) * (len + 1);
    EJSBool ool_buffer = EJS_FALSE;

    if (value_size > 2048) {
        value_size = EJS_PRIMSTR_FLAT_ALLOC_SIZE;
        ool_buffer = EJS_TRUE;
    }

    EJSPrimString* rv = _ejs_gc_new_primstr(value_size);
    EJS_PRIMSTR_SET_TYPE(rv, EJS_STRING_FLAT);
    rv->length = len;
    if (ool_buffer) {
        EJS_PRIMSTR_SET_HAS_OOL_BUFFER(rv);
        rv->data.flat = (jschar*)malloc(sizeof(jschar) * (len + 1));
    }
    else {
        rv->data.flat = (jschar*)((char*)rv + EJS_PRIMSTR_FLAT_ALLOC_SIZE);
    }
    memmove (rv->data.flat, str, len * sizeof(jschar));
    rv->data.flat[len] = 0;
    return STRING_TO_EJSVAL(rv);
}

static void flatten_dep (jschar **p, EJSPrimString *n, int* off, int* len);

ejsval
_ejs_string_new_substring (ejsval str, int off, int len)
{
    EJSPrimString *prim_str = EJSVAL_TO_STRING(str);
    EJSPrimString* rv;

    // XXX we should probably validate off/len here..
    if (len < FLAT_DEP_THRESHOLD) {
        rv = _ejs_gc_new_primstr(EJS_PRIMSTR_FLAT_ALLOC_SIZE + sizeof(jschar) * (len + 1));
        EJS_PRIMSTR_SET_TYPE(rv, EJS_STRING_FLAT);

        rv->data.flat = (jschar*)((char*)rv + EJS_PRIMSTR_FLAT_ALLOC_SIZE);

        // reuse the flatten machinery by using a stack allocated dep string that we flatten into the rv
        EJSPrimString dep = { 0 };
        EJS_PRIMSTR_SET_TYPE(&dep, EJS_STRING_DEPENDENT);
        dep.length = len;
        dep.data.dependent.dep = prim_str;
        dep.data.dependent.off = off;
        jschar* p = rv->data.flat;
        int tmp_off = 0;
        int tmp_len = len;
        flatten_dep (&p, &dep, &tmp_off, &tmp_len);
        *p = 0;
    }
    else {
        rv = _ejs_gc_new_primstr(EJS_PRIMSTR_DEP_ALLOC_SIZE);
        EJS_PRIMSTR_SET_TYPE(rv, EJS_STRING_DEPENDENT);
        rv->data.dependent.dep = prim_str;
        rv->data.dependent.off = off;
    }
    rv->length = len;
    return STRING_TO_EJSVAL(rv);
}


static void flatten_rope (jschar **p, EJSPrimString *n);

ejsval
_ejs_string_concat (ejsval left, ejsval right)
{
    EJSPrimString* lhs = EJSVAL_TO_STRING(left);
    EJSPrimString* rhs = EJSVAL_TO_STRING(right);
    
    if (lhs->length + rhs->length < FLAT_ROPE_THRESHOLD) {
        uint32_t new_strlen = lhs->length + rhs->length;
        jschar buffer[FLAT_ROPE_THRESHOLD];
        jschar *p = buffer;
        flatten_rope(&p, lhs);
        flatten_rope(&p, rhs);
        return _ejs_string_new_ucs2_len(buffer, new_strlen);
    }
    else {
        EJSPrimString* rv = _ejs_gc_new_primstr (EJS_PRIMSTR_ROPE_ALLOC_SIZE);
        EJS_PRIMSTR_SET_TYPE(rv, EJS_STRING_ROPE);
        rv->length = lhs->length + rhs->length;
        rv->data.rope.left = lhs;
        rv->data.rope.right = rhs;
        return STRING_TO_EJSVAL(rv);
    }
}

ejsval
_ejs_string_concatv (ejsval first, ...)
{
    ejsval result = first;
    ejsval arg;

    va_list ap;

    va_start(ap, first);

    arg = va_arg(ap, ejsval);
    do {
        result = _ejs_string_concat (result, arg);
        arg = va_arg(ap, ejsval);
    } while (!EJSVAL_IS_NULL_OR_UNDEFINED(arg));

    va_end(ap);

    return result;
}

static void flatten_dep (jschar **p, EJSPrimString *n, int* off, int* len);

static void
flatten_rope (jschar **p, EJSPrimString *n)
{
    switch (EJS_PRIMSTR_GET_TYPE(n)) {
    case EJS_STRING_FLAT:
        memmove (*p, n->data.flat, n->length * sizeof(jschar));
        *p += n->length;
        break;
    case EJS_STRING_ROPE:
        flatten_rope(p, n->data.rope.left);
        flatten_rope(p, n->data.rope.right);
        break;
    case EJS_STRING_DEPENDENT: {
        int off = n->data.dependent.off;
        int len = n->length;
        flatten_dep (p, n->data.dependent.dep, &off, &len);
        break;
    }
    default:
        EJS_NOT_IMPLEMENTED();
    }
}

static void
flatten_dep (jschar **p, EJSPrimString *n, int* off, int* len)
{
    // nothing else to append
    if (*len == 0)
        return;

    // first step is to locate the node that contains the start of our characters
    if (*off >= 0) {
        switch (EJS_PRIMSTR_GET_TYPE(n)) {
        case EJS_STRING_FLAT: {
            if (*off < n->length) {
                // we handle the first append here
                int length_to_append = MIN(*len, n->length - *off);
                memmove (*p, n->data.flat + *off, length_to_append * sizeof(jschar));
                *p += length_to_append;
                *len -= length_to_append;
                *off = 0;
            }
            else {
                *off -= n->length;
            }
            break;
        }
        case EJS_STRING_ROPE:
            flatten_dep(p, n->data.rope.left, off, len);
            flatten_dep(p, n->data.rope.right, off, len);
            break;
        case EJS_STRING_DEPENDENT: {
            *off += n->data.dependent.off;
            flatten_dep(p, n->data.dependent.dep, off, len);
            break;
        }
        default:
            EJS_NOT_IMPLEMENTED();
        }
    }
    else {
        // we need to append len characters from this string into the buffer
        switch (EJS_PRIMSTR_GET_TYPE(n)) {
        case EJS_STRING_FLAT: {
            int length_to_append = MIN (*len, n->length);
            memmove (*p, n->data.flat, length_to_append * sizeof(jschar));
            *p += length_to_append;
            *len -= length_to_append;
            break;
        }
        case EJS_STRING_ROPE:
            flatten_dep(p, n->data.rope.left, off, len);
            flatten_dep(p, n->data.rope.right, off, len);
            break;
        case EJS_STRING_DEPENDENT: {
            int length_to_append = MIN (*len, n->length);
            flatten_dep (p, n->data.dependent.dep, off, &length_to_append);
            *len -= length_to_append;
            break;
        }
        default:
            EJS_NOT_IMPLEMENTED();
        }
    }
}

EJSPrimString*
_ejs_primstring_flatten (EJSPrimString* primstr)
{
    if (EJS_PRIMSTR_GET_TYPE(primstr) == EJS_STRING_FLAT)
        return primstr;

    jschar *buffer = (jschar*)malloc(sizeof(jschar) * (primstr->length + 1));
    jschar *p = buffer;

    switch (EJS_PRIMSTR_GET_TYPE(primstr)) {
    case EJS_STRING_DEPENDENT: {
        // modify the string in-place, switching from a dep to a flat string
        int off = 0;
        int length = primstr->length;
        flatten_dep (&p, primstr, &off, &length);
        //EJS_ASSERT (off == 0);
        //EJS_ASSERT (length == 0);
        break;
    }
    case EJS_STRING_ROPE: {
        // modify the string in-place, switching from a rope to a flat string
        flatten_rope (&p, primstr);
        break;
    }
    default:
        EJS_NOT_REACHED();
    }

    buffer[primstr->length] = 0;

    EJS_PRIMSTR_CLEAR_TYPE(primstr);
    EJS_PRIMSTR_SET_TYPE(primstr, EJS_STRING_FLAT);
    EJS_PRIMSTR_SET_HAS_OOL_BUFFER(primstr);
    primstr->data.flat = buffer;
    return primstr;
}

EJSPrimString*
_ejs_string_flatten (ejsval str)
{
    return _ejs_primstring_flatten (EJSVAL_TO_STRING_IMPL(str));
}

static uint32_t
hash_dep (int hash, EJSPrimString* n, int* off, int* len)
{
    // nothing left to hash
    if (*len == 0)
        return hash;

    // first step is to locate the node that contains the start of our characters
    if (*off >= 0) {
        switch (EJS_PRIMSTR_GET_TYPE(n)) {
        case EJS_STRING_FLAT:
            if (*off < n->length) {
                int length_to_hash = MIN(*len, n->length - *off);
                hash = ucs2_hash (n->data.flat + *off, hash, length_to_hash);
                *len -= length_to_hash;
                *off = 0;
            }
            else {
                *off -= n->length;
            }
            break;
        case EJS_STRING_ROPE:
            hash = hash_dep (hash, n->data.rope.left, off, len);
            hash = hash_dep (hash, n->data.rope.right, off, len);
            break;
        case EJS_STRING_DEPENDENT:
            *off += n->data.dependent.off;
            hash = hash_dep(hash, n->data.dependent.dep, off, len);
            break;
        }
    }
    return hash;
}

static uint32_t
_ejs_primstring_hash_inner (EJSPrimString* primstr, int cur_hash)
{
    switch (EJS_PRIMSTR_GET_TYPE(primstr)) {
    case EJS_STRING_FLAT:
        return ucs2_hash (primstr->data.flat, cur_hash, primstr->length);
    case EJS_STRING_DEPENDENT: {
        int length = primstr->length;
        int off = 0;
        return hash_dep (cur_hash, primstr, &off, &length);
    }
    case EJS_STRING_ROPE: {
        int hash = _ejs_primstring_hash_inner (primstr->data.rope.left, cur_hash);
        return _ejs_primstring_hash_inner (primstr->data.rope.right, hash);
    }
    }
}

uint32_t
_ejs_primstring_hash (EJSPrimString* primstr)
{
    if (!EJS_PRIMSTR_HAS_HASH(primstr)) {
        primstr->hash = _ejs_primstring_hash_inner (primstr, 0);
        EJS_PRIMSTR_SET_HAS_HASH(primstr);
    }
    return primstr->hash;
}

uint32_t
_ejs_string_hash (ejsval str)
{
    EJS_ASSERT (EJSVAL_IS_STRING(str));

    return _ejs_primstring_hash (EJSVAL_TO_STRING_IMPL(str));
}

jschar
_ejs_string_char_code_at(EJSPrimString* primstr, int i)
{
    if (i >= primstr->length) {
        //printf ("char_code_at error\n");
        return (jschar)-1;
    }

    switch (EJS_PRIMSTR_GET_TYPE(primstr)) {
    case EJS_STRING_FLAT:
        return primstr->data.flat[i];
    case EJS_STRING_ROPE: {
        if (i >= primstr->data.rope.left->length) {
            return _ejs_string_char_code_at (primstr->data.rope.left, i);
        }
        else {
            return _ejs_string_char_code_at (primstr->data.rope.right, i - primstr->data.rope.left->length);
        }
    }
    case EJS_STRING_DEPENDENT: {
        return _ejs_string_char_code_at (primstr->data.dependent.dep, i + primstr->data.dependent.off);
    }
    default:
        EJS_NOT_IMPLEMENTED();
    }
}

char*
_ejs_string_to_utf8(EJSPrimString* primstr)
{
    int length = primstr->length * 4+1;
    char* buf = (char*)malloc(length);
    char *p = buf;

    for (int i = 0; i < primstr->length; i ++) {
        jschar ucs2 = _ejs_string_char_code_at(primstr, i);
        if (ucs2 == (jschar)-1) {
            free (buf);
            char error_buf[256];
            snprintf (error_buf, sizeof(error_buf), "error converting ucs2 to utf8, index %d\n", i);
            return strdup(error_buf);
        }
        int adv = ucs2_to_utf8_char (ucs2, p);
        if (adv < 1) {
            printf ("error converting ucs2 to utf8, index %d\n", i);
            free(buf);
            return NULL;
        }
        p += adv;
    }

    *p = 0;

    return buf;
}

void
_ejs_string_init_literal (const char *name, ejsval *val, EJSPrimString* str, jschar* ucs2_data, int32_t length)
{
    str->length = length;
    str->hash = 0;
    str->gc_header = (EJS_STRING_FLAT|EJS_PRIMSTR_HAS_OOL_BUFFER_MASK) << EJS_GC_USER_FLAGS_SHIFT;
    str->data.flat = ucs2_data;
    *val = STRING_TO_EJSVAL(str);
}

char*
_ejs_string_describe (ejsval str)
{
    return ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(str));
}

char*
_ejs_string_describe_prim (EJSPrimString *str)
{
    return ucs2_to_utf8(_ejs_primstring_flatten(str)->data.flat);
}
