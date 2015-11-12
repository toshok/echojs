/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <string.h>
#include <math.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-function.h"
#include "ejs-proxy.h"
#include "ejs-json.h"
#include "ejs-array.h"
#include "ejs-number.h"
#include "ejs-string.h"
#include "ejs-boolean.h"
#include "ejs-symbol.h"
#include "../parson/parson.h"

ejsval _ejs_JSON EJSVAL_ALIGNMENT;

static EJSBool
json_value_to_ejsval(JSON_Value *v, ejsval *rv)
{
    switch (json_value_get_type (v)) {
    case JSONNull:
        *rv = _ejs_null;
        return EJS_TRUE;

    case JSONString:
        *rv = _ejs_string_new_utf8 (json_value_get_string(v));
        return EJS_TRUE;

    case JSONNumber:
        *rv = NUMBER_TO_EJSVAL(json_value_get_number(v));
        return EJS_TRUE;

    case JSONObject: {
        JSON_Object *obj = json_value_get_object (v);
        *rv = _ejs_object_new(_ejs_null, &_ejs_Object_specops);

        int count = json_object_get_count (obj);
        for (int i = 0; i < count; i ++) {
            const char *propkey = json_object_get_name (obj, i);
            ejsval propval;
            if (!json_value_to_ejsval (json_object_get_value (obj, propkey), &propval))
                return EJS_FALSE;
            _ejs_object_setprop_utf8 (*rv, propkey, propval);
        }

        return EJS_TRUE;
    }
        
    case JSONArray: {
        JSON_Array *arr = json_value_get_array (v);
        int count = json_array_get_count (arr);

        *rv = _ejs_array_new (count, EJS_FALSE);

        for (int i = 0; i < count; i ++) {
            ejsval propkey = _ejs_number_new (i);
            ejsval propval;
            if (!json_value_to_ejsval (json_array_get_value (arr, i), &propval))
                return EJS_FALSE;
            _ejs_object_setprop (*rv, propkey, propval);
        }

        return EJS_TRUE;
    }
    case JSONBoolean:
        *rv = BOOLEAN_TO_EJSVAL(json_value_get_boolean(v));
        return EJS_TRUE;


    case JSONError:
        EJS_NOT_IMPLEMENTED();
        return EJS_FALSE;
    }
}

/* 15.12.2 */
static EJS_NATIVE_FUNC(_ejs_JSON_parse) {
    ejsval text = _ejs_undefined;
    ejsval reviver = _ejs_undefined;

    if (argc > 0) text = args[0];
    if (argc > 1) reviver = args[1];


    /* 1. Let JText be ToString(text). */
    ejsval jtext = ToString(text);

    /* 2. Parse JText using the grammars in 15.12.1. Throw a SyntaxError exception if JText did not conform to the 
       JSON grammar for the goal symbol JSONText.  */
    char *flattened_jtext =  ucs2_to_utf8(EJSVAL_TO_FLAT_STRING(jtext));

    /* 3. Let unfiltered be the result of parsing and evaluating JText as if it was the source text of an ECMAScript 
       Program but using JSONString in place of StringLiteral. Note that since JText conforms to the JSON 
       grammar this result will be either a primitive value or an object that is defined by either an ArrayLiteral or 
       an ObjectLiteral. */
    JSON_Value* root_val = json_parse_string(flattened_jtext);

    free(flattened_jtext);

    if (root_val == NULL) {
        printf ("SyntaxError\n");
        EJS_NOT_IMPLEMENTED();
    }

    ejsval unfiltered;
    if (!json_value_to_ejsval(root_val, &unfiltered)) {
        json_value_free (root_val);
        /* unfiltered here is an exception */
        EJS_NOT_IMPLEMENTED();
    }

    json_value_free (root_val);

    /* 4. If IsCallable(reviver) is true, then */
    if (IsCallable(reviver)) {
        printf ("no reviver support in JSON.parse yet\n");
        EJS_NOT_IMPLEMENTED();
        /*    a. Let root be a new object created as if by the expression new Object(), where Object is the
              standard built-in constructor with that name. */
        /*    b. Call the [[DefineOwnProperty]] internal method of root with the empty String, the 
              PropertyDescriptor {[[Value]]: unfiltered, [[Writable]]: true, [[Enumerable]]: true, 
              [[Configurable]]: true}, and false as arguments. */
        /*    c. Return the result of calling the abstract operation Walk, passing root and the empty String. The 
              abstract operation Walk is described below. */
    }
    /* 5. Else */
    else {
        /*    a. Return unfiltered. */
        return unfiltered;
    }
}

