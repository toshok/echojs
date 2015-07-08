/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <string.h>

#include "ejs-array.h"
#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-regexp.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-error.h"
#include "ejs-symbol.h"
#include "ejs-proxy.h"
#include "ejs-number.h"

#include "pcre.h"

static ejsval _ejs_RegExp_impl(ejsval env, ejsval _this, uint32_t argc,
                               ejsval *args);

static const unsigned char *pcre16_tables;

ejsval _ejs_regexp_new(ejsval pattern, ejsval flags) {
    EJSRegExp *rv = _ejs_gc_new(EJSRegExp);

    _ejs_init_object((EJSObject *)rv, _ejs_RegExp_prototype,
                     &_ejs_RegExp_specops);

    ejsval args[2] = {pattern, flags};

    return _ejs_RegExp_impl(_ejs_null, OBJECT_TO_EJSVAL(rv), 2, args);
}

ejsval _ejs_regexp_new_utf8(const char *pattern, const char *flags) {
    return _ejs_regexp_new(_ejs_string_new_utf8(pattern),
                           _ejs_string_new_utf8(flags));
}

ejsval _ejs_regexp_replace(ejsval str, ejsval search_re, ejsval replace) {
    EJSRegExp *re = (EJSRegExp *)EJSVAL_TO_OBJECT(search_re);

    pcre16_extra extra;
    memset(&extra, 0, sizeof(extra));

    pcre16 *code = (pcre16 *)re->compiled_pattern;

    int capture_count;
    pcre16_fullinfo(code, NULL, PCRE_INFO_CAPTURECOUNT, &capture_count);

    int ovec_count = 3 * (1 + capture_count);
    int *ovec = malloc(sizeof(int) * ovec_count);
    int cur_off = 0;

    do {
        EJSPrimString *flat_str = _ejs_string_flatten(str);
        jschar *chars_str = flat_str->data.flat;

        int rv = pcre16_exec(code, &extra, chars_str, flat_str->length, cur_off,
                             PCRE_NO_UTF16_CHECK, ovec, ovec_count);

        if (rv < 0)
            break;

        ejsval replaceval;

        if (EJSVAL_IS_FUNCTION(replace)) {
            ejsval substr_match =
                _ejs_string_new_substring(str, ovec[0], ovec[1] - ovec[0]);
            ejsval capture =
                _ejs_string_new_substring(str, ovec[2], ovec[3] - ovec[2]);

            _ejs_log(
                "substring match is %s\n",
                ucs2_to_utf8(_ejs_string_flatten(substr_match)->data.flat));
            _ejs_log("capture is %s\n",
                     ucs2_to_utf8(_ejs_string_flatten(capture)->data.flat));

            int argc = 3;
            ejsval args[3];

            args[0] = substr_match;
            args[1] = capture;
            args[2] = _ejs_undefined;

            replaceval = ToString(
                _ejs_invoke_closure(replace, _ejs_undefined, argc, args));
        } else {
            replaceval = ToString(replace);
        }

        if (ovec[0] == 0) {
            // we matched from the beginning of the string, so nothing from
            // there to prepend
            str = _ejs_string_concat(
                replaceval, _ejs_string_new_substring(
                                str, ovec[1], flat_str->length - ovec[1]));
        } else {
            str = _ejs_string_concatv(
                _ejs_string_new_substring(str, 0, ovec[0]), replaceval,
                _ejs_string_new_substring(str, ovec[1],
                                          flat_str->length - ovec[1]),
                _ejs_null);
        }

        cur_off = ovec[1];

        // if the RegExp object was created without a 'g' flag, only replace the
        // first match
        if (!re->global)
            break;
    } while (EJS_TRUE);

    free(ovec);
    return str;
}

ejsval _ejs_RegExp EJSVAL_ALIGNMENT;
ejsval _ejs_RegExp_prototype EJSVAL_ALIGNMENT;

static ejsval _ejs_RegExp_impl(ejsval env, ejsval _this, uint32_t argc,
                               ejsval *args) {
    EJSRegExp *re;

    if (EJSVAL_IS_UNDEFINED(_this)) {
        // called as a function
        _this = _ejs_object_new(_ejs_RegExp_prototype, &_ejs_RegExp_specops);
    }

    re = (EJSRegExp *)EJSVAL_TO_OBJECT(_this);

    re->pattern = _ejs_undefined;
    re->flags = _ejs_undefined;

    ejsval pattern = _ejs_undefined;

    if (argc > 0)
        pattern = args[0];
    if (argc > 1)
        re->flags = args[1];

    if (EJSVAL_IS_OBJECT(pattern) && EJSVAL_IS_REGEXP(pattern))
        re->pattern = ((EJSRegExp *)EJSVAL_TO_OBJECT(pattern))->pattern;
    else
        re->pattern = pattern;

    if (!EJSVAL_IS_STRING(re->pattern))
        EJS_NOT_IMPLEMENTED();

    EJSPrimString *flat_pattern = _ejs_string_flatten(re->pattern);
    jschar *chars = flat_pattern->data.flat;

    const char *pcre_error;
    int pcre_erroffset;

    re->compiled_pattern =
        pcre16_compile(chars, PCRE_UTF16 | PCRE_NO_UTF16_CHECK, &pcre_error,
                       &pcre_erroffset, pcre16_tables);

    _ejs_object_define_value_property(_this, _ejs_atom_source, re->pattern,
                                      EJS_PROP_NOT_ENUMERABLE |
                                          EJS_PROP_NOT_CONFIGURABLE |
                                          EJS_PROP_NOT_WRITABLE);
    _ejs_object_define_value_property(
        _this, _ejs_atom_lastIndex, NUMBER_TO_EJSVAL(0),
        EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE |
            EJS_PROP_WRITABLE);

    if (EJSVAL_IS_STRING(re->flags)) {
        EJSPrimString *flat_flags = _ejs_string_flatten(re->flags);
        chars = flat_flags->data.flat;

        for (int i = 0; i < flat_flags->length; i++) {
            if (chars[i] == 'g' && !re->global) {
                re->global = EJS_TRUE;
                continue;
            } else if (chars[i] == 'i' && !re->ignoreCase) {
                re->ignoreCase = EJS_TRUE;
                continue;
            } else if (chars[i] == 'm' && !re->multiline) {
                re->multiline = EJS_TRUE;
                continue;
            } else if (chars[i] == 'y' && !re->sticky) {
                re->sticky = EJS_TRUE;
                continue;
            } else if (chars[i] == 'u' && !re->unicode) {
                re->unicode = EJS_TRUE;
                continue;
            }
            _ejs_throw_nativeerror_utf8(
                EJS_SYNTAX_ERROR,
                "Invalid flag supplied to RegExp constructor");
        }
    }

    return _this;
}

