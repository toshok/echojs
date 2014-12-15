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

#include "pcre.h"

static ejsval _ejs_RegExp_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args);

static const unsigned char* pcre16_tables;

ejsval
_ejs_regexp_new (ejsval pattern, ejsval flags)
{
    EJSRegExp* rv = _ejs_gc_new(EJSRegExp);

    _ejs_init_object ((EJSObject*)rv, _ejs_RegExp_prototype, &_ejs_RegExp_specops);

    ejsval args[2] = { pattern, flags };

    return _ejs_RegExp_impl (_ejs_null, OBJECT_TO_EJSVAL(rv), 2, args);
}

ejsval
_ejs_regexp_new_utf8 (const char *pattern, const char *flags)
{
    return _ejs_regexp_new (_ejs_string_new_utf8 (pattern),
                            _ejs_string_new_utf8 (flags));
}

ejsval
_ejs_regexp_replace(ejsval str, ejsval search_re, ejsval replace)
{
    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(search_re);

    pcre16_extra extra;
    memset (&extra, 0, sizeof(extra));

    pcre16* code = (pcre16*)re->compiled_pattern;

    int capture_count;
    pcre16_fullinfo (code, NULL, PCRE_INFO_CAPTURECOUNT, &capture_count);

    int ovec_count = 3 * (1 + capture_count);
    int* ovec = malloc(sizeof(int) * ovec_count);
    int cur_off = 0;

    do {
        EJSPrimString *flat_str = _ejs_string_flatten (str);
        jschar *chars_str = flat_str->data.flat;

        int rv = pcre16_exec(code, &extra,
                             chars_str, flat_str->length, cur_off,
                             PCRE_NO_UTF16_CHECK, ovec, ovec_count);

        if (rv < 0)
            break;

        ejsval replaceval;

        if (EJSVAL_IS_FUNCTION(replace)) {
            ejsval substr_match = _ejs_string_new_substring (str, ovec[0], ovec[1] - ovec[0]);
            ejsval capture = _ejs_string_new_substring (str, ovec[2], ovec[3] - ovec[2]);

            _ejs_log ("substring match is %s\n", ucs2_to_utf8(_ejs_string_flatten(substr_match)->data.flat));
            _ejs_log ("capture is %s\n", ucs2_to_utf8(_ejs_string_flatten(capture)->data.flat));

            int argc = 3;
            ejsval args[3];

            args[0] = substr_match;
            args[1] = capture;
            args[2] = _ejs_undefined;

            replaceval = ToString(_ejs_invoke_closure (replace, _ejs_undefined, argc, args));
        }
        else {
            replaceval = ToString(replace);
        }

        if (ovec[0] == 0) {
            // we matched from the beginning of the string, so nothing from there to prepend
            str = _ejs_string_concat (replaceval, _ejs_string_new_substring (str, ovec[1], flat_str->length - ovec[1]));
        }
        else {
            str = _ejs_string_concatv (_ejs_string_new_substring (str, 0, ovec[0]),
                                       replaceval,
                                       _ejs_string_new_substring (str, ovec[1], flat_str->length - ovec[1]),
                                       _ejs_null);
        }

        cur_off = ovec[1];

        // if the RegExp object was created without a 'g' flag, only replace the first match
        if (!re->global)
            break;
    } while (EJS_TRUE);

    free (ovec);
    return str;
}


ejsval _ejs_RegExp EJSVAL_ALIGNMENT;
ejsval _ejs_RegExp_prototype EJSVAL_ALIGNMENT;