// all state for JSON.stringify that isn't pass as parameters.  stack allocated at the beginning of
// _stringify;
typedef struct {
    ejsval stack;
    ejsval indent;
    ejsval gap;
    ejsval ReplacerFunction;
    ejsval PropertyList;
} StringifyState;

static ejsval SerializeJSONArray (StringifyState* state, ejsval value);
static ejsval SerializeJSONObject (StringifyState* state, ejsval value);
static ejsval SerializeJSONProperty (StringifyState* state, ejsval key, ejsval holder);
static ejsval QuoteJSONString (StringifyState* state, ejsval value);

// ES2015, June 2015
// 24.3.2.2 abstract operation QuoteJSONString ( value )
static ejsval
QuoteJSONString(StringifyState* state, ejsval value) {
    int len = EJSVAL_TO_STRLEN(value);
    jschar *product = malloc((len * 4 + 2) * sizeof(jschar));
    EJSPrimString* prim_value = _ejs_primstring_flatten(EJSVAL_TO_STRING(value)); // this is pretty terrible.. we need a way to simply iterate over characters in a string

    int pi = 0;
    
    // 1. Let product be code unit 0x0022 (QUOTATION MARK).
    product[pi++] = '"';

    // 2. For each code unit C in value
    for (int vi = 0; vi < len; vi++) {
        jschar C = prim_value->data.flat[vi];
        // a. If C is 0x0022 (QUOTATION MARK) or 0x005C (REVERSE SOLIDUS), then
        if (C == '\"' || C == '\\') {
            // i. Let product be the concatenation of product and code unit 0x005C (REVERSE SOLIDUS).
            product[pi++] = '\\';
            // ii. Let product be the concatenation of product and C.
            product[pi++] = C;
        }
        // b. Else if C is 0x0008 (BACKSPACE), 0x000C (FORM FEED), 0x000A (LINE FEED), 0x000D(CARRIAGE RETURN), or 0x000B (LINE TABULATION), then
        else if (C == '\b' ||
                 C == '\f' ||
                 C == '\n' ||
                 C == '\r' ||
                 C == '\t') {
            // i. Let product be the concatenation of product and code unit 0x005C (REVERSE SOLIDUS).
            product[pi++] = '\\';

            // ii. Let abbrev be the String value corresponding to the value of C as follows:
            jschar abbrev;
            // BACKSPACE "b"
            if (C == '\b') abbrev = 'b';
            // FORM FEED (FF) "f"
            if (C == '\f') abbrev = 'f';
            // LINE FEED (LF) "n"
            if (C == '\n') abbrev = 'n';
            // CARRIAGE RETURN (CR) "r"
            if (C == '\r') abbrev = 'r';
            // LINE TABULATION "t"
            if (C == '\t') abbrev = 't';
            // iii. Let product be the concatenation of product and abbrev.
            product[pi++] = abbrev;
        }
        // c. Else if C has a code unit value less than 0x0020 (SPACE), then
        else if (C < ' ') {
            // i. Let product be the concatenation of product and code unit 0x005C (REVERSE SOLIDUS).
            product[pi++] = '\\';

            // ii. Let product be the concatenation of product and "u".
            product[pi++] = 'u';

            // iii. Let hex be the string result of converting the numeric code unit value of C to a String of four hexadecimal digits. Alphabetic hexadecimal digits are presented as lowercase Latin letters.
            static char* hexdigits = "012356789abcdef";

            // iv. Let product be the concatenation of product and hex.
            product[pi++] = '0';
            product[pi++] = '0';
            product[pi++] = hexdigits[(C & 0xf0) >> 4];
            product[pi++] = hexdigits[C & 0xf];
        }
        // d. Else,
        else {
            // i. Let product be the concatenation of product and C.
            product[pi++] = C;
        }
    }
    // 3. Let product be the concatenation of product and code unit 0x0022 (QUOTATION MARK).
    product[pi++] = '\"';
    /* 4. Return product. */
    ejsval rv = _ejs_string_new_ucs2_len(product, pi);
    free (product);
    return rv;
}