// ES6 21.2.5.2.2
// Runtime Semantics: RegExpBuiltinExec ( R, S ) Abstract Operation
static ejsval RegExpBuiltinExec(ejsval R, ejsval S) {
    // 1. Assert: R is an initialized RegExp instance.
    EJS_ASSERT(EJSVAL_IS_REGEXP(R));
    // 2. Assert: Type(S) is String.
    EJS_ASSERT(EJSVAL_IS_STRING_TYPE(S));

    EJSRegExp *re = (EJSRegExp *)EJSVAL_TO_OBJECT(R);

    // 3. Let length be the number of code units in S.
    int length = EJSVAL_TO_STRLEN(S);

    // 4. Let lastIndex be Get(R,"lastIndex").
    ejsval lastIndex = Get(R, _ejs_atom_lastIndex);
    // 5. Let i be ToLength(lastIndex).
    // 6. ReturnIfAbrupt(i).
    int64_t i = ToLength(lastIndex);

    // 7. Let global be ToBoolean(Get(R, "global")).
    // 8. ReturnIfAbrupt(global).
    EJSBool global = ToEJSBool(Get(R, _ejs_atom_global));

    // 9. Let sticky be ToBoolean(Get(R, "sticky")).
    // 10. ReturnIfAbrupt(sticky).
    EJSBool sticky = ToEJSBool(Get(R, _ejs_atom_sticky));

    // 11. If global is false and sticky is false, then let i = 0.
    if (!global && !sticky)
        i = 0;

    ejsval subject = S;

    pcre16_extra extra;
    memset(&extra, 0, sizeof(extra));

    EJSPrimString *flat_subject = _ejs_string_flatten(subject);
    jschar *subject_chars = flat_subject->data.flat;

    int ovec[60];

    // 12. Let matcher be the value of R’s [[RegExpMatcher]] internal slot.
    pcre16 *matcher = (pcre16 *)re->compiled_pattern;

    // 13. Let flags be the value of R’s [[OriginalFlags]] internal slot.
    // XXX

    // 14. If flags contains "u" then let fullUnicode be true, else let
    // fullUnicode be false.
    EJSBool fullUnicode = re->unicode;

    // 15. Let matchSucceeded be false.
    EJSBool matchSucceeded = EJS_FALSE;

    int r;

#if false
    // 16. Repeat, while matchSucceeded is false
    while (!matchSucceeded) {
        //     a. If i < 0 or i > length, then
        if (i < 0 || i > length) {
            // i. Let putStatus be Put(R, "lastIndex", 0, true).
            // ii. ReturnIfAbrupt(putStatus).
            Put(R, _ejs_atom_lastIndex, NUMBER_TO_EJSVAL(0), EJS_TRUE);
            // iii. Return null.
            return _ejs_null;
        }

        //  b. Let r be the result of calling matcher with arguments S and i.
        r = pcre16_exec(matcher, &extra, subject_chars, length, i,
                            PCRE_NO_UTF16_CHECK, ovec, 3);

        //     c. If r is failure, then
        if (r == PCRE_ERROR_NOMATCH) {
            // i. If sticky is true, then
            if (sticky) {
                // 1. Let putStatus be Put(R, "lastIndex", 0, true).
                // 2. ReturnIfAbrupt(putStatus).
                Put(R, _ejs_atom_lastIndex, NUMBER_TO_EJSVAL(0), EJS_TRUE);
                // 3. Return null.
                return _ejs_null;
            }
            // ii. Let i = i+1.
            i = i + 1;
        }
        else {
            //     d. else
            //        i. Assert: r is a State.
            //        ii. Set matchSucceeded to true.
            matchSucceeded = EJS_TRUE;
        }
    }
#else
    r = pcre16_exec(matcher, &extra, subject_chars, length, i,
                    PCRE_NO_UTF16_CHECK, ovec, sizeof(ovec) / sizeof(ovec[0]));
    if (r == PCRE_ERROR_NOMATCH) {
        Put(R, _ejs_atom_lastIndex, NUMBER_TO_EJSVAL(0), EJS_TRUE);
        return _ejs_null;
    }

    i = ovec[0];
#endif

    // 17. Let e be r's endIndex value.
    int e = ovec[1];

    // 18. If fullUnicode is true, then
    if (fullUnicode) {
        //     a. e is an index into the Input character list, derived from S,
        //     matched by matcher. Let eUTF be the smallest
        //        index into S that corresponds to the character at element e of
        //        Input. If e is greater than the length of
        //        Input, then eUTF is 1 + the number of code units in S.
        //     b. Let e be eUTF.
        EJS_NOT_IMPLEMENTED();
    }
    // 19. If global is true or sticky is true,
    if (global || sticky) {
        // a. Let putStatus be the result of Put(R, "lastIndex", e, true).
        // b. ReturnIfAbrupt(putStatus).
        Put(R, _ejs_atom_lastIndex, NUMBER_TO_EJSVAL(e), EJS_TRUE);
    }
    // 20. Let n be the length of r's captures List. (This is the same value as
    // 21.2.2.1's NcapturingParens.)
    int n = r - 1;

    // 21. Let A be the result of the abstract operation ArrayCreate(n + 1).
    ejsval A = _ejs_array_new(n + 1, EJS_FALSE);
    // 22. Assert: The value of A’s "length" property is n + 1.
    // 23. Let matchIndex be i.
    int matchIndex = i;
    // 24. Assert: The following CreateDataProperty calls will not result in an
    // abrupt completion.
    // 25. Perform CreateDataProperty(A, "index", matchIndex).
    _ejs_object_setprop(A, _ejs_atom_index, NUMBER_TO_EJSVAL(matchIndex));
    // 26. Perform CreateDataProperty(A, "input", S).
    _ejs_object_setprop(A, _ejs_atom_input, S);
    // 27. Let matchedSubstr be the matched substring (i.e. the portion of S
    // between offset i inclusive and offset e exclusive).
    ejsval matchedSubstr = _ejs_string_new_substring(S, i, e - i);
    // 28. Perform CreateDataProperty(A, "0", matchedSubstr).
    EJS_DENSE_ARRAY_ELEMENTS(A)[0] = matchedSubstr;
    // 29. For each integer i such that i > 0 and i <= n
    for (int i = 1; i <= n; i++) {
        // a. Let captureI be ith element of r's captures List.
        ejsval capturedValue;

        // b. If captureI is undefined, then let capturedValue be undefined.
        if (ovec[i * 2] == ovec[i * 2 + 1]) {
            capturedValue = _ejs_undefined;
        } else {
            // c. Else if fullUnicode is true,
            if (fullUnicode) {
                // i. Assert: captureI is a List of code points.
                // ii. Let capturedValue be a string whose code units are the
                // UTF-16Encoding (10.1.1) of the code points of capture.
                EJS_NOT_IMPLEMENTED();
            }
            // d. Else, fullUnicode is false,
            else {
                //    i. Assert: captureI is a List of code units.
                //    ii. Let capturedValue be a string consisting of the code
                //    units of captureI.
                capturedValue = _ejs_string_new_substring(
                    S, ovec[i * 2], ovec[i * 2 + 1] - ovec[i * 2]);
            }
        }
        // e. Perform CreateDataProperty(A, ToString(i) , capturedValue).
        EJS_DENSE_ARRAY_ELEMENTS(A)[i] = capturedValue;
    }
    // 30. Return A.
    return A;
}