static ejsval
_ejs_RegExp_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSRegExp *re;

    if (EJSVAL_IS_UNDEFINED(_this)) {
        // called as a function
        _this = _ejs_object_new(_ejs_RegExp_prototype, &_ejs_RegExp_specops);
    }

    re = (EJSRegExp*)EJSVAL_TO_OBJECT(_this);

    re->pattern = _ejs_undefined;
    re->flags = _ejs_undefined;

    if (argc > 0) re->pattern = args[0];
    if (argc > 1) re->flags = args[1];

    if (!EJSVAL_IS_STRING(re->pattern))
        EJS_NOT_IMPLEMENTED();

    EJSPrimString *flat_pattern = _ejs_string_flatten (re->pattern);
    jschar* chars = flat_pattern->data.flat;

    const char *pcre_error;
    int pcre_erroffset;

    re->compiled_pattern = pcre16_compile(chars,
                                          PCRE_UTF16 | PCRE_NO_UTF16_CHECK,
                                          &pcre_error, &pcre_erroffset,
                                          pcre16_tables);

    _ejs_object_define_value_property (_this, _ejs_atom_source, re->pattern, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_CONFIGURABLE | EJS_PROP_NOT_WRITABLE);

    if (EJSVAL_IS_STRING(re->flags)) {
        EJSPrimString *flat_flags = _ejs_string_flatten(re->flags);
        chars = flat_flags->data.flat;

        for (int i = 0; i < flat_flags->length; i ++) {
            if      (chars[i] == 'g' && !re->global)     { re->global     = EJS_TRUE; continue; }
            else if (chars[i] == 'i' && !re->ignoreCase) { re->ignoreCase = EJS_TRUE; continue; }
            else if (chars[i] == 'm' && !re->multiline)  { re->multiline  = EJS_TRUE; continue; }
            else if (chars[i] == 'y' && !re->sticky)     { re->sticky     = EJS_TRUE; continue; }
            else if (chars[i] == 'u' && !re->unicode)    { re->unicode    = EJS_TRUE; continue; }
            _ejs_throw_nativeerror_utf8 (EJS_SYNTAX_ERROR, "Invalid flag supplied to RegExp constructor");
        }
    }

    return _this;
}

static ejsval
RegExpBuiltinExec(ejsval R, ejsval S)
{
    EJS_ASSERT(EJSVAL_IS_REGEXP(R));
    EJS_ASSERT(EJSVAL_IS_STRING_TYPE(S));

    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(R);

    ejsval subject = S;

    pcre16_extra extra;
    memset (&extra, 0, sizeof(extra));

    EJSPrimString *flat_subject = _ejs_string_flatten (subject);
    jschar* subject_chars = flat_subject->data.flat;

    int ovec[3];

    int rv = pcre16_exec((pcre16*)re->compiled_pattern, &extra,
                         subject_chars, flat_subject->length, 0,
                         PCRE_NO_UTF16_CHECK, ovec, 3);

    if (rv == PCRE_ERROR_NOMATCH)
        return _ejs_null;

    // XXX we're supposed to return an object here...
    EJS_NOT_IMPLEMENTED();
}

static ejsval
RegExpExec(ejsval R, ejsval S)
{
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

        // c. If Type(result) is neither Object or Null, then throw a TypeError exception.
        if (!EJSVAL_IS_OBJECT(result) && !EJSVAL_IS_NULL(result))
            _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'exec' returned non-object, non-null value");

        // d. Return(result).
        return result;
    }

    // 6. If R does not have a [[RegExpMatcher]] internal slot, then throw a TypeError exception.
    // 7. If the value of R’s [[RegExpMatcher]] internal slot is undefined, then throw a TypeError exception.
    if (!EJSVAL_IS_REGEXP(R))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' is not a RegExp");

    // 8. Return RegExpBuiltinExec(R, S).
    return RegExpBuiltinExec(R, S);
}

// ES6 21.2.5.2
// RegExp.prototype.exec ( string )
ejsval
_ejs_RegExp_prototype_exec (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval string = _ejs_undefined;
    if (argc > 0) string = args[0];

    // 1. Let R be the this value.
    ejsval R = _this;

    // 2. If Type(R) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(R))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "non-object 'this' in RegExp.prototype.exec");

    // 3. If R does not have a [[RegExpMatcher]] internal slot, then throw a TypeError exception.
    // 4. If the value of R’s [[RegExpMatcher]] internal slot is undefined, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(R))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "non-RegExp 'this' in RegExp.prototype.exec");

    // 5. Let S be ToString(string)
    // 6. ReturnIfAbrupt(S).
    ejsval S = ToString(string);

    // 7. Return RegExpBuiltinExec(R, S).
    return RegExpBuiltinExec(R, S);
}

// ECMA262: 15.10.6.3
static ejsval
_ejs_RegExp_prototype_test (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    if (!EJSVAL_IS_REGEXP(_this))
        EJS_NOT_IMPLEMENTED();

    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(_this);

    ejsval subject = _ejs_undefined;
    if (argc > 0) subject = args[0];

    pcre16_extra extra;
    memset (&extra, 0, sizeof(extra));

    EJSPrimString *flat_subject = _ejs_string_flatten (subject);
    jschar* subject_chars = flat_subject->data.flat;

    int ovec[3];

    int rv = pcre16_exec((pcre16*)re->compiled_pattern, &extra,
                         subject_chars, flat_subject->length, 0,
                         PCRE_NO_UTF16_CHECK, ovec, 3);

    return rv == PCRE_ERROR_NOMATCH ? _ejs_false : _ejs_true;
}

