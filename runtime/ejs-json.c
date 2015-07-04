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
static ejsval
_ejs_JSON_parse (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
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
    if (EJSVAL_IS_CALLABLE(reviver)) {
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
    // XXX stack goes here
    ejsval indent;
    ejsval gap;
    ejsval ReplacerFunction;
    ejsval PropertyList;
} StringifyState;

/* abstract operations JA from 15.12.3 */
static ejsval JA(StringifyState *state, ejsval value);
static ejsval JO(StringifyState *state, ejsval value);
static ejsval Quote(StringifyState *state, ejsval value);
static ejsval Str(StringifyState *state, ejsval key, ejsval holder);

static ejsval
JA(StringifyState *state, ejsval value)
{
    ejsval final;

    /* 1. If stack contains value then throw a TypeError exception because the structure is cyclical. */
    /* 2. Append value to stack. */
    /* 3. Let stepback be indent. */
    ejsval stepback = state->indent;
    /* 4. Let indent be the concatenation of indent and gap. */
    state->indent = _ejs_string_concat (state->indent, state->gap);
    /* 5. Let partial be an empty List. */
    ejsval partial = _ejs_array_new (0, EJS_FALSE);

    /* 6. Let len be the result of calling the [[Get]] internal method of value with argument "length". */
    int len = EJS_ARRAY_LEN(value);

    /* 7. Let index be 0. */
    int index = 0;

    /* 8. Repeat while index < len */
    while (index < len) {
        /*    a. Let strP be the result of calling the abstract operation Str with arguments ToString(index) and value.  */
        ejsval strP = Str (state, NUMBER_TO_EJSVAL(index), value);
        /*    b. If strP is undefined */
        if (EJSVAL_IS_UNDEFINED(strP)) {
            /*       i. Append "null" to partial. */
            _ejs_array_push_dense (partial, 1, &_ejs_atom_null);
            
        }
        /*    c. Else */
        else {
            /*       i. Append  strP to partial. */
            _ejs_array_push_dense (partial, 1, &strP);
        }
        /*    d. Increment index by 1. */
        ++index;
    }
    /* 9. If partial is empty ,then */
    if (EJS_ARRAY_LEN(partial) == 0) {
        /*    a. Let final be "[]". */
        final = _ejs_atom_empty_array;
    }
    /* 10. Else */
    else {
        /*     a. If gap is the empty String */
        if (EJSVAL_EQ(state->gap, _ejs_atom_empty)) {
            /*        i. Let properties be a String formed by concatenating all the element Strings of partial with
                         each adjacent pair of Strings separated with the comma character. A comma is not inserted 
                         either before the first String or after the last String. */
            ejsval properties = EJS_DENSE_ARRAY_ELEMENTS(partial)[0];
            ejsval comma_str = _ejs_string_new_utf8(",");
            for (int i = 1; i < EJS_ARRAY_LEN(partial); i ++) {
                properties = _ejs_string_concatv (properties, comma_str, EJS_DENSE_ARRAY_ELEMENTS(partial)[i], _ejs_null);
            }

            /*        ii. Let final be the result of concatenating "[", properties, and "]". */
            final = _ejs_string_concatv (_ejs_string_new_utf8("["),
                                         properties,
                                         _ejs_string_new_utf8("]"),
                                         _ejs_null);
        }
        /*     b. Else */
        else {
            /*        i. Let separator be the result of concatenating the comma character, the line feed character, 
                      and indent. */
            ejsval separator = _ejs_string_concat (_ejs_string_new_utf8(",\n"), state->indent);
            /*        ii. Let properties be a String formed by concatenating all the element Strings of partial with 
                          each adjacent pair of Strings separated with separator. The separator String is not inserted 
                          either before the first String or after the last String. */
            ejsval properties = EJS_DENSE_ARRAY_ELEMENTS(partial)[0];
            for (int i = 1; i < EJS_ARRAY_LEN(partial); i ++) {
                properties = _ejs_string_concatv (properties, separator, EJS_DENSE_ARRAY_ELEMENTS(partial)[i], _ejs_null);
            }
            /*        iii. Let final be the result of concatenating "[", the line feed character, indent, properties, the 
                           line feed character, stepback, and "]". */
            final = _ejs_string_concatv (_ejs_string_new_utf8("[\n"),
                                         state->indent,
                                         properties,
                                         _ejs_string_new_utf8("\n"),
                                         stepback,
                                         _ejs_string_new_utf8("]"),
                                         _ejs_null);
        }
    }
    /* 11. Remove the last element of stack. */
    /* 12. Let indent be stepback. */
    state->indent = stepback;
    /* 13. Return final. */
    return final;
}

/* abstract operation JO from 15.12.3 */
static ejsval
JO(StringifyState *state, ejsval value)
{
    ejsval final = _ejs_undefined;

    /* 1. If stack contains value then throw a TypeError exception because the structure is cyclical. */
    /* 2. Append value to stack. */
    /* 3. Let stepback be indent. */
    ejsval stepback = state->indent;
    /* 4. Let indent be the concatenation of indent and gap. */
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

    /* 5. If PropertyList is not undefined, then */
    if (!EJSVAL_IS_UNDEFINED(state->PropertyList)) {
        /*    a. Let K be PropertyList. */
        K = state->PropertyList;
    }
    /* 6. Else */
    else {
        /*    a. Let K be an internal List of Strings consisting of the names of all the own properties of value whose 
              [[Enumerable]] attribute is true. The ordering of the Strings should be the same as that used by the 
              Object.keys standard built-in function. */
        EJSObject* value_obj = EJSVAL_TO_OBJECT(value);
        K = _ejs_array_new (0, EJS_FALSE);
        for (_EJSPropertyMapEntry *s = value_obj->map->head_insert; s; s = s->next_insert) {
            if (!_ejs_property_desc_is_enumerable(s->desc))
                continue;

            ejsval propname = s->name;
            _ejs_array_push_dense(K, 1, &propname);
        }
    }
    /* 7. Let partial be an empty List. */
    ejsval partial = _ejs_array_new (0, EJS_FALSE);

    /* 8. For each element P of K. */
    for (int kk = 0; kk < EJS_ARRAY_LEN(K); kk++) {
        ejsval P = EJS_DENSE_ARRAY_ELEMENTS(K)[kk];

        /*    a. Let strP be the result of calling the abstract operation Str with arguments P and value. */
        ejsval strP = Str(state, P, value);

        /*    b. If strP is not undefined */
        if (!EJSVAL_IS_UNDEFINED(strP)) {
            /*       i. Let member be the result of calling the abstract operation Quote with argument P. */
            ejsval member = Quote (state, P);
            /*       ii. Let member be the concatenation of member and the colon character. */
            member = _ejs_string_concat (member, _ejs_string_new_utf8(":"));
            /*       iii. If gap is not the empty String */
            if (!EJSVAL_EQ(state->gap, _ejs_atom_empty)) {
                /*            1. Let member be the concatenation of member and the space character. */
                member = _ejs_string_concat (member, _ejs_string_new_utf8(" "));
            }
            /*       iv. Let member be the concatenation of member and strP. */
            member = _ejs_string_concat (member, strP);
            /*       v. Append member to partial. */
            _ejs_array_push_dense(partial, 1, &member);
        }
    }
    /* 9. If partial is empty, then */
    if (EJS_ARRAY_LEN(partial) == 0) {
        /*    a. Let final be "{}". */
        final = _ejs_atom_empty_object;
    }
    /* 10. Else */
    else {
        /*     a. If gap is the empty String */
        if (EJSVAL_EQ(state->gap, _ejs_atom_empty)) {
            /*        i. Let properties be a String formed by concatenating all the element Strings of partial with 
                         each adjacent pair of Strings separated with the comma character. A comma is not inserted 
                         either before the first String or after the last String.  */
            ejsval properties = EJS_DENSE_ARRAY_ELEMENTS(partial)[0];
            ejsval comma_str = _ejs_string_new_utf8(",");
            for (int i = 1; i < EJS_ARRAY_LEN(partial); i ++) {
                properties = _ejs_string_concatv (properties, comma_str, EJS_DENSE_ARRAY_ELEMENTS(partial)[i], _ejs_null);
            }
            /*        ii. Let final be the result of concatenating "{", properties, and "}". */
            final = _ejs_string_concatv (_ejs_string_new_utf8("{"),
                                         properties,
                                         _ejs_string_new_utf8("}"),
                                         _ejs_null);
        }
        /*     b. Else gap is not the empty String */
        else {
            /*        i. Let separator be the result of concatenating the comma character, the line feed character, 
                         and indent. */
            ejsval separator = _ejs_string_concat (_ejs_string_new_utf8(",\n"), state->indent);
            /*        ii. Let properties be a String formed by concatenating all the element Strings of partial with 
                          each adjacent pair of Strings separated with separator. The separator String is not inserted 
                          either before the first String or after the last String. */
            ejsval properties = EJS_DENSE_ARRAY_ELEMENTS(partial)[0];
            for (int i = 1; i < EJS_ARRAY_LEN(partial); i ++) {
                properties = _ejs_string_concatv (properties, separator, EJS_DENSE_ARRAY_ELEMENTS(partial)[i], _ejs_null);
            }
            /*        iii. Let final be the result of concatenating "{", the line feed character, indent, properties, the 
                           line feed character, stepback, and "}". */
            final = _ejs_string_concatv (_ejs_string_new_utf8("{\n"),
                                         state->indent,
                                         properties,
                                         _ejs_string_new_utf8("\n"),
                                         stepback,
                                         _ejs_string_new_utf8("}"),
                                         _ejs_null);
        }
    }

    /* 11. Remove the last element of stack. */

    /* 12. Let indent be stepback. */
    state->indent = stepback;
    /* 13. Return final. */
    return final;
}

/* abstract operation Quote from 15.12.3 */
static ejsval
Quote(StringifyState *state, ejsval value)
{
    int len = EJSVAL_TO_STRLEN(value);
    jschar *product = malloc((len * 4 + 2) * sizeof(jschar));
    EJSPrimString* prim_value = _ejs_primstring_flatten(EJSVAL_TO_STRING(value)); // this is pretty terrible.. we need a way to simply iterate over characters in a string

    int pi = 0;
    /* 1. Let product be the double quote character. */
    product[pi++] = '"';

    /* 2. For each character C in value */
    for (int vi = 0; vi < len; vi++) {
        jschar C = prim_value->data.flat[vi];
        /*    a. If C is the double quote character or the backslash character */
        if (C == '\"' || C == '\\') {
            /*       i. Let product be the concatenation of product and the backslash character. */
            product[pi++] = '\\';
            /*       ii. Let product be the concatenation of product and C. */
            product[pi++] = C;
        }
        /*    b. Else if C is backspace, formfeed, newline, carriage return, or tab */
        else if (C == '\b' ||
                 C == '\f' ||
                 C == '\n' ||
                 C == '\r' ||
                 C == '\t') {
            /*       i. Let product be the concatenation of product and the backslash character. */
            product[pi++] = '\\';
            /*       ii. Let abbrev be the character corresponding to the value of C as follows: */
            /*           backspace "b" */
            jschar abbrev;
            if (C == '\b') abbrev = 'b';
            /*           formfeed "f" */
            if (C == '\f') abbrev = 'f';
            /*           newline "n" */
            if (C == '\n') abbrev = 'n';
            /*           carriage return "r" */
            if (C == '\r') abbrev = 'r';
            /*           tab "t" */
            if (C == '\t') abbrev = 't';
            /*       iii. Let product be the concatenation of product and abbrev. */
            product[pi++] = abbrev;
        }
        /*    c. Else if C is a control character having a code unit value less than the space character */
        else if (C < ' ') {
            /*       i. Let product be the concatenation of product and the backslash character. */
            product[pi++] = '\\';
            /*       ii. Let product be the concatenation of product and "u". */
            product[pi++] = 'u';
            /*       iii. Let hex be the result of converting the numeric code unit value of C to a String of four 
                     hexadecimal digits. */
            static char* hexdigits = "012356789abcdef";
            /*       iv. Let product be the concatenation of product and hex. */
            product[pi++] = '0';
            product[pi++] = '0';
            product[pi++] = hexdigits[(C & 0xf0) >> 4];
            product[pi++] = hexdigits[C & 0xf];
        }
        /*    d. Else */
        else {
            /*       i. Let product be the concatenation of product and C. */
            product[pi++] = C;
        }
    }
    /* 3. Let product be the concatenation of product and the double quote character. */
    product[pi++] = '\"';
    /* 4. Return product. */
    ejsval rv = _ejs_string_new_ucs2_len(product, pi);
    free (product);
    return rv;
}

/* abstract operation Str from 15.12.3 */
static ejsval
Str(StringifyState *state, ejsval key, ejsval holder)
{
    /* 1. Let value be the result of calling the [[Get]] internal method of holder with argument key. */
    ejsval value = OP(EJSVAL_TO_OBJECT(holder),Get)(holder, key, holder);

    /* 2. If Type(value) is Object, then */
    if (EJSVAL_IS_OBJECT(value)) {
        /*    a. Let toJSON be the result of calling the [[Get]] internal method of value with argument "toJSON". */
        ejsval toJSON = OP(EJSVAL_TO_OBJECT(holder),Get)(holder, _ejs_atom_toJSON, holder);
        /*    b. If IsCallable(toJSON) is true */
        if (EJSVAL_IS_CALLABLE(toJSON)) {
            /*       i. Let value be the result of calling the [[Call]] internal method of toJSON passing value as the 
                     this value and with an argument list consisting of key. */
            _ejs_invoke_closure (toJSON, value, 1, &key);
        }
    }

    /* 3. If ReplacerFunction is not undefined, then */
    if (!EJSVAL_IS_UNDEFINED(state->ReplacerFunction)) {
        /*    a. Let value be the result of calling the [[Call]] internal method of ReplacerFunction passing holder as 
              the this value and with an argument list consisting of key and value. */
        EJS_NOT_IMPLEMENTED();
    }

    /* 4. If Type(value) is Object then, */
    if (EJSVAL_IS_OBJECT(value)) {
        /*    a. If the [[Class]] internal property of value is "Number" then, */
        if (EJSVAL_IS_NUMBER_OBJECT(value)) {
            /*       i. Let value be ToNumber(value). */
            value = NUMBER_TO_EJSVAL(ToDouble(value));
        }
        /*    b. Else if the [[Class]] internal property of value is "String" then, */
        else if (EJSVAL_IS_STRING_OBJECT(value)) {
            /*       i. Let value be ToString(value). */
            value = ToString(value);
        }
        /*    c. Else if the [[Class]] internal property of value is "Boolean" then, */
        else if (EJSVAL_IS_BOOLEAN_OBJECT(value)) {
            /*       i. Let value be the value of the [[PrimitiveValue]] internal property of  value. */
            value = ToBoolean(value);
        }
    }

    /* 5. If value is null then return "null". */
    if (EJSVAL_IS_NULL(value)) {
        return _ejs_atom_null;
    }

    if (EJSVAL_IS_BOOLEAN(value)) {
        /* 6. If value is true then return "true". */
        /* 7. If value is false then return "false". */
        return EJSVAL_TO_BOOLEAN(value) ? _ejs_atom_true : _ejs_atom_false;
    }

    /* 8. If Type(value) is String, then return the result of calling the abstract operation Quote with argument value. */
    if (EJSVAL_IS_STRING(value)) {
        return Quote(state, value);
    }

    /* 9. If Type(value) is Number */
    if (EJSVAL_IS_NUMBER(value)) {
        /*    a. If value is finite then return ToString(value). */
        if (isfinite (EJSVAL_TO_NUMBER(value)))
            return ToString(value);

        /*    b. Else, return "null". */
        return _ejs_null;
    }

    /* 10. If Type(value) is Object, and IsCallable(value) is false */
    if (EJSVAL_IS_OBJECT(value) && !EJSVAL_IS_CALLABLE(value)) {
        /*     a. If the [[Class]] internal property of value is "Array" then */
        if (EJSVAL_IS_ARRAY(value)) {
            /*        i. Return the result of calling the abstract operation JA with argument value. */
            return JA(state, value);
        }
        else {
            /*     b. Else, return the result of calling the abstract operation JO with argument value. */
            return JO(state, value);
        }
    }

    /* 11. Return undefined. */
    return _ejs_undefined;
}

/* 15.12.3 */
static ejsval
_ejs_JSON_stringify (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    ejsval value = _ejs_undefined;
    ejsval replacer = _ejs_undefined;
    ejsval space = _ejs_undefined;
    StringifyState state;

    if (argc > 0) value = args[0];
    if (argc > 1) replacer = args[1];
    if (argc > 2) space = args[2];

    /* 1. Let stack be an empty List. */
    
    /* 2. Let indent be the empty String. */
    state.indent = _ejs_atom_empty;

    /* 3. Let PropertyList and ReplacerFunction be undefined. */
    state.PropertyList = _ejs_undefined;
    state.ReplacerFunction = _ejs_undefined;

    /* 4. If Type(replacer) is Object, then */
    if (EJSVAL_IS_OBJECT(replacer)) {
        /*    a. If IsCallable(replacer) is true, then */
        if (EJSVAL_IS_CALLABLE(replacer)) {
            /*       i. Let ReplacerFunction be replacer. */
            state.ReplacerFunction = replacer;
        }
        /*    b. Else if the [[Class]] internal property of replacer is "Array", then */
        else if (EJSVAL_IS_ARRAY(replacer)) {
            /*       i. Let PropertyList  be an empty internal List */
            state.PropertyList = _ejs_array_new(0, EJS_FALSE);
            /*       ii. For each value v of a property of replacer that has an array index property name. The
                         properties are enumerated in the ascending array index order of their names. */
            for (int i = 0; i < EJS_ARRAY_LEN(replacer); i ++) {
                ejsval v = EJS_DENSE_ARRAY_ELEMENTS(replacer)[i];
                /*           1. Let item be undefined. */
                ejsval item = _ejs_undefined;
                /*           2. If Type(v) is String then let item be v. */
                if (EJSVAL_IS_STRING(v))
                    item = v;
                /*           3. Else if Type(v) is Number then let item be ToString(v). */
                else if (EJSVAL_IS_NUMBER(v))
                    item = ToString(v);
                /*           4. Else if Type(v) is Object then, */
                else if (EJSVAL_IS_OBJECT(v)) {
                    /*              a If the [[Class]] internal property of v is "String" or "Number" then let 
                                    item be ToString(v). */
                    if (EJSVAL_IS_STRING_OBJECT(v) || EJSVAL_IS_NUMBER_OBJECT(v))
                        item = ToString(v);
                }
                /*           5. If item is not undefined and item is not currently an element of PropertyList then, */
                if (!EJSVAL_IS_UNDEFINED(item) /* XXX && not currently in PropertyList */) {
                    /*              a Append item to the end of PropertyList. */
                    _ejs_array_push_dense (state.PropertyList, 1, &item);
                }
            }
        }
    }

    /* 5. If Type(space) is Object then, */
    if (EJSVAL_IS_OBJECT(space)) {
        /*    a. If the [[Class]] internal property of space is "Number" then, */
        if (EJSVAL_IS_NUMBER_OBJECT(space)) {
            /*       i. Let space be ToNumber(space). */
            space = NUMBER_TO_EJSVAL(ToDouble(space));
        }
        /*    b. Else if the [[Class]] internal property of space is "String" then, */
        else if (EJSVAL_IS_STRING(space)) {
            /*       i. Let space be ToString(space). */
            space = ToString(space);
        }
    }

    state.gap = _ejs_atom_empty;

    /* 6. If Type(space) is Number */
    if (EJSVAL_IS_NUMBER(space)) {
        /*    a. Let space be min(10, ToInteger(space)). */
        int space_int = MIN(10, ToInteger(space));

        /*    b. Set gap to a String containing space space characters. This will be the empty String if space is less 
              than 1. */
        if (space_int < 1) {
            state.gap = _ejs_atom_empty;
        }
        else {
            char *space_str = (char*)malloc (space_int + 1);
            memset (space_str, ' ', space_int);
            space_str[space_int] = 0;
            state.gap = _ejs_string_new_utf8 (space_str);
            free (space_str);
        }
    }
    /* 7. Else if Type(space) is String */
    else if (EJSVAL_IS_STRING(space)) {
        /*    a. If the number of characters in space is 10 or less, set gap to space otherwise set gap to a String
              consisting of the first 10 characters of space. */
        jschar *flattened = EJSVAL_TO_FLAT_STRING(space);
        state.gap = _ejs_string_new_ucs2_len (flattened, 10);
    }
    /* 8. Else */
    else {
        /*    a. Set gap to the empty String. */
        state.gap = _ejs_atom_empty;
    }

    /* 9. Let wrapper be a new object created as if by the expression new Object(), where Object is the 
       standard built-in constructor with that name. */
    ejsval wrapper = _ejs_object_new(_ejs_Object_prototype, &_ejs_Object_specops);
    /* 10. Call the [[DefineOwnProperty]]  internal method of wrapper with arguments the empty String, the Property 
       Descriptor {[[Value]]: value, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false. */
    EJSPropertyDesc desc = { .value = value, .flags = EJS_PROP_FLAGS_VALUE_SET | EJS_PROP_ENUMERABLE | EJS_PROP_CONFIGURABLE | EJS_PROP_WRITABLE };
    OP(EJSVAL_TO_OBJECT(wrapper), DefineOwnProperty)(wrapper, _ejs_atom_empty, &desc, EJS_FALSE);
    /* 11. Return the result of calling the abstract operation Str with the empty String and wrapper. */
    return Str(&state, _ejs_atom_empty, wrapper);
}

ejsval
_ejs_json_stringify (ejsval arg)
{
    return _ejs_JSON_stringify(_ejs_undefined, _ejs_undefined, 1, &arg);
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