static ejsval RegExpExec(ejsval R, ejsval S) {
    // 1. Assert: Type(R) is Object.
    // 2. Assert: Type(S) is String.

    // 3. Let exec be Get(R, "exec").
    // 4. ReturnIfAbrupt(exec).
    ejsval exec = Get(R, _ejs_atom_exec);

    // 5. If IsCallable(exec) is true, then
    if (EJSVAL_IS_FUNCTION(exec)) {
        // a. Let result be Call(exec, R, «S»).
        // b. ReturnIfAbrupt(result).
        ejsval result = _ejs_invoke_closure(exec, R, 1, &S);

        // c. If Type(result) is neither Object or Null, then throw a TypeError
        // exception.
        if (!EJSVAL_IS_OBJECT(result) && !EJSVAL_IS_NULL(result))
            _ejs_throw_nativeerror_utf8(
                EJS_TYPE_ERROR, "'exec' returned non-object, non-null value");

        // d. Return(result).
        return result;
    }

    // 6. If R does not have a [[RegExpMatcher]] internal slot, then throw a
    // TypeError exception.
    // 7. If the value of R’s [[RegExpMatcher]] internal slot is undefined, then
    // throw a TypeError exception.
    if (!EJSVAL_IS_REGEXP(R))
        _ejs_throw_nativeerror_utf8(EJS_TYPE_ERROR, "'this' is not a RegExp");

    // 8. Return RegExpBuiltinExec(R, S).
    return RegExpBuiltinExec(R, S);
}

// ES6 21.2.5.2
// RegExp.prototype.exec ( string )
ejsval _ejs_RegExp_prototype_exec(ejsval env, ejsval _this, uint32_t argc,
                                  ejsval *args) {
    ejsval string = _ejs_undefined;
    if (argc > 0)
        string = args[0];

    // 1. Let R be the this value.
    ejsval R = _this;

    // 2. If Type(R) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(R))
        _ejs_throw_nativeerror_utf8(
            EJS_TYPE_ERROR, "non-object 'this' in RegExp.prototype.exec");

    // 3. If R does not have a [[RegExpMatcher]] internal slot, then throw a
    // TypeError exception.
    // 4. If the value of R’s [[RegExpMatcher]] internal slot is undefined, then
    // throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(R))
        _ejs_throw_nativeerror_utf8(
            EJS_TYPE_ERROR, "non-RegExp 'this' in RegExp.prototype.exec");

    // 5. Let S be ToString(string)
    // 6. ReturnIfAbrupt(S).
    ejsval S = ToString(string);

    // 7. Return RegExpBuiltinExec(R, S).
    return RegExpBuiltinExec(R, S);
}

// ECMA262: 15.10.6.3
static ejsval _ejs_RegExp_prototype_test(ejsval env, ejsval _this,
                                         uint32_t argc, ejsval *args) {
    if (!EJSVAL_IS_REGEXP(_this))
        EJS_NOT_IMPLEMENTED();

    EJSRegExp *re = (EJSRegExp *)EJSVAL_TO_OBJECT(_this);

    ejsval subject = _ejs_undefined;
    if (argc > 0)
        subject = args[0];

    pcre16_extra extra;
    memset(&extra, 0, sizeof(extra));

    EJSPrimString *flat_subject = _ejs_string_flatten(subject);
    jschar *subject_chars = flat_subject->data.flat;

    int ovec[3];

    int rv = pcre16_exec((pcre16 *)re->compiled_pattern, &extra, subject_chars,
                         flat_subject->length, 0, PCRE_NO_UTF16_CHECK, ovec, 3);

    return rv == PCRE_ERROR_NOMATCH ? _ejs_false : _ejs_true;
}

static ejsval _ejs_RegExp_prototype_toString(ejsval env, ejsval _this,
                                             uint32_t argc, ejsval *args) {
    EJSRegExp *re = (EJSRegExp *)EJSVAL_TO_OBJECT(_this);

    return _ejs_string_concatv(_ejs_atom_slash, re->pattern, _ejs_atom_slash,
                               re->flags, _ejs_null);
}