// ES2015, June 2015
// 24.3.2.3 abstract operation SerializeJSONObject ( value )
static ejsval
SerializeJSONObject (StringifyState* state, ejsval value) {
    // 1. If stack contains value, throw a TypeError exception because the structure is cyclical.

    // XXX

    // 2. Append value to stack.
    _ejs_array_push_dense (state->stack, 1, &value);

    // 3. Let stepback be indent.
    ejsval stepback = state->indent;

    // 4. Let indent be the concatenation of indent and gap.
    state->indent = _ejs_string_concat (state->indent, state->gap);

    // XXX(toshok): we need this volatile because in the else case
    // below(6), K is allocated, but then we never use the actual
    // value of K again - we only access it via interior pointers.  so
    // if the allocation of partial (7) happens to cause a GC (which
    // happens during stage2 build), we free K and the loop at (8)
    // crashes.  we need to switch to C++ to have some sort of stack
    // rooting smart pointer-esque class.  There are probably lots of
    // dragons of this sort around the code.
    volatile ejsval K;

    // 5. If PropertyList is not undefined, then
    if (!EJSVAL_IS_UNDEFINED(state->PropertyList)) {
        // a. Let K be PropertyList.
        K = state->PropertyList;
    }
    // 6. Else,
    else {
        // a. Let K be EnumerableOwnNames(value).
        K = EnumerableOwnNames(value);
    }

    // 7. Let partial be an empty List.
    ejsval partial = _ejs_array_new (0, EJS_FALSE);

    // 8. For each element P of K,
    for (int kk = 0; kk < EJS_ARRAY_LEN(K); kk++) {
        ejsval P = EJS_DENSE_ARRAY_ELEMENTS(K)[kk];
        // a. Let strP be SerializeJSONProperty(P, value).
        // b. ReturnIfAbrupt(strP).
        ejsval strP = SerializeJSONProperty(state, P, value);

        // c. If strP is not undefined, then
        if (!EJSVAL_IS_UNDEFINED(strP)) {
            // i. Let member be QuoteJSONString(P).
            ejsval member = QuoteJSONString (state, P);
            // ii. Let member be the concatenation of member and the string ":".
            member = _ejs_string_concat (member, _ejs_string_new_utf8(":"));
            // iii. If gap is not the empty String, then
            if (!EJSVAL_EQ(state->gap, _ejs_atom_empty)) {
                // 1. Let member be the concatenation of member and code unit 0x0020 (SPACE).
                member = _ejs_string_concat (member, _ejs_string_new_utf8(" "));
            }
            // iv. Let member be the concatenation of member and strP.
            member = _ejs_string_concat (member, strP);
            // v. Append member to partial.
            _ejs_array_push_dense(partial, 1, &member);
        }
    }

    ejsval final;

    // 9. If partial is empty, then
    if (EJS_ARRAY_LEN(partial) == 0) {
        // a. Let final be "{}".
        final = _ejs_atom_empty_object;
    }
    // 10. Else,
    else {
        // a. If gap is the empty String, then
        if (EJSVAL_EQ(state->gap, _ejs_atom_empty)) {
            // i. Let properties be a String formed by concatenating all the element Strings of partial with each
            // adjacent pair of Strings separated with code unit 0x002C (COMMA). A comma is not inserted
            // either before the first String or after the last String.
            ejsval properties = EJS_DENSE_ARRAY_ELEMENTS(partial)[0];
            ejsval comma_str = _ejs_string_new_utf8(",");
            for (int i = 1; i < EJS_ARRAY_LEN(partial); i ++) {
                properties = _ejs_string_concatv (properties, comma_str, EJS_DENSE_ARRAY_ELEMENTS(partial)[i], _ejs_null);
            }

            // ii. Let final be the result of concatenating "{", properties, and "}".
            final = _ejs_string_concatv (_ejs_string_new_utf8("{"),
                                         properties,
                                         _ejs_string_new_utf8("}"),
                                         _ejs_null);
        }
        // b. Else gap is not the empty String
        else {
            // i. Let separator be the result of concatenating code unit 0x002C (COMMA), code unit 0x000A (LINE FEED), and indent.
            ejsval separator = _ejs_string_concat (_ejs_string_new_utf8(",\n"), state->indent);
            // ii. Let properties be a String formed by concatenating all the element Strings of partial with each
            // adjacent pair of Strings separated with separator. The separator String is not inserted either
            // before the first String or after the last String.
            ejsval properties = EJS_DENSE_ARRAY_ELEMENTS(partial)[0];
            for (int i = 1; i < EJS_ARRAY_LEN(partial); i ++) {
                properties = _ejs_string_concatv (properties, separator, EJS_DENSE_ARRAY_ELEMENTS(partial)[i], _ejs_null);
            }
            // iii. Let final be the result of concatenating "{", code unit 0x000A (LINE FEED), indent, properties, code unit 0x000A, stepback, and "}".
            final = _ejs_string_concatv (_ejs_string_new_utf8("{\n"),
                                         state->indent,
                                         properties,
                                         _ejs_string_new_utf8("\n"),
                                         stepback,
                                         _ejs_string_new_utf8("}"),
                                         _ejs_null);
        }
    }
    // 11. Remove the last element of stack.
    _ejs_array_pop_dense(state->stack);

    // 12. Let indent be stepback.
    state->indent = stepback;

    // 13. Return final.
    return final;
}