static ejsval
_ejs_RegExp_prototype_toString (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(_this);

    return _ejs_string_concatv (_ejs_atom_slash, re->pattern, _ejs_atom_slash, re->flags, _ejs_null);
}

static ejsval
_ejs_RegExp_prototype_get_global (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(_this);
    return BOOLEAN_TO_EJSVAL(re->global);
}

static ejsval
_ejs_RegExp_prototype_get_ignoreCase (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(_this);
    return BOOLEAN_TO_EJSVAL(re->ignoreCase);
}

static ejsval
_ejs_RegExp_prototype_get_lastIndex (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(_this);
    return NUMBER_TO_EJSVAL(re->lastIndex);
}

static ejsval
_ejs_RegExp_prototype_get_multiline (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(_this);
    return BOOLEAN_TO_EJSVAL(re->multiline);
}

static ejsval
_ejs_RegExp_prototype_get_sticky (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(_this);
    return BOOLEAN_TO_EJSVAL(re->sticky);
}

static ejsval
_ejs_RegExp_prototype_get_unicode (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(_this);
    return BOOLEAN_TO_EJSVAL(re->unicode);
}

static ejsval
_ejs_RegExp_prototype_get_source (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(_this);
    return re->pattern;
}

// ES6 21.2.5.3
// get RegExp.prototype.flags ( )
static ejsval
_ejs_RegExp_prototype_get_flags (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let R be the this value.
    ejsval R = _this;

    // 2. If Type(R) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(R))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "get Regexp.prototype.flags called with non-object 'this'");
    
    // 3. Let result be the empty String.
    char result_buf[6];
    memset (result_buf, 0, sizeof(result_buf));
    char* p = result_buf;


    // 4. Let global be ToBoolean(Get(R, "global")).
    // 5. ReturnIfAbrupt(global).
    EJSBool global = ToEJSBool(Get(R, _ejs_atom_global));


    // 6. If global is true, then append "g" as the last code unit of result.
    if (global) *p++ = 'g';

    // 7. Let ignoreCase be ToBoolean(Get(R, "ignoreCase")).
    // 8. ReturnIfAbrupt(ignoreCase).
    EJSBool ignoreCase = ToEJSBool(Get(R, _ejs_atom_ignoreCase));

    // 9. If ignoreCase is true, then append "i" as the last code unit of result.
    if (ignoreCase) *p++ = 'i';

    // 10. Let multiline be ToBoolean(Get(R, "multiline")).
    // 11. ReturnIfAbrupt(multiline).
    EJSBool multiline = ToEJSBool(Get(R, _ejs_atom_multiline));

    // 12. If multiline is true, then append "m" as the last code unit of result.
    if (multiline) *p++ = 'm';

    // 13. Let sticky be ToBoolean(Get(R, "sticky")).
    // 14. ReturnIfAbrupt(sticky).
    EJSBool sticky = ToEJSBool(Get(R, _ejs_atom_sticky));

    // 15. If sticky is true, then append "y" as the last code unit of result.
    if (sticky) *p++ = 'y';

    // 16. Let unicode be ToBoolean(Get(R, "unicode")).
    // 17. ReturnIfAbrupt(unicode).
    EJSBool unicode = ToEJSBool(Get(R, _ejs_atom_unicode));

    // 18. If unicode is true, then append "u" as the last code unit of result.
    if (unicode) *p++ = 'u';

    // 19. Return result.
    return _ejs_string_new_utf8(result_buf);
}

static ejsval
_ejs_RegExp_get_species (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    return _ejs_RegExp;
}

static ejsval
_ejs_RegExp_create (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    // 1. Let F be the this value. 
    ejsval F = _this;

    if (!EJSVAL_IS_CONSTRUCTOR(F)) 
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "'this' in RegExp[Symbol.create] is not a constructor");

    EJSObject* F_ = EJSVAL_TO_OBJECT(F);

    // 2. Let obj be the result of calling OrdinaryCreateFromConstructor(constructor, "%RegExpPrototype%", ( [[RegExpMatcher]], [[OriginalSource]], [[OriginalFlags]])). 
    ejsval proto = OP(F_,Get)(F, _ejs_atom_prototype, F);
    if (EJSVAL_IS_UNDEFINED(proto))
        proto = _ejs_RegExp_prototype;

    EJSRegExp* re = (EJSRegExp*)_ejs_gc_new (EJSRegExp);
    _ejs_init_object ((EJSObject*)re, proto, &_ejs_RegExp_specops);
    
    re->pattern = _ejs_undefined;
    re->flags = _ejs_undefined;

    return OBJECT_TO_EJSVAL((EJSObject*)re);
}