static ejsval _ejs_RegExp_prototype_get_global(ejsval env, ejsval _this,
                                               uint32_t argc, ejsval *args) {
    EJSRegExp *re = (EJSRegExp *)EJSVAL_TO_OBJECT(_this);
    return BOOLEAN_TO_EJSVAL(re->global);
}

static ejsval _ejs_RegExp_prototype_get_ignoreCase(ejsval env, ejsval _this,
                                                   uint32_t argc,
                                                   ejsval *args) {
    EJSRegExp *re = (EJSRegExp *)EJSVAL_TO_OBJECT(_this);
    return BOOLEAN_TO_EJSVAL(re->ignoreCase);
}

static ejsval _ejs_RegExp_prototype_get_lastIndex(ejsval env, ejsval _this,
                                                  uint32_t argc, ejsval *args) {
    EJSRegExp *re = (EJSRegExp *)EJSVAL_TO_OBJECT(_this);
    return NUMBER_TO_EJSVAL(re->lastIndex);
}

static ejsval _ejs_RegExp_prototype_get_multiline(ejsval env, ejsval _this,
                                                  uint32_t argc, ejsval *args) {
    EJSRegExp *re = (EJSRegExp *)EJSVAL_TO_OBJECT(_this);
    return BOOLEAN_TO_EJSVAL(re->multiline);
}

static ejsval _ejs_RegExp_prototype_get_sticky(ejsval env, ejsval _this,
                                               uint32_t argc, ejsval *args) {
    EJSRegExp *re = (EJSRegExp *)EJSVAL_TO_OBJECT(_this);
    return BOOLEAN_TO_EJSVAL(re->sticky);
}

static ejsval _ejs_RegExp_prototype_get_unicode(ejsval env, ejsval _this,
                                                uint32_t argc, ejsval *args) {
    EJSRegExp *re = (EJSRegExp *)EJSVAL_TO_OBJECT(_this);
    return BOOLEAN_TO_EJSVAL(re->unicode);
}

static ejsval _ejs_RegExp_prototype_get_source(ejsval env, ejsval _this,
                                               uint32_t argc, ejsval *args) {
    EJSRegExp *re = (EJSRegExp *)EJSVAL_TO_OBJECT(_this);
    return re->pattern;
}

// ES6 21.2.5.3
// get RegExp.prototype.flags ( )
static ejsval _ejs_RegExp_prototype_get_flags(ejsval env, ejsval _this,
                                              uint32_t argc, ejsval *args) {
    // 1. Let R be the this value.
    ejsval R = _this;

    // 2. If Type(R) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(R))
        _ejs_throw_nativeerror_utf8(
            EJS_TYPE_ERROR,
            "get Regexp.prototype.flags called with non-object 'this'");

    // 3. Let result be the empty String.
    char result_buf[6];
    memset(result_buf, 0, sizeof(result_buf));
    char *p = result_buf;

    // 4. Let global be ToBoolean(Get(R, "global")).
    // 5. ReturnIfAbrupt(global).
    EJSBool global = ToEJSBool(Get(R, _ejs_atom_global));

    // 6. If global is true, then append "g" as the last code unit of result.
    if (global)
        *p++ = 'g';

    // 7. Let ignoreCase be ToBoolean(Get(R, "ignoreCase")).
    // 8. ReturnIfAbrupt(ignoreCase).
    EJSBool ignoreCase = ToEJSBool(Get(R, _ejs_atom_ignoreCase));

    // 9. If ignoreCase is true, then append "i" as the last code unit of
    // result.
    if (ignoreCase)
        *p++ = 'i';

    // 10. Let multiline be ToBoolean(Get(R, "multiline")).
    // 11. ReturnIfAbrupt(multiline).
    EJSBool multiline = ToEJSBool(Get(R, _ejs_atom_multiline));

    // 12. If multiline is true, then append "m" as the last code unit of
    // result.
    if (multiline)
        *p++ = 'm';

    // 13. Let sticky be ToBoolean(Get(R, "sticky")).
    // 14. ReturnIfAbrupt(sticky).
    EJSBool sticky = ToEJSBool(Get(R, _ejs_atom_sticky));

    // 15. If sticky is true, then append "y" as the last code unit of result.
    if (sticky)
        *p++ = 'y';

    // 16. Let unicode be ToBoolean(Get(R, "unicode")).
    // 17. ReturnIfAbrupt(unicode).
    EJSBool unicode = ToEJSBool(Get(R, _ejs_atom_unicode));

    // 18. If unicode is true, then append "u" as the last code unit of result.
    if (unicode)
        *p++ = 'u';

    // 19. Return result.
    return _ejs_string_new_utf8(result_buf);
}

static ejsval _ejs_RegExp_get_species(ejsval env, ejsval _this, uint32_t argc,
                                      ejsval *args) {
    return _ejs_RegExp;
}

static ejsval _ejs_RegExp_create(ejsval env, ejsval _this, uint32_t argc,
                                 ejsval *args) {
    // 1. Let F be the this value.
    ejsval F = _this;

    if (!EJSVAL_IS_CONSTRUCTOR(F))
        _ejs_throw_nativeerror_utf8(
            EJS_TYPE_ERROR,
            "'this' in RegExp[Symbol.create] is not a constructor");

    EJSObject *F_ = EJSVAL_TO_OBJECT(F);

    // 2. Let obj be the result of calling
    // OrdinaryCreateFromConstructor(constructor, "%RegExpPrototype%", (
    // [[RegExpMatcher]], [[OriginalSource]], [[OriginalFlags]])).
    ejsval proto = OP(F_, Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto))
        proto = _ejs_RegExp_prototype;

    EJSRegExp *re = (EJSRegExp *)_ejs_gc_new(EJSRegExp);
    _ejs_init_object((EJSObject *)re, proto, &_ejs_RegExp_specops);

    re->pattern = _ejs_undefined;
    re->flags = _ejs_undefined;

    return OBJECT_TO_EJSVAL((EJSObject *)re);
}

