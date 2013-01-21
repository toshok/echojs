/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <ctype.h>

#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-string.h"
#include "ejs-function.h"
#include "ejs-regexp.h"
#include "ejs-ops.h"

static ejsval _ejs_string_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr);
static EJSPropertyDesc* _ejs_string_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_string_specop_get_property (ejsval obj, ejsval propertyName);
static void      _ejs_string_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool   _ejs_string_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_string_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_string_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval _ejs_string_specop_default_value (ejsval obj, const char *hint);
static EJSBool  _ejs_string_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSObject* _ejs_string_specop_allocate ();
static void      _ejs_string_specop_finalize (EJSObject* obj);
static void      _ejs_string_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

EJSSpecOps _ejs_string_specops = {
    "String",
    _ejs_string_specop_get,
    _ejs_string_specop_get_own_property,
    _ejs_string_specop_get_property,
    _ejs_string_specop_put,
    _ejs_string_specop_can_put,
    _ejs_string_specop_has_property,
    _ejs_string_specop_delete,
    _ejs_string_specop_default_value,
    _ejs_string_specop_define_own_property,

    _ejs_string_specop_allocate,
    _ejs_string_specop_finalize,
    _ejs_string_specop_scan
};

ejsval _ejs_String;
ejsval _ejs_String__proto__;
ejsval _ejs_String_prototype;

static ejsval
_ejs_String_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (EJSVAL_IS_NULL(_this) || EJSVAL_IS_UNDEFINED(_this)) {
        if (argc > 0)
            return ToString(args[0]);
        else
            return _ejs_string_new_utf8("");
    }
    else {
        // called as a constructor
        EJSString* str = (EJSString*)EJSVAL_TO_OBJECT(_this);
        ((EJSObject*)str)->ops = &_ejs_string_specops;

        if (argc > 0) {
            str->primStr = ToString(args[0]);
        }
        else {
            str->primStr = _ejs_string_new_utf8("");
        }
        return _this;
    }
}

static ejsval
_ejs_String_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSString *str = (EJSString*)EJSVAL_TO_OBJECT(_this);

    return _ejs_string_new_utf8 (EJSVAL_TO_FLAT_STRING(str->primStr));
}

static ejsval
_ejs_String_prototype_replace (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (argc == 0)
        return _this;

    ejsval thisStr = ToString(_this);
    ejsval searchValue = args[0];
    ejsval replaceValue = argc > 1 ? args[1] : _ejs_undefined;

    if (EJSVAL_IS_OBJECT(searchValue) && !strcmp (CLASSNAME(EJSVAL_TO_OBJECT(searchValue)), "RegExp")){
        EJS_NOT_IMPLEMENTED();
    }
    else {
        ejsval searchValueStr = ToString(searchValue);
        ejsval replaceValueStr = ToString(replaceValue);
        char *p = strstr (EJSVAL_TO_FLAT_STRING(thisStr), EJSVAL_TO_FLAT_STRING(searchValueStr));
        if (p == NULL)
            return _this;
        else {
            int len1 = p - EJSVAL_TO_FLAT_STRING(thisStr);
            int len2 = EJSVAL_TO_STRLEN(replaceValueStr);
            int len3 = strlen(p + EJSVAL_TO_STRLEN(searchValueStr));

            int new_len = len1;
            new_len += len2;
            new_len += len3;
            new_len += 1; // for the \0

            char* result = (char*)calloc(new_len, 1);
            char*p = result;
            strncpy (p, EJSVAL_TO_FLAT_STRING(thisStr), len1); p += len1;
            strcpy (p, EJSVAL_TO_FLAT_STRING(replaceValueStr)); p += len2;
            strcpy (p, p + EJSVAL_TO_STRLEN(searchValueStr));

            ejsval rv = _ejs_string_new_utf8 (result);
            free (result);
            return rv;
        }
    }
}

static ejsval
_ejs_String_prototype_charAt (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval primStr;

    if (EJSVAL_IS_STRING(_this)) {
        primStr = _this;
    }
    else {
        EJSString *str = (EJSString*)EJSVAL_TO_OBJECT(_this);
        primStr = str->primStr;
    }

    int idx = 0;
    if (argc > 0 && EJSVAL_IS_NUMBER(args[0])) {
        idx = (int)EJSVAL_TO_NUMBER(args[0]);
    }

    if (idx < 0 || idx > EJSVAL_TO_STRLEN(primStr))
        return _ejs_string_new_utf8 ("");

    char c[2];
    c[0] = EJSVAL_TO_FLAT_STRING(primStr)[idx];
    c[1] = '\0';
    return _ejs_string_new_utf8 (c);
}