// ES6 21.2.5.6
// RegExp.prototype [ @@match ] ( string )
static ejsval
_ejs_RegExp_prototype_match (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval string = _ejs_undefined;
    if (argc > 0) string = args[0];

    // 1. Let rx be the this value.
    ejsval rx = _this;

    // 2. If Type(rx) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(rx))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Regexp[Symbol.match] called with non-object 'this'");
        
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
                    // a. Let putStatus be Put(rx, "lastIndex", thisIndex+1, true).
                    // b. ReturnIfAbrupt(putStatus).
                    Put(rx, _ejs_atom_lastIndex, NUMBER_TO_EJSVAL(thisIndex+1), EJS_TRUE);
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

                // 6. Let status be CreateDataProperty(A, ToString(n), matchStr).
                // 7. Assert: status is true.
                _ejs_object_define_value_property (A, ToString(NUMBER_TO_EJSVAL(n)), matchStr, EJS_PROP_FLAGS_ENUMERABLE | EJS_PROP_FLAGS_CONFIGURABLE | EJS_PROP_FLAGS_WRITABLE);
                // 8. Increment n.
                n++;
            }
        }
    }
}

// ES6 21.2.5.8
//  RegExp.prototype [ @@replace ] ( string, replaceValue )
static ejsval
_ejs_RegExp_prototype_replace (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval string = _ejs_undefined;
    if (argc > 0) string = args[0];

    ejsval replaceValue = _ejs_undefined;
    if (argc > 1) replaceValue = args[1];

    // 1. Let rx be the this value.
    ejsval rx = _this;

    // 2. If Type(rx) is not Object, then throw a TypeError exception.
    if (!EJSVAL_IS_OBJECT(rx))
        _ejs_throw_nativeerror_utf8 (EJS_TYPE_ERROR, "Regexp.prototype[Symbol.replace] called with non-object 'this'");

    // 3. Let S be ToString(string).
    // 4. ReturnIfAbrupt(S).
    ejsval S = ToString(string);

    EJS_NOT_IMPLEMENTED();

    // 5. Let lengthS be the number of code unit elements in S.
    // 6. Let functionalReplace be IsCallable(replaceValue).
    // 7. If functionReplace is false, then
    //    a. Let replaceValue be ToString(replaceValue).
    //    b. ReturnIfAbrupt(replaceValue).
    // 8. Let global be ToBoolean(Get(rx, "global")).
    // 9. ReturnIfAbrupt(global).
    // 10. If global is true, then
    //     a. Let putStatus be Put(rx, "lastIndex", 0, true).
    //     b. ReturnIfAbrupt(putStatus).
    // 11. Let previousLastIndex be 0.
    // 12. Let results be a new empty List.
    // 13. Let done be false.
    // 14. Repeat, while done is false
    //     a. Let result be RegExpExec(rx, S).
    //     b. ReturnIfAbrupt(result).
    //     c. If result is null, then set done to true.
    //     d. Else result is not null,
    //        i. If global is false, then set done to true.
    //        ii. Else,
    //            1. Let thisIndex be ToInteger(Get(rx, "lastIndex")).
    //            2. ReturnIfAbrupt(thisIndex).
    //            3. If thisIndex = previousLastIndex then
    //               a. Let putStatus be Put(rx, "lastIndex", thisIndex+1, true).
    //               b. ReturnIfAbrupt(putStatus).
    //               c. Set previousLastIndex to thisIndex+1.
    //            4. Else,
    //               a. Set previousLastIndex to thisIndex.
    //     e. If result is not null, then append result to the end of results.
    // 15. Let accumulatedResult be the empty String value.
    // 16. Let nextSourcePosition be 0.
    // 17. Repeat, for each result in results,
    //     a. Let nCaptures be ToLength(Get(result, "length")).
    //     b. ReturnIfAbrupt(nCaptures).
    //     c. Let nCaptures be max(nCaptures − 1, 0).
    //     d. Let matched be ToString(Get(result, "0")).
    //     e. ReturnIfAbrupt(matched).
    //     f. Let matchLength be the number of code units in matched.
    //     g. Let position be ToInteger(Get(result, "index")).
    //     h. ReturnIfAbrupt(position).
    //     i. Let position be max(min(position, lengthS), 0).
    //     j. Let n be 1.
    //     k. Let captures be an empty List.
    //     l. Repeat while n ≤ nCaptures
    //        i. Let capN be Get(result, ToString(n)).
    //        ii. If Type(capN) is not Undefined, then let capN be ToString(capN).
    //        iii. ReturnIfAbrupt(capN).
    //        iv. Append capN as the last element of captures.
    //        v. Let n be n+1
    //     m. If functionalReplace is true, then
    //        i. Let replacerArgs be the List (matched).
    //        ii. Append in list order the elements of captures to the end of the List replacerArgs.
    //        iii. Append position and S as the last two elements of replacerArgs.
    //        iv. Let replValue be Call(replaceValue, undefined, replacerArgs).
    //        v. Let replacement be ToString(replValue).
    //     n. Else,
    //        i. Let replacement be GetReplaceSubstitution(matched, S, position, captures, replaceValue).
    //     o. ReturnIfAbrupt(replacement).
    //     p. If position ≥ nextSourcePosition, then
    //        i. NOTE position should not normally move backwards. If it does, it is in indication of a ill-behaving
    //           RegExp subclass or use of an access triggered side-effect to change the global flag or other
    //           characteristics of rx. In such cases, the corresponding substitution is ignored.
    //        ii. Let accumulatedResult be the String formed by concatenating the code units of the current
    //           value of accumulatedResult with the substring of S consisting of the code units from
    //           nextSourcePosition (inclusive) up to position (exclusive) and with the code units of
    //           replacement.
    //        iii. Let nextSourcePosition be position + matchLength.
    // 18. If nextSourcePosition ≥ lengthS, then return accumulatedResult.
    // 19. Return the String formed by concatenating the code units of accumulatedResult with the substring
    //     of S consisting of the code units from nextSourcePosition (inclusive) up through the final code unit
    //     of S (inclusive). 
}