// ES6 21.2.5.6
// RegExp.prototype [ @@match ] ( string )
static ejsval _ejs_RegExp_prototype_match(ejsval env, ejsval _this,
                                          uint32_t argc, ejsval *args) {
    ejsval string = _ejs_undefined;
    if (argc > 0)
        string = args[0];

    // 1. Let rx be the this value.
    ejsval rx = _this;

    // 2. If Type(rx) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(rx))
        _ejs_throw_nativeerror_utf8(
            EJS_TYPE_ERROR,
            "Regexp[Symbol.match] called with non-object 'this'");

    // 3. Let S be ToString(string)
    // 4. ReturnIfAbrupt(S).
    ejsval S = ToString(string);

    // 5. Let global be ToBoolean(Get(rx, "global")).
    // 6. ReturnIfAbrupt(global).
    ejsval global = ToBoolean(Get(rx, _ejs_atom_global));

    // 7. If global is not true, then
    if (!EJSVAL_TO_BOOLEAN(global)) {
        // a. Return the result of RegExpExec(rx, S).
        return RegExpExec(rx, S);
    }
    // 8. Else global is true,
    else {
        // a. Let putStatus be Put(rx, "lastIndex", 0, true).
        // b. ReturnIfAbrupt(putStatus).
        Put(rx, _ejs_atom_lastIndex, NUMBER_TO_EJSVAL(0), EJS_TRUE);

        // c. Let A be ArrayCreate(0).
        ejsval A = _ejs_array_new(0, EJS_FALSE);

        // d. Let previousLastIndex be 0.
        int previousLastIndex = 0;

        // e. Let n be 0.
        int n = 0;

        // f. Repeat,
        while (EJS_TRUE) {
            //   i. Let result be RegExpExec(rx, S).
            //  ii. ReturnIfAbrupt(result).
            ejsval result = RegExpExec(rx, S);

            // iii. If result is null, then
            if (EJSVAL_IS_NULL(result)) {
                // 1. If n=0, then return null.
                if (n == 0)
                    return _ejs_null;
                else
                    // 2. Else, return A.
                    return A;
            }
            // iv. Else result is not null,
            else {
                // 1. Let thisIndex be ToInteger(Get(rx, "lastIndex")).
                // 2. ReturnIfAbrupt(thisIndex).
                int thisIndex = ToInteger(Get(rx, _ejs_atom_lastIndex));

                // 3. If thisIndex = previousLastIndex then
                if (thisIndex == previousLastIndex) {
                    // a. Let putStatus be Put(rx, "lastIndex", thisIndex+1,
                    // true).
                    // b. ReturnIfAbrupt(putStatus).
                    Put(rx, _ejs_atom_lastIndex,
                        NUMBER_TO_EJSVAL(thisIndex + 1), EJS_TRUE);
                    // c. Set previousLastIndex to thisIndex+1.
                    previousLastIndex = thisIndex + 1;
                }
                // 4. Else,
                else {
                    // a. Set previousLastIndex to thisIndex.
                    previousLastIndex = thisIndex;
                }
                // 5. Let matchStr be Get(result, "0").
                ejsval matchStr = Get(result, _ejs_atom_0);

                // 6. Let status be CreateDataProperty(A, ToString(n),
                // matchStr).
                // 7. Assert: status is true.
                _ejs_object_define_value_property(
                    A, ToString(NUMBER_TO_EJSVAL(n)), matchStr,
                    EJS_PROP_FLAGS_ENUMERABLE | EJS_PROP_FLAGS_CONFIGURABLE |
                        EJS_PROP_FLAGS_WRITABLE);
                // 8. Increment n.
                n++;
            }
        }
    }
}