static ejsval
_ejs_String_prototype_charCodeAt (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval primStr;

    if (EJSVAL_IS_STRING(_this)) {
        primStr = _this;
    }
    else {
        EJSString *str = (EJSString*)EJSVAL_TO_OBJECT(_this);
        primStr = str->primStr;
    }

    int idx = 0;
    if (argc > 0 && EJSVAL_IS_NUMBER(args[0])) {
        idx = (int)EJSVAL_TO_NUMBER(args[0]);
    }

    if (idx < 0 || idx >= EJSVAL_TO_STRLEN(primStr))
        return _ejs_nan;

    return NUMBER_TO_EJSVAL (EJSVAL_TO_FLAT_STRING(primStr)[idx]);
}

static ejsval
_ejs_String_prototype_concat (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_indexOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    int idx = -1;
    if (argc == 0)
        return NUMBER_TO_EJSVAL(idx);

    ejsval haystack = ToString(_this);
    char* haystack_cstr;
    if (EJSVAL_IS_STRING(haystack)) {
        haystack_cstr = EJSVAL_TO_FLAT_STRING(haystack);
    }
    else {
        haystack_cstr = EJSVAL_TO_FLAT_STRING(((EJSString*)EJSVAL_TO_OBJECT(haystack))->primStr);
    }

    ejsval needle = ToString(args[0]);
    char *needle_cstr;
    if (EJSVAL_IS_STRING(needle)) {
        needle_cstr = EJSVAL_TO_FLAT_STRING(needle);
    }
    else {
        needle_cstr = EJSVAL_TO_FLAT_STRING(((EJSString*)EJSVAL_TO_OBJECT(needle))->primStr);
    }
  
    char* p = strstr(haystack_cstr, needle_cstr);
    if (p == NULL)
        return NUMBER_TO_EJSVAL(idx);

    return NUMBER_TO_EJSVAL (p - haystack_cstr);
}