// ES2015, June 2015
// 24.3.2.4 abstract operation SerializeJSONArray ( value )
static ejsval
SerializeJSONArray(StringifyState* state, ejsval value) {
    // 1. If stack contains value, throw a TypeError exception because the structure is cyclical.
    // XXX

    // 2. Append value to stack.
    _ejs_array_push_dense(state->stack, 1, &value);

    // 3. Let stepback be indent.
    ejsval stepback = state->indent;

    // 4. Let indent be the concatenation of indent and gap.
    state->indent = _ejs_string_concat (state->indent, state->gap);
    
    // 5. Let partial be an empty List.
    ejsval partial = _ejs_array_new (0, EJS_FALSE);

    // 6. Let len be ToLength(Get(value, "length")).
    // 7. ReturnIfAbrupt(len).
    int len = ToLength(Get(value, _ejs_atom_length));

    // 8. Let index be 0.
    int index = 0;

    // 9. Repeat while index < len
    while (index < len) {
        // a. Let strP be SerializeJSONProperty(ToString(index), value).
        // b. ReturnIfAbrupt(strP).
        ejsval strP = SerializeJSONProperty(state, ToString(NUMBER_TO_EJSVAL(index)), value);

        // c. If strP is undefined, then
        if (EJSVAL_IS_UNDEFINED(strP)) {
            // i. Append "null" to partial.
            _ejs_array_push_dense (partial, 1, &_ejs_atom_null);
        }
        // d. Else,
        else {
            // i. Append strP to partial.
            _ejs_array_push_dense (partial, 1, &strP);
        }
        // e. Increment index by 1.
        ++index;
    }

    ejsval final;

    // 10. If partial is empty, then
    if (EJS_ARRAY_LEN(partial) == 0) {
        // a. Let final be "[]".
        final = _ejs_atom_empty_array;
    }
    // 11. Else,
    else {
        // a. If gap is the empty String, then
        if (EJSVAL_EQ(state->gap, _ejs_atom_empty)) {
            // i. Let properties be a String formed by concatenating all the element Strings of partial with each
            // adjacent pair of Strings separated with code unit 0x002C (COMMA). A comma is not inserted
            // either before the first String or after the last String.
            ejsval properties = EJS_DENSE_ARRAY_ELEMENTS(partial)[0];
            ejsval comma_str = _ejs_string_new_utf8(",");
            for (int i = 1; i < EJS_ARRAY_LEN(partial); i ++) {
                properties = _ejs_string_concatv (properties, comma_str, EJS_DENSE_ARRAY_ELEMENTS(partial)[i], _ejs_null);
            }

            // ii. Let final be the result of concatenating "[", properties, and "]".
            final = _ejs_string_concatv (_ejs_string_new_utf8("["),
                                         properties,
                                         _ejs_string_new_utf8("]"),
                                         _ejs_null);
        }
        // b. Else,
        else {
            // i. Let separator be the result of concatenating code unit 0x002C (COMMA), code unit 0x000A
            // (LINE FEED), and indent.
            ejsval separator = _ejs_string_concat (_ejs_string_new_utf8(",\n"), state->indent);
            // ii. Let properties be a String formed by concatenating all the element Strings of partial with each
            // adjacent pair of Strings separated with separator. The separator String is not inserted either
            // before the first String or after the last String.
            ejsval properties = EJS_DENSE_ARRAY_ELEMENTS(partial)[0];
            for (int i = 1; i < EJS_ARRAY_LEN(partial); i ++) {
                properties = _ejs_string_concatv (properties, separator, EJS_DENSE_ARRAY_ELEMENTS(partial)[i], _ejs_null);
            }
            // iii. Let final be the result of concatenating "[", code unit 0x000A (LINE FEED), indent, properties,
            // code unit 0x000A, stepback, and "]".
            final = _ejs_string_concatv (_ejs_string_new_utf8("[\n"),
                                         state->indent,
                                         properties,
                                         _ejs_string_new_utf8("\n"),
                                         stepback,
                                         _ejs_string_new_utf8("]"),
                                         _ejs_null);
        }
    }
    // 12. Remove the last element of stack.
    _ejs_array_pop_dense(state->stack);
    // 13. Let indent be stepback.
    state->indent = stepback;
    // 14. Return final.
    return final;
}