// ES6 21.2.5.8
//  RegExp.prototype [ @@replace ] ( string, replaceValue )
static ejsval _ejs_RegExp_prototype_replace(ejsval env, ejsval _this,
                                            uint32_t argc, ejsval *args) {
    ejsval string = _ejs_undefined;
    if (argc > 0)
        string = args[0];

    ejsval replaceValue = _ejs_undefined;
    if (argc > 1)
        replaceValue = args[1];

    // 1. Let rx be the this value.
    ejsval rx = _this;

    // 2. If Type(rx) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(rx))
        _ejs_throw_nativeerror_utf8(
            EJS_TYPE_ERROR,
            "Regexp.prototype[Symbol.replace] called with non-object 'this'");

    // 3. Let S be ToString(string).
    // 4. ReturnIfAbrupt(S).
    ejsval S = ToString(string);

    // 5. Let lengthS be the number of code unit elements in S.
    int lengthS = EJSVAL_TO_STRLEN(S);

    // 6. Let functionalReplace be IsCallable(replaceValue).
    EJSBool functionalReplace = EJSVAL_IS_FUNCTION(replaceValue);

    // 7. If functionalReplace is false, then
    if (!functionalReplace) {
        // a. Let replaceValue be ToString(replaceValue).
        // b. ReturnIfAbrupt(replaceValue).
        replaceValue = ToString(replaceValue);
    }
    // 8. Let global be ToBoolean(Get(rx, "global")).
    // 9. ReturnIfAbrupt(global).
    EJSBool global = ToEJSBool(Get(rx, _ejs_atom_global));

    // 10. If global is true, then
    if (global) {
        // a. Let putStatus be Put(rx, "lastIndex", 0, true).
        // b. ReturnIfAbrupt(putStatus).
        Put(rx, _ejs_atom_lastIndex, NUMBER_TO_EJSVAL(0), EJS_TRUE);
    }
    // 11. Let previousLastIndex be 0.
    int previousLastIndex = 0;
    // 12. Let results be a new empty List.
    ejsval results = _ejs_array_new(0, EJS_FALSE);

    // 13. Let done be false.
    EJSBool done = EJS_FALSE;
    // 14. Repeat, while done is false
    while (!done) {
        // a. Let result be RegExpExec(rx, S).
        // b. ReturnIfAbrupt(result).
        ejsval result = RegExpExec(rx, S);

        // c. If result is null, then set done to true.
        if (EJSVAL_IS_NULL(result))
            done = EJS_TRUE;
        // d. Else result is not null,
        else {
            // i. Append result to the end of results.
            _ejs_array_push_dense(results, 1, &result);

            // ii. If global is false, then set done to true.
            if (!global)
                done = EJS_TRUE;
            // iii. Else,
            else {
                // 1. Let matchStr be ToString(Get(result, "0")).
                // 2. ReturnIfAbrupt(matchStr).
                ejsval matchStr = ToString(Get(result, _ejs_atom_0));

                // 3. If matchStr is the empty String, then
                if (EJSVAL_TO_BOOLEAN(
                        _ejs_op_strict_eq(matchStr, _ejs_atom_empty))) {
                    //    a. Let thisIndex be ToLength(Get(rx, "lastIndex")).
                    //    b. ReturnIfAbrupt(thisIndex).
                    int64_t thisIndex = ToLength(Get(rx, _ejs_atom_lastIndex));

                    //    c. Let putStatus be Put(rx, "lastIndex", thisIndex+1,
                    //    true).
                    //    d. ReturnIfAbrupt(putStatus).
                    Put(rx, _ejs_atom_lastIndex,
                        NUMBER_TO_EJSVAL(thisIndex + 1), EJS_TRUE);
                }
            }
        }
    }
    // 15. Let accumulatedResult be the empty String value.
    ejsval accumulatedResult = _ejs_atom_empty;

    // 16. Let nextSourcePosition be 0.
    int64_t nextSourcePosition = 0;

    EJS_ASSERT(EJSVAL_IS_DENSE_ARRAY(results));
    // 17. Repeat, for each result in results,
    for (int i = 0, e = EJS_ARRAY_LEN(results); i < e; i++) {
        ejsval result = EJS_DENSE_ARRAY_ELEMENTS(results)[i];

        // a. Let nCaptures be ToLength(Get(result, "length")).
        // b. ReturnIfAbrupt(nCaptures).
        int64_t nCaptures = ToLength(Get(result, _ejs_atom_length));

        // c. Let nCaptures be max(nCaptures − 1, 0).
        nCaptures = MAX(nCaptures - 1, 0);

        // d. Let matched be ToString(Get(result, "0")).
        // e. ReturnIfAbrupt(matched).
        ejsval matched = ToString(Get(result, _ejs_atom_0));

        // f. Let matchLength be the number of code units in matched.
        int matchLength = EJSVAL_TO_STRLEN(matched);

        // g. Let position be ToInteger(Get(result, "index")).
        // h. ReturnIfAbrupt(position).
        int64_t position = ToInteger(Get(result, _ejs_atom_index));

        // i. Let position be max(min(position, lengthS), 0).
        position = MAX(MIN(position, lengthS), 0);

        // j. Let n be 1.
        int n = 1;

        // k. Let captures be an empty List.
        ejsval captures = _ejs_array_new(0, EJS_FALSE);

        // l. Repeat while n <= nCaptures
        while (n <= nCaptures) {
            //    i. Let capN be Get(result, ToString(n)).
            ejsval capN = Get(result, ToString(NUMBER_TO_EJSVAL(n)));
            //    ii. If Type(capN) is not Undefined, then let capN be
            //    ToString(capN).
            if (!EJSVAL_IS_UNDEFINED(capN))
                capN = ToString(capN);
            //    iii. ReturnIfAbrupt(capN).

            //    iv. Append capN as the last element of captures.
            _ejs_array_push_dense(captures, 1, &capN);

            //    v. Let n be n+1
            n = n + 1;
        }

        ejsval replacement;

        // m. If functionalReplace is true, then
        if (functionalReplace) {
            //    i. Let replacerArgs be the List (matched).
            int numReplacerArgs = EJS_ARRAY_LEN(captures) + 3;
            ejsval *replacerArgs =
                (ejsval *)alloca(sizeof(ejsval) * numReplacerArgs);
            *replacerArgs = matched;

            //    ii. Append in list order the elements of captures to the end
            //    of the List replacerArgs.
            memmove(replacerArgs + 1, EJS_DENSE_ARRAY_ELEMENTS(captures),
                    (numReplacerArgs - 3) * sizeof(ejsval));
            //    iii. Append position and S as the last two elements of
            //    replacerArgs.
            replacerArgs[numReplacerArgs - 2] = NUMBER_TO_EJSVAL(position);
            replacerArgs[numReplacerArgs - 1] = S;

            //    iv. Let replValue be Call(replaceValue, undefined,
            //    replacerArgs).
            ejsval replValue = _ejs_invoke_closure(
                replaceValue, _ejs_undefined, numReplacerArgs, replacerArgs);
            //    v. Let replacement be ToString(replValue).
            replacement = ToString(replValue);
        }
        // n. Else,
        else {
            // i. Let replacement be GetReplaceSubstitution(matched, S,
            // position, captures, replaceValue).
            replacement = GetReplaceSubstitution(matched, S, position, captures,
                                                 replaceValue);
        }
        // o. ReturnIfAbrupt(replacement).

        // p. If position >= nextSourcePosition, then
        if (position >= nextSourcePosition) {
            //    i. NOTE position should not normally move backwards. If it
            //    does, it is in indication of a ill-behaving
            //       RegExp subclass or use of an access triggered side-effect
            //       to change the global flag or other
            //       characteristics of rx. In such cases, the corresponding
            //       substitution is ignored.
            //    ii. Let accumulatedResult be the String formed by
            //    concatenating the code units of the current
            //       value of accumulatedResult with the substring of S
            //       consisting of the code units from
            //       nextSourcePosition (inclusive) up to position (exclusive)
            //       and with the code units of
            //       replacement.
            accumulatedResult = _ejs_string_concatv(
                accumulatedResult,
                _ejs_string_new_substring(S, nextSourcePosition,
                                          position - nextSourcePosition),
                replacement, _ejs_undefined);
            //    iii. Let nextSourcePosition be position + matchLength.
            nextSourcePosition = position + matchLength;
        }
    }
    // 18. If nextSourcePosition >= lengthS, then return accumulatedResult.
    if (nextSourcePosition >= lengthS)
        return accumulatedResult;

    // 19. Return the String formed by concatenating the code units of
    // accumulatedResult with the substring
    //     of S consisting of the code units from nextSourcePosition (inclusive)
    //     up through the final code unit
    //     of S (inclusive).
    return _ejs_string_concat(
        accumulatedResult,
        _ejs_string_new_substring(S, nextSourcePosition,
                                  EJSVAL_TO_STRLEN(S) - nextSourcePosition));
}