static ejsval
_ejs_String_prototype_lastIndexOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_localeCompare (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_match (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_search (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_substring (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

// ECMA262: B.2.3
static ejsval
_ejs_String_prototype_substr (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval start = _ejs_undefined;
    ejsval length = _ejs_undefined;

    if (argc > 0) start = args[0];
    if (argc > 1) length = args[1];

    /* 1. Call ToString, giving it the this value as its argument. */
    ejsval Result1 = ToString(_this);

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
    char* result1_str = EJSVAL_TO_FLAT_STRING(Result1);
    return _ejs_string_new_utf8_len(result1_str + Result5, Result6);
}

static ejsval
_ejs_String_prototype_toLowerCase (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    /* 1. Call CheckObjectCoercible passing the this value as its argument. */
    /* 2. Let S be the result of calling ToString, giving it the this value as its argument. */
    ejsval S = ToString(_this);
    char* sstr = strdup(EJSVAL_TO_FLAT_STRING(S));

    /* 3. Let L be a String where each character of L is either the Unicode lowercase equivalent of the corresponding  */
    /*    character of S or the actual corresponding character of S if no Unicode lowercase equivalent exists. */
    char* p = sstr;
    while (*p) {
        *p = tolower(*p);
        p++;
    }

    free(sstr);
    ejsval L = _ejs_string_new_utf8(sstr);

    /* 4. Return L. */
    return L;
}

static ejsval
_ejs_String_prototype_toLocaleLowerCase (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_toUpperCase (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_toLocaleUpperCase (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_trim (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_valueOf (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
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
    char* sstr = EJSVAL_TO_FLAT_STRING(S);
    char* rstr = EJSVAL_TO_FLAT_STRING(R);
    for (int i = 0; i < r; i ++) {
        if (sstr[q+i] != rstr[i]) {
            MatchResultState rv = { MATCH_RESULT_FAILURE };
            return rv;
        }
    }

    /* 6. Let cap be an empty array of captures (see 15.10.2.1). */
    ejsval cap = _ejs_array_new(0);
    /* 7. Return the State (q+r, cap). (see 15.10.2.1) */
    MatchResultState rv = { MATCH_RESULT_SUCCESS, q+r, cap };
    return rv;
}

static ejsval
_ejs_String_prototype_split (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval separator = _ejs_undefined;
    ejsval limit = _ejs_undefined;

    if (argc > 0) separator = args[0];
    if (argc > 1) limit = args[1];

    /* 1. Call CheckObjectCoercible passing the this value as its argument. */
    /* 2. Let S be the result of calling ToString, giving it the this value as its argument. */
    ejsval S = ToString(_this);
    char *Sstr = EJSVAL_TO_FLAT_STRING(S);

    /* 3. Let A be a new array created as if by the expression new Array() where Array is the standard built-in  */
    /*    constructor with that name. */
    ejsval A = _ejs_array_new(0);

    /* 4. Let lengthA be 0. */
    int lengthA = 0;

    /* 5. If limit is undefined, let lim = 2^32–1; else let lim = ToUint32(limit). */
    uint32_t lim = (EJSVAL_IS_UNDEFINED(limit)) ? UINT32_MAX : ToUint32(limit);

    /* 6. Let s be the number of characters in S. */
    int s = EJSVAL_TO_STRLEN(S);

    /* 7. Let p = 0. */
    int p = 0;

    /* 8. If separator is a RegExp object (its [[Class]] is "RegExp"), let R = separator; otherwise let R =  */
    /*    ToString(separator). */
    ejsval R = (EJSVAL_IS_REGEXP(separator)) ? separator : ToString(separator);

    /* 9. If lim = 0, return A. */
    if (lim == 0)
        return A;

    /* 10. If separator is undefined, then */
    if (EJSVAL_IS_UNDEFINED(separator)) {
        /*     a. Call the [[DefineOwnProperty]] internal method of A with arguments "0", Property Descriptor  */
        /*        {[[Value]]: S, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
        _ejs_array_push_dense (A, 1, &S);

        /*     b. Return A. */
        return A;
    }
    /* 11. If s = 0, then */
    if (s == 0) {
        EJS_NOT_IMPLEMENTED();
        /*     a. Call SplitMatch(S, 0, R) and let z be its MatchResult result. */
        /*     b. If z is not failure, return A. */
        /*     c. Call the [[DefineOwnProperty]] internal method of A with arguments "0", Property Descriptor  */
        /*        {[[Value]]: S, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
        /*     d. Return A. */
        return A;
    }
    /* 12. Let q = p. */
    int q = p;

    /* 13. Repeat, while q != s */
    while (q != s) {
        /* a. Call SplitMatch(S, q, R) and let z be its MatchResult result. */
        MatchResultState z = SplitMatch(S, q, R);
        
        /* b. If z is failure, then let q = q+1. */
        if (z.type == MATCH_RESULT_FAILURE) {
            q++;
        }
        /* c. Else,  z is not failure */
        else {
            /*    i. z must be a State. Let e be z's endIndex and let cap be z's captures array. */
            int e = z.endIndex;
            ejsval cap = z.captures;

            /*    ii. If e = p, then let q = q+1. */
            if (e == p) {
                q++;
            }
            /*    iii. Else, e != p */
            else {
                /*         1. Let T be a String value equal to the substring of S consisting of the characters at  */
                /*            positions p (inclusive) through q (exclusive). */
                ejsval T = _ejs_string_new_utf8_len (Sstr + p, q-p);
                /*         2. Call the [[DefineOwnProperty]] internal method of A with arguments  */
                /*            ToString(lengthA), Property Descriptor {[[Value]]: T, [[Writable]]: true, */
                /*            [[Enumerable]]: true, [[Configurable]]: true}, and false. */
                _ejs_array_push_dense (A, 1, &T);

                /*         3. Increment lengthA by 1. */
                lengthA ++;
                /*         4. If lengthA = lim, return A. */
                if (lengthA == lim)
                    return A;
                /*         5. Let p = e. */
                p = e;
                /*         6. Let i = 0. */
                int i = 0;
                /*         7. Repeat, while i is not equal to the number of elements in cap. */
                while (i != EJS_ARRAY_LEN(cap)) {
                    /*            a Let i = i+1. */
                    i++;
                    /*            b Call the [[DefineOwnProperty]] internal method of A with arguments  */
                    /*              ToString(lengthA), Property Descriptor {[[Value]]: cap[i], [[Writable]]:  */
                    /*              true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
                    ejsval pushcap = EJS_ARRAY_ELEMENTS(cap)[i];
                    _ejs_array_push_dense (A, 1, &pushcap);
                    /*            c Increment lengthA by 1. */
                    lengthA ++;
                    /*            d If lengthA = lim, return A. */
                    if (lengthA == lim)
                        return A;
                }
                /*         8. Let q = p. */
                q = p;
            }
        }
    }
    /* 14. Let T be a String value equal to the substring of S consisting of the characters at positions p (inclusive)  */
    /*     through s (exclusive). */
    ejsval T = _ejs_string_new_utf8_len (Sstr + p, s-p);
    /* 15. Call the [[DefineOwnProperty]] internal method of A with arguments ToString(lengthA), Property Descriptor  */
    /*     {[[Value]]: T, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
    _ejs_array_push_dense (A, 1, &T);

    /* 16. Return A. */
    return A;
}

static ejsval
_ejs_String_prototype_slice (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // assert argc >= 1

    ejsval start = _ejs_undefined;
    ejsval end = _ejs_undefined;

    if (argc > 0) start = args[0];
    if (argc > 1) end = args[1];

    // Call CheckObjectCoercible passing the this value as its argument.
    // Let S be the result of calling ToString, giving it the this value as its argument.
    ejsval S = ToString(_this);
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
    return _ejs_string_new_utf8_len (EJSVAL_TO_FLAT_STRING(S) + from, span);
}

static ejsval
_ejs_String_fromCharCode (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    int length = argc;
    char* buf = (char*)malloc(length+1);
    for (int i = 0; i < argc; i ++) {
        uint16_t code_unit = ToUint16(args[i]);
        assert (code_unit < 256); // XXX we suck
        buf[i] = (char)code_unit;
    }
    buf[length] = 0;
    ejsval rv = _ejs_string_new_utf8(buf);
    free (buf);
    return rv;
}

static void
_ejs_string_init_proto()
{
    _ejs_gc_add_named_root (_ejs_String__proto__);
    _ejs_gc_add_named_root (_ejs_String_prototype);

    EJSFunction* __proto__ = _ejs_gc_new(EJSFunction);
    __proto__->name = _ejs_atom_Empty;
    __proto__->func = _ejs_Function_empty;
    __proto__->env = _ejs_null;

    EJSObject* prototype = _ejs_gc_new(EJSObject);

    _ejs_String__proto__ = OBJECT_TO_EJSVAL((EJSObject*)__proto__);
    _ejs_String_prototype = OBJECT_TO_EJSVAL(prototype);

    _ejs_init_object (prototype, _ejs_null, &_ejs_string_specops);
    _ejs_init_object ((EJSObject*)__proto__, _ejs_Object_prototype, &_ejs_function_specops);
}

void
_ejs_string_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    _ejs_string_init_proto();
  
    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_String, (EJSClosureFunc)_ejs_String_impl));
    _ejs_String = tmpobj;

    _ejs_object_setprop (_ejs_String,       _ejs_atom_prototype,  _ejs_String_prototype);

#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_String, EJS_STRINGIFY(x), _ejs_String_##x)
#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_String_prototype, EJS_STRINGIFY(x), _ejs_String_prototype_##x)

    PROTO_METHOD(charAt);
    PROTO_METHOD(charCodeAt);
    PROTO_METHOD(concat);
    PROTO_METHOD(indexOf);
    PROTO_METHOD(lastIndexOf);
    PROTO_METHOD(localeCompare);
    PROTO_METHOD(match);
    PROTO_METHOD(replace);
    PROTO_METHOD(search);
    PROTO_METHOD(slice);
    PROTO_METHOD(split);
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

#undef OBJ_METHOD
#undef PROTO_METHOD

    _ejs_object_setprop (global, _ejs_atom_String, _ejs_String);

    END_SHADOW_STACK_FRAME;
}

static ejsval
_ejs_string_specop_get (ejsval obj, ejsval propertyName, EJSBool isCStr)
{
    // check if propertyName is an integer, or a string that we can convert to an int
    EJSBool is_index = EJS_FALSE;
    int idx = 0;
    if (!isCStr && EJSVAL_IS_NUMBER(propertyName)) {
        double n = EJSVAL_TO_NUMBER(propertyName);
        if (floor(n) == n) {
            idx = (int)n;
            is_index = EJS_TRUE;
        }
    }

    EJSString* estr = (EJSString*)EJSVAL_TO_OBJECT(obj);
    if (is_index) {
        if (idx < 0 || idx > EJSVAL_TO_STRLEN(estr->primStr))
            return _ejs_undefined;
        char c[2];
        c[0] = EJSVAL_TO_FLAT_STRING(estr->primStr)[idx];
        c[1] = '\0';
        return _ejs_string_new_utf8 (c);
    }

    // we also handle the length getter here
    if ((isCStr && !strcmp("length", (char*)EJSVAL_TO_PRIVATE_PTR_IMPL(propertyName)))
        || (!isCStr && EJSVAL_IS_STRING(propertyName) && !strcmp ("length", EJSVAL_TO_FLAT_STRING(propertyName)))) {
        return NUMBER_TO_EJSVAL (EJSVAL_TO_STRLEN(estr->primStr));
    }

    // otherwise we fallback to the object implementation
    return _ejs_object_specops.get (obj, propertyName, isCStr);
}

static EJSPropertyDesc*
_ejs_string_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSPropertyDesc*
_ejs_string_specop_get_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_string_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_string_specop_can_put (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_string_specop_has_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_string_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    return _ejs_object_specops._delete (obj, propertyName, flag);
}

static ejsval
_ejs_string_specop_default_value (ejsval obj, const char *hint)
{
    return _ejs_object_specops.default_value (obj, hint);
}

static EJSBool
_ejs_string_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    return _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static EJSObject*
_ejs_string_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new (EJSString);
}

static void
_ejs_string_specop_finalize (EJSObject* obj)
{
    _ejs_object_specops.finalize (obj);
}

static void
_ejs_string_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSString* ejss = (EJSString*)obj;
    scan_func (ejss->primStr);
    _ejs_object_specops.scan (obj, scan_func);
}