void
_ejs_regexp_init(ejsval global)
{
    pcre16_tables = pcre16_maketables();

    _ejs_RegExp = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_RegExp, (EJSClosureFunc)_ejs_RegExp_impl);
    _ejs_object_setprop (global, _ejs_atom_RegExp, _ejs_RegExp);

    _ejs_gc_add_root (&_ejs_RegExp_prototype);
    _ejs_RegExp_prototype = _ejs_object_new(_ejs_null, &_ejs_RegExp_specops);
    EJSRegExp* re_proto = (EJSRegExp*)EJSVAL_TO_OBJECT(_ejs_RegExp_prototype);
    re_proto->pattern = _ejs_string_new_utf8("(?:)");
    re_proto->flags = _ejs_atom_empty;

    _ejs_object_setprop (_ejs_RegExp,       _ejs_atom_prototype,  _ejs_RegExp_prototype);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_RegExp, x, _ejs_RegExp_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_RegExp_prototype, x, _ejs_RegExp_prototype_##x)
#define PROTO_METHOD_VAL(x) EJS_INSTALL_ATOM_FUNCTION_VAL(_ejs_RegExp_prototype, x, _ejs_RegExp_prototype_##x)
#define PROTO_GETTER(x) EJS_INSTALL_ATOM_GETTER(_ejs_RegExp_prototype, x, _ejs_RegExp_prototype_get_##x)

    _ejs_gc_add_root (&_ejs_RegExp_prototype_exec_closure);
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

    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_RegExp, create, _ejs_RegExp_create, EJS_PROP_NOT_ENUMERABLE);
    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_RegExp_prototype, match, _ejs_RegExp_prototype_match, EJS_PROP_NOT_ENUMERABLE);
    EJS_INSTALL_SYMBOL_FUNCTION_FLAGS (_ejs_RegExp_prototype, replace, _ejs_RegExp_prototype_replace, EJS_PROP_NOT_ENUMERABLE);
    EJS_INSTALL_SYMBOL_GETTER(_ejs_RegExp, species, _ejs_RegExp_get_species);
}

static EJSObject*
_ejs_regexp_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new(EJSRegExp);
}

static void
_ejs_regexp_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSRegExp *re = (EJSRegExp*)obj;
    scan_func (re->pattern);
    scan_func (re->flags);

    _ejs_Object_specops.Scan (obj, scan_func);
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
                 _ejs_regexp_specop_scan
                 )

