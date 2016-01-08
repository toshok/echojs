/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <stdlib.h>

#include "ejs-error.h"
#include "ejs-ops.h"
#include "ejs-string.h"
#include "ejs-uri.h"

static ejsval
Encode (ejsval string, ejsval unescaped)
{
    EJSPrimString *stringObj = EJSVAL_TO_STRING(string);
    jschar *unescapedStr = EJSVAL_TO_FLAT_STRING(unescaped);

    /* 1. Let strLen be the number of code units in string. */
    int32_t strLen = stringObj->length;

    /* 2. Let R be the empty String. */
    ejsval R = _ejs_atom_empty;

    /* 3. Let k be 0. */
    int32_t k = 0;

    /* 4. Repeat. */
    for (;;) {
        /* a. If k equals strLen, return R. */
        if (k == strLen)
            return R;

        /* b. Let C be the code unit at index k within string. */
        jschar C = _ejs_string_ucs2_at (stringObj, k);
        jschar C_arr [2] = { C, 0 };

        /* c. If C is in unescapedSet, then */
        jschar *p = ucs2_strstr (unescapedStr, C_arr);
        if (p) {
            /* i. Let S be a String containing only the code unit C. */
            ejsval S = _ejs_string_new_substring (string, k, 1);

            /* ii. Let R be a new String value computed by concatenating the previous value of R and S. */
            R = _ejs_string_concat (R, S);
        }
        /* d. Else C is not in unescapedSet, */
        else {
            /* i. If the code unit value of C is not less than 0xDC00 and not greater than 0xDFFF, throw a URIError exception. */
            if (C >= 0xDC00 && C <= 0xDFFF)
                _ejs_throw_nativeerror_utf8 (EJS_URI_ERROR, "URI malformed");

            /* ii. If the code unit value of C is less than 0xD800 or greater than 0xDBFF, then Let V be the code unit value of C. */
            jschar V;
            if (C < 0xD800 || C > 0xDBFF)
                V = C;
            /* iii. Else, */
            else {
                /* 1. Increase k by 1. */
                k++;

                /* 2. If k equals strLen, throw a URIError exception. */
                if (k == strLen)
                    _ejs_throw_nativeerror_utf8 (EJS_URI_ERROR, "URI malformed");

                /* 3. Let kChar be the code unit value of the code unit at index k within string. */
                jschar kChar = _ejs_string_ucs2_at (stringObj, k);

                /* 4. If kChar is less than 0xDC00 or greater than 0xDFFF, throw a URIError exception. */
                if (kChar < 0xDC00 || kChar > 0xDFFF)
                    _ejs_throw_nativeerror_utf8 (EJS_URI_ERROR, "URI malformed");

                /* 5. Let V be (((the code unit value of C) – 0xD800) × 0x400 + (kChar – 0xDC00) + 0x10000). */
                V = (C - 0xD800) * 0x400 + (kChar - 0xDC00) + 0x10000;
            }

            /* iv. Let Octets be the array of octets resulting by applying the UTF-8 transformation to V, and let L be the array size. */
            char octets[4];
            int32_t L = ucs2_to_utf8_char (V, octets);

            /* v. Let j be 0. */
            int32_t j = 0;

            /* vi. Repeat, while j < L. */
            while (j < L) {
                /* 1.  Let jOctet be the value at index j within Octets. */
                char jOctet = octets [j];

                /* 2. Let S be a String containing three code units “%XY” where XY are two uppercase hexadecimal
                 * digits encoding the value of jOctet. */
                char buff[4];
                sprintf(buff, "%%%X", jOctet);
                ejsval S = _ejs_string_new_utf8 (buff);

                /* 3. Let R be a new String value computed by concatenating the previous value of R and S. */
                R = _ejs_string_concat (R, S);

                /* 4. Increase j by 1. */
                j++;
            }
        }

        /* e. Increase k by 1. */
        k++;
    }
}

static ejsval _ejs_uriUnescaped;

ejsval _ejs_decodeURI EJSVAL_ALIGNMENT;
ejsval _ejs_decodeURIComponent EJSVAL_ALIGNMENT;
ejsval _ejs_encodeURI EJSVAL_ALIGNMENT;
ejsval _ejs_encodeURIComponent EJSVAL_ALIGNMENT;

EJS_NATIVE_FUNC(_ejs_decodeURI_impl) {
    EJS_NOT_IMPLEMENTED();
}

EJS_NATIVE_FUNC(_ejs_decodeURIComponent_impl) {
    EJS_NOT_IMPLEMENTED();
}


EJS_NATIVE_FUNC(_ejs_encodeURI_impl) {
    EJS_NOT_IMPLEMENTED();
}

EJS_NATIVE_FUNC(_ejs_encodeURIComponent_impl) {
    ejsval uriComponent = _ejs_undefined;

    if (argc >= 1)
        uriComponent = args [0];

    /* 1. Let componentString be ToString(uriComponent). */
    /* 2. ReturnIfAbrupt(componentString). */
    ejsval componentString = ToString(uriComponent);

    /* 3. Let unescapedURIComponentSet be a String containing one instance of each code unit valid in uriUnescaped. */
    ejsval unescapedURIComponentSet = _ejs_uriUnescaped;

    /* 4. Return Encode(componentString, unescapedURIComponentSet) */
    return Encode (componentString, unescapedURIComponentSet);
}

void
_ejs_uri_init (ejsval global)
{
    /* 18.2.6.1 URI Syntax and Semantics */
    /* a 'funny' thing is that we are flatting the string whenever we are encoding it. It would be nice to have it
     * in that state always! */
    char *uriUnescaped = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~*'()";
    _ejs_uriUnescaped = _ejs_string_new_utf8 (uriUnescaped);
}