// ES6 21.2.5.11
// RegExp.prototype [ @@split ] ( string, limit )
static ejsval _ejs_RegExp_prototype_split(ejsval env, ejsval _this,
                                          uint32_t argc, ejsval *args) {
    ejsval string = _ejs_undefined;
    if (argc > 0)
        string = args[0];

    ejsval limit = _ejs_undefined;
    if (argc > 1)
        limit = args[1];

    // 1. Let rx be the this value.
    ejsval rx = _this;

    // 2. If Type(rx) is not Object, throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(rx))
        _ejs_throw_nativeerror_utf8(
            EJS_TYPE_ERROR,
            "Regexp.prototype[Symbol.split] called with non-object 'this'");

    // 3. Let S be ToString(string).
    // 4. ReturnIfAbrupt(S).
    ejsval S = ToString(string);
    jschar *S_chars = EJSVAL_TO_FLAT_STRING(S);

    // 5. Let C be SpeciesConstructor(rx, %RegExp%).
    // 6. ReturnIfAbrupt(C).
    // XXX we don't supporrt the SpeciesConstructor stuff yet.  always construct
    // a RegExp.  see #11 below

    // 7. Let flags be ToString(Get(rx, "flags"))
    // 8. ReturnIfAbrupt(flags).
    ejsval flags = ToString(Get(rx, _ejs_atom_flags));

    // 9. If flags contains "u",then let unicodeMatching be true else let
    // unicodeMatching be false.
    EJSBool unicodeMatching = EJS_FALSE;
    // XXX

    // 10. If flags contains "y", let newFlags be flags else let newFlags be the
    // string that is the concatenation of flags and "y".
    ejsval newFlags = flags;
    // XXX

    // 11. Let splitter be Construct(C, «rx, newFlags»).
    // 12. ReturnIfAbrupt(splitter).
    ejsval splitter = _ejs_regexp_new(rx, newFlags);

    // 13. Let A be ArrayCreate(0).
    ejsval A = _ejs_array_new(0, EJS_FALSE);
    // 14. Let lengthA be 0.
    int lengthA = 0;

    // 15. If limit is undefined, let lim = 2^53–1; else let lim =
    // ToLength(limit).
    // 16. ReturnIfAbrupt(lim).
    int64_t lim =
        EJSVAL_IS_UNDEFINED(limit) ? EJS_MAX_SAFE_INTEGER : ToLength(limit);

    // 17. Let size be the number of elements in S.
    int size = EJSVAL_TO_STRLEN(S);

    // 18. Let p = 0.
    int p = 0;
    // 19. If lim = 0, return A.
    if (lim == 0)
        return A;

    // 20. If size = 0, then
    if (size == 0) {
        // a. Let z be RegExpExec(splitter, S).
        // b. ReturnIfAbrupt(z).
        ejsval z = RegExpExec(splitter, S);

        // c. If z is not null, return A.
        if (!EJSVAL_IS_NULL(z))
            return A;

        // d. Assert: The following call will never result in an abrupt
        // completion.
        // e. Call CreateDataProperty(A, "0", S).
        _ejs_array_push_dense(A, 1, &S);
        // f. Return A.
        return A;
    }
    // 21. Let q = p.
    int q = p;
    // 22. Repeat, while q < size
    while (q < size) {
        // a. Let putStatus be Put(splitter, "lastIndex", q, true).
        // b. ReturnIfAbrupt(putStatus).
        Put(splitter, _ejs_atom_lastIndex, NUMBER_TO_EJSVAL(q), EJS_TRUE);

        // c. Let z be RegExpExec(splitter, S).
        // d. ReturnIfAbrupt(z).
        ejsval z = RegExpExec(splitter, S);

        // e. If z is null, then
        if (EJSVAL_IS_NULL(z)) {
            // i. If unicodeMatching is true, then
            if (unicodeMatching) {
                // 1. Let first be the code unit value of the element at index q
                // in the String S.
                jschar first = S_chars[q];
                // 2. If first ≥ 0xD800 and first ≤ 0xDBFF and q+1 ≠ size, then
                if (first >= 0xD800 && first <= 0xDBFF && q < size - 1) {
                    // a. Let second be the code unit value of the element at
                    // index q+1 in the String S.
                    jschar second = S_chars[q + 1];
                    // b. If second ≥ 0xDC00 and second ≤ 0xDFFF, then
                    if (second >= 0xDC00 && second <= 0xDFFF)
                        // i. Let q = q+1.
                        q++;
                }
            }
            // ii. Let q = q+1.
            q++;
        }
        // f. Else z is not null,
        else {
            // i. Let e be ToLength(Get(splitter, "lastIndex")).
            // ii. ReturnIfAbrupt(e).
            int64_t e = ToLength(Get(splitter, _ejs_atom_lastIndex));
            // iii. If e = p, then
            if (e == p) {
                // 1. If unicodeMatching is true, then
                if (unicodeMatching) {
                    // a. Let first be the code unit value of the element at
                    // index q in the String S.
                    jschar first = S_chars[q];
                    // b. If first ≥ 0xD800 and first ≤ 0xDBFF and q+1 ≠ size,
                    // then
                    if (first >= 0xD800 && first <= 0xDBFF && q < size - 1) {
                        // i. Let second be the code unit value of the element
                        // at index q+1 in the String S.
                        jschar second = S_chars[q + 1];
                        // ii. If second ≥ 0xDC00 and second ≤ 0xDFFF, then
                        if (second >= 0xDC00 && second <= 0xDFFF)
                            // 1. Let q = q+1.
                            q++;
                    }
                }
                // 2. Let q = q+1.
                q++;
            }
            // iv. Else e ≠ p,
            else {
                // 1. Let T be a String value equal to the substring of S
                // consisting of the elements at indices p (inclusive) through q
                // (exclusive).
                ejsval T = _ejs_string_new_substring(S, p, q - p);
                // 2. Assert: The following call will never result in an abrupt
                // completion.
                // 3. Call CreateDataProperty(A, ToString(lengthA), T).
                _ejs_array_push_dense(A, 1, &T);

                // 4. Let lengthA be lengthA +1.
                lengthA++;

                // 5. If lengthA = lim, return A.
                if (lengthA == lim)
                    return A;

                // 6. Let p = e.
                p = e;

                // 7. Let i = 0.
                // 8. Let numberOfCaptures be ToLength(Get(z, "length")).
                // 9. ReturnIfAbrupt(numberOfCaptures).
                int64_t numberOfCaptures = ToLength(Get(z, _ejs_atom_length));
                // 10. Let numberOfCaptures be max(numberOfCaptures-1, 0).
                numberOfCaptures = MAX(numberOfCaptures - 1, 0);
                // 11. Let i be 1.
                int i = 1;
                // 12. Repeat, while i ≤ numberOfCaptures.
                while (i <= numberOfCaptures) {
                    EJS_NOT_IMPLEMENTED();
                    // a. Let nextCapture be Get(z, ToString(i)).
                    // b. ReturnIfAbrupt(nextCapture).
                    // c. Call CreateDataProperty(A, ToString(lengthA),
                    // nextCapture).
                    // d. Let i be i +1.
                    // e. Let lengthA be lengthA +1.
                    // f. If lengthA = lim, return A.
                }
                // 13. Let q = p.
                q = p;
            }
        }
    }
    // 23. Let T be a String value equal to the substring of S consisting of the
    // elements at indices p (inclusive) through size (exclusive).
    ejsval T = _ejs_string_new_substring(S, p, size - p);
    // 24. Assert: The following call will never result in an abrupt completion.
    // 25. Call CreateDataProperty(A, ToString(lengthA), T ).
    _ejs_array_push_dense(A, 1, &T);
    // 26. Return A.
    return A;
}