// ES2015, June 2015
// 24.3.2.1 abstract operation SerializeJSONProperty ( key, holder )
static ejsval
SerializeJSONProperty(StringifyState* state, ejsval key, ejsval holder) {
    // 1. Let value be Get(holder, key).
    // 2. ReturnIfAbrupt(value).
    ejsval value = Get(holder, key);

    // 3. If Type(value) is Object, then
    if (EJSVAL_IS_OBJECT(value)) {
        // a. Let toJSON be Get(value, "toJSON").
        // b. ReturnIfAbrupt(toJSON).
        ejsval toJSON = Get(value, _ejs_atom_toJSON);
        // c. If IsCallable(toJSON) is true
        if (IsCallable(toJSON)) {
            // i. Let value be Call(toJSON, value, «key»).
            // ii. ReturnIfAbrupt(value).
            ejsval args[] = { key };
            value = _ejs_invoke_closure(toJSON, &value, 1, args, _ejs_undefined);
        }
    }
    // 4. If ReplacerFunction is not undefined, then
    if (!EJSVAL_IS_UNDEFINED(state->ReplacerFunction)) {
        // a. Let value be Call(ReplacerFunction, holder, «key, value»).
        // b. ReturnIfAbrupt(value).
        ejsval args[] = { key, value };
        value = _ejs_invoke_closure(state->ReplacerFunction, &holder, 2, args, _ejs_undefined);
    }
    // 5. If Type(value) is Object, then
    if (EJSVAL_IS_OBJECT(value)) {
        // a. If value has a [[NumberData]] internal slot, then
        if (EJSVAL_IS_NUMBER_OBJECT(value)) {
            // i. Let value be ToNumber(value).
            // ii. ReturnIfAbrupt(value).
            value = ToNumber(value);
        }
        // b. Else if value has a [[StringData]] internal slot, then
        else if (EJSVAL_IS_STRING_OBJECT(value)) {
            // i. Let value be ToString(value).
            // ii. ReturnIfAbrupt(value).
            value = ToString(value);
        }
        // c. Else if value has a [[BooleanData]] internal slot, then
        else if (EJSVAL_IS_BOOLEAN_OBJECT(value)) {
            // i. Let value be the value of the [[BooleanData]] internal slot of value.
            value = ToBoolean(value);
        }
    }
    // 6. If value is null, return "null".
    if (EJSVAL_IS_NULL(value)) return _ejs_atom_null;

    if (EJSVAL_IS_BOOLEAN(value)) {
        // 7. If value is true, return "true".
        // 8. If value is false, return "false".
        return EJSVAL_TO_BOOLEAN(value) ? _ejs_atom_true : _ejs_atom_false;
    }
    // 9. If Type(value) is String, return QuoteJSONString(value).
    if (EJSVAL_IS_STRING(value)) {
        return QuoteJSONString(state, value);
    }
    // 10. If Type(value) is Number, then
    if (EJSVAL_IS_NUMBER(value)) {
        // a. If value is finite, return ToString(value).
        if (isfinite (EJSVAL_TO_NUMBER(value)))
            return ToString(value);
        // b. Else, return "null".
        else
            return _ejs_atom_null;
    }
    // 11. If Type(value) is Object, and IsCallable(value) is false, then
    if (EJSVAL_IS_OBJECT(value) && !IsCallable(value)) {
        // a. Let isArray be IsArray(value).
        // b. ReturnIfAbrupt(isArray).
        EJSBool isArray = IsArray(value);

        // c. If isArray is true, return SerializeJSONArray(value).
        if (isArray)
            return SerializeJSONArray(state, value);
        // d. Else, return SerializeJSONObject(value).
        else
            return SerializeJSONObject(state, value);
    }
    // 12. Return undefined.
    return _ejs_undefined;
}