// ES6 21.2.5.9
// RegExp.prototype [ @@search ] ( string )
static ejsval _ejs_RegExp_prototype_search(ejsval env, ejsval _this,
                                           uint32_t argc, ejsval *args) {
    EJS_NOT_IMPLEMENTED();
}

void _ejs_regexp_init(ejsval global) {
    pcre16_tables = pcre16_maketables();

    _ejs_RegExp = _ejs_function_new_without_proto(
        _ejs_null, _ejs_atom_RegExp, (EJSClosureFunc)_ejs_RegExp_impl);
    _ejs_object_setprop(global, _ejs_atom_RegExp, _ejs_RegExp);

    _ejs_gc_add_root(&_ejs_RegExp_prototype);
    _ejs_RegExp_prototype = _ejs_object_new(_ejs_null, &_ejs_RegExp_specops);
    EJSRegExp *re_proto = (EJSRegExp *)EJSVAL_TO_OBJECT(_ejs_RegExp_prototype);
    re_proto->pattern = _ejs_string_new_utf8("(?:)");
    re_proto->flags = _ejs_atom_empty;

    _ejs_object_setprop(_ejs_RegExp, _ejs_atom_prototype,
                        _ejs_RegExp_prototype);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_RegExp, x, _ejs_RegExp_##x)
#define PROTO_METHOD(x)                                                        \
    EJS_INSTALL_ATOM_FUNCTION(_ejs_RegExp_prototype, x,                        \
                              _ejs_RegExp_prototype_##x)
#define PROTO_METHOD_VAL(x)                                                    \
    EJS_INSTALL_ATOM_FUNCTION_VAL(_ejs_RegExp_prototype, x,                    \
                                  _ejs_RegExp_prototype_##x)
#define PROTO_GETTER(x)                                                        \
    EJS_INSTALL_ATOM_GETTER(_ejs_RegExp_prototype, x,                          \
                            _ejs_RegExp_prototype_get_##x)

    _ejs_gc_add_root(&_ejs_RegExp_prototype_exec_closure);
    _ejs_RegExp_prototype_exec_closure = PROTO_METHOD_VAL(exec);

    PROTO_METHOD(test);
    PROTO_METHOD(toString);

    PROTO_GETTER(global);
    PROTO_GETTER(ignoreCase);
    PROTO_GETTER(lastIndex);
    PROTO_GETTER(multiline);
    PROTO_GETTER(source);
    PROTO_GETTER(sticky);
    PROTO_GETTER(unicode);
    PROTO_GETTER(flags);

#undef OBJ_METHOD
#undef PROTO_METHOD

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS(_ejs_RegExp, create, _ejs_RegExp_create,
                                      EJS_PROP_NOT_ENUMERABLE);
    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS(_ejs_RegExp_prototype, match,
                                      _ejs_RegExp_prototype_match,
                                      EJS_PROP_NOT_ENUMERABLE);
    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS(_ejs_RegExp_prototype, replace,
                                      _ejs_RegExp_prototype_replace,
                                      EJS_PROP_NOT_ENUMERABLE);
    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS(_ejs_RegExp_prototype, split,
                                      _ejs_RegExp_prototype_split,
                                      EJS_PROP_NOT_ENUMERABLE);
    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS(_ejs_RegExp_prototype, search,
                                      _ejs_RegExp_prototype_search,
                                      EJS_PROP_NOT_ENUMERABLE);
    EJS_INSTALL_SYMBOL_GETTER(_ejs_RegExp, species, _ejs_RegExp_get_species);
}

static EJSObject *_ejs_regexp_specop_allocate() {
    return (EJSObject *)_ejs_gc_new(EJSRegExp);
}

static void _ejs_regexp_specop_scan(EJSObject *obj, EJSValueFunc scan_func) {
    EJSRegExp *re = (EJSRegExp *)obj;
    scan_func(re->pattern);
    scan_func(re->flags);

    _ejs_Object_specops.Scan(obj, scan_func);
}

EJS_DEFINE_CLASS(RegExp,
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
                 _ejs_regexp_specop_allocate,
                 OP_INHERIT, // [[Finalize]]
                 _ejs_regexp_specop_scan)