// ES2015, June 2015
// 24.3.2 JSON.stringify ( value [ , replacer [ , space ] ] )
static EJS_NATIVE_FUNC(_ejs_JSON_stringify) {
    ejsval value = _ejs_undefined;
    ejsval replacer = _ejs_undefined;
    ejsval space = _ejs_undefined;
    StringifyState state;

    if (argc > 0) value = args[0];
    if (argc > 1) replacer = args[1];
    if (argc > 2) space = args[2];

    // 1. Let stack be an empty List.
    state.stack = _ejs_array_new(0, EJS_FALSE);

    // 2. Let indent be the empty String.
    state.indent = _ejs_atom_empty;

    // 3. Let PropertyList and ReplacerFunction be undefined.
    state.PropertyList = _ejs_undefined;
    state.ReplacerFunction = _ejs_undefined;

    // 4. If Type(replacer) is Object, then
    if (EJSVAL_IS_OBJECT(replacer)) {
        // a. If IsCallable(replacer) is true, then
        if (IsCallable(replacer)) {
            // i. Let ReplacerFunction be replacer.
            state.ReplacerFunction = replacer;
        }
        // b. Else,
        else {
            // i. Let isArray be IsArray(replacer).
            // ii. ReturnIfAbrupt(isArray).
            EJSBool isArray = IsArray(replacer);
            // iii. If isArray is true, then
            if (isArray) {
                // 1. Let PropertyList be an empty List
                state.PropertyList = _ejs_array_new(0, EJS_FALSE);
                // 2. Let len be ToLength(Get(replacer, "length")).
                // 3. ReturnIfAbrupt(len).
                int64_t len = ToLength(Get(replacer, _ejs_atom_length));

                // 4. Let k be 0.
                int64_t k = 0;
                // 5. Repeat while k<len.
                while (k < len) {
                    // a. Let v be Get(replacer, ToString(k)).
                    // b. ReturnIfAbrupt(v).
                    ejsval v = Get(replacer, ToString(NUMBER_TO_EJSVAL(k)));
                    // c. Let item be undefined.
                    ejsval item = _ejs_undefined;
                    // d. If Type(v) is String, let item be v.
                    if (EJSVAL_IS_STRING(v))
                        item = v;
                    // e. Else if Type(v) is Number, let item be ToString(v).
                    else if (EJSVAL_IS_NUMBER(v))
                        item = ToString(v);
                    // f. Else if Type(v) is Object, then
                    else if (EJSVAL_IS_OBJECT(v)) {
                        // i. If v has a [[StringData]] or [[NumberData]] internal slot, let item be ToString(v).
                        // ii. ReturnIfAbrupt(item).
                        if (EJSVAL_IS_STRING_OBJECT(v) || EJSVAL_IS_NUMBER_OBJECT(v))
                            item = ToString(v);
                    }
                    // g. If item is not undefined and item is not currently an element of PropertyList, then
                    if (!EJSVAL_IS_UNDEFINED(item) /* XXX && not currently in PropertyList */) {
                        // i. Append item to the end of PropertyList.
                        _ejs_array_push_dense (state.PropertyList, 1, &item);
                    }
                    // h. Let k be k+1.
                    k += 1;
                }
            }
        }
    }
    // 5. If Type(space) is Object, then
    if (EJSVAL_IS_OBJECT(space)) {
        // a. If space has a [[NumberData]] internal slot, then
        if (EJSVAL_IS_NUMBER_OBJECT(space)) {
            // i. Let space be ToNumber(space).
            // ii. ReturnIfAbrupt(space).
            space = ToNumber(space);
        }
        // b. Else if space has a [[StringData]] internal slot, then
        else if (EJSVAL_IS_STRING_OBJECT(space)) {
            // i. Let space be ToString(space).
            // ii. ReturnIfAbrupt(space).
            space = ToString(space);
        }
    }

    // 6. If Type(space) is Number, then
    if (EJSVAL_IS_NUMBER(space)) {
        static jschar spaces[10] = { 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020 };
        // a. Let space be min(10, ToInteger(space)).
        int space_int = ToInteger(space);
        int space_count = MIN(10, space_int);
        // b. Set gap to a String containing space occurrences of code unit 0x0020 (SPACE). This will be the empty String if space is less than 1.
        if (space_count < 1) {
            state.gap = _ejs_atom_empty;
        }
        else {
            state.gap = _ejs_string_new_ucs2_len(spaces, space_count);
        }

    }
    // 7. Else if Type(space) is String, then
    else if (EJSVAL_IS_STRING(space)) {
        // a. If the number of elements in space is 10 or less, set gap to space otherwise set gap to a String consisting of the first 10 elements of space.
        if (EJSVAL_TO_STRLEN(space) <= 10) {
            state.gap = space;
        }
        else {
            state.gap = _ejs_string_new_substring(space, 0, 10);
        }
    }
    // 8. Else
    else {
        // a. Set gap to the empty String.
        state.gap = _ejs_atom_empty;
    }
    // 9. Let wrapper be ObjectCreate(%ObjectPrototype%).
    ejsval wrapper = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);

    // 10. Let status be CreateDataProperty(wrapper, the empty String, value).
    _ejs_object_setprop (wrapper, _ejs_atom_empty, value);
    // 11. Assert: status is true.
    // 12. Return SerializeJSONProperty(the empty String, wrapper).
    return SerializeJSONProperty(&state, _ejs_atom_empty, wrapper);
}

ejsval
_ejs_json_stringify (ejsval arg)
{
    ejsval undef_this = _ejs_undefined;
    return _ejs_JSON_stringify(_ejs_undefined, &undef_this, 1, &arg, _ejs_undefined);
}

void
_ejs_json_init(ejsval global)
{
    _ejs_JSON = _ejs_object_new (_ejs_Object_prototype, &_ejs_Object_specops);
    _ejs_object_setprop (global, _ejs_atom_JSON, _ejs_JSON);

    _ejs_object_define_value_property (_ejs_JSON, _ejs_Symbol_toStringTag, _ejs_atom_JSON, EJS_PROP_NOT_ENUMERABLE | EJS_PROP_NOT_WRITABLE | EJS_PROP_CONFIGURABLE);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_JSON, x, _ejs_JSON_##x)

    OBJ_METHOD(parse);
    OBJ_METHOD(stringify);

#undef OBJ_METHOD
}
