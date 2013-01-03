/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */
#include <assert.h>
#include <string.h>
#include <math.h>

#include "ejs-value.h"
#include "ejs-array.h"
#include "ejs-string.h"
#include "ejs-function.h"
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
    _ejs_string_specop_finalize,
    _ejs_string_specop_scan
};

EJSObject* _ejs_string_alloc_instance()
{
    return (EJSObject*)_ejs_gc_new (EJSString);
}

ejsval _ejs_String;
ejsval _ejs_String_proto;

static ejsval
_ejs_String_impl (ejsval env, ejsval _this, int argc, ejsval *args)
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
_ejs_String_prototype_toString (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJSString *str = (EJSString*)EJSVAL_TO_OBJECT(_this);

    return _ejs_string_new_utf8 (EJSVAL_TO_FLAT_STRING(str->primStr));
}

static ejsval
_ejs_String_prototype_replace (ejsval env, ejsval _this, int argc, ejsval *args)
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
_ejs_String_prototype_charAt (ejsval env, ejsval _this, int argc, ejsval *args)
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
_ejs_String_prototype_charCodeAt (ejsval env, ejsval _this, int argc, ejsval *args)
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
_ejs_String_prototype_concat (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_indexOf (ejsval env, ejsval _this, int argc, ejsval *args)
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
_ejs_String_prototype_lastIndexOf (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_localeCompare (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_match (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_search (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_substring (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_toLowerCase (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_toLocaleLowerCase (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_toUpperCase (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_toLocaleUpperCase (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_trim (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_valueOf (ejsval env, ejsval _this, int argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
}

static ejsval
_ejs_String_prototype_split (ejsval env, ejsval _this, int argc, ejsval *args)
{
    // for now let's just not split anything at all, return the original string as element0 of the array.

    ejsval rv = _ejs_array_new (1);
    _ejs_object_setprop (rv, NUMBER_TO_EJSVAL (0), _this);
    return rv;
}

static ejsval
_ejs_String_prototype_slice (ejsval env, ejsval _this, int argc, ejsval *args)
{
    // assert argc >= 1

    ejsval start = args[0];
    ejsval end = argc > 1 ? args[1] : _ejs_undefined;

    // Call CheckObjectCoercible passing the this value as its argument.
    // Let S be the result of calling ToString, giving it the this value as its argument.
    ejsval S = ToString(_this);
    // Let len be the number of characters in S.
    int len = EJSVAL_TO_STRLEN(S);
    // Let intStart be ToInteger(start).
    int intStart = ToInteger(start);

    // If end is undefined, let intEnd be len; else let intEnd be ToInteger(end).
    int intEnd = EJSVAL_IS_UNDEFINED(end) ? len : ToInteger(end);

    // If intStart is negative, let from be max(len + intStart,0); else let from be min(intStart, len).
    int from = intStart < 0 ? MAX(len + intStart, 0) : MIN(intStart, len);

    // If intEnd is negative, let to be max(len + intEnd,0); else let to be min(intEnd, len).
    int to = intEnd < 0 ? MAX(len + intEnd, 0) : MIN(intEnd, len);

    // Let span be max(to – from,0).
    int span = MAX(to - from, 0);

    // Return a String containing span consecutive characters from S beginning with the character at position from.
    return _ejs_string_new_utf8_len (EJSVAL_TO_FLAT_STRING(S), span);
}

void
_ejs_string_init(ejsval global)
{
    START_SHADOW_STACK_FRAME;

    _ejs_gc_add_named_root (_ejs_String_proto);
    _ejs_String_proto = _ejs_object_new(_ejs_null);
  
    ADD_STACK_ROOT(ejsval, tmpobj, _ejs_function_new (_ejs_null, _ejs_atom_String, (EJSClosureFunc)_ejs_String_impl));
    _ejs_String = tmpobj;

    _ejs_object_setprop (_ejs_String,       _ejs_atom_prototype,  _ejs_String_proto);

#define OBJ_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_String, EJS_STRINGIFY(x), _ejs_String_##x)
#define PROTO_METHOD(x) EJS_INSTALL_FUNCTION(_ejs_String_proto, EJS_STRINGIFY(x), _ejs_String_prototype_##x)

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
    PROTO_METHOD(substring);
    PROTO_METHOD(toLocaleLowerCase);
    PROTO_METHOD(toLocaleUpperCase);
    PROTO_METHOD(toLowerCase);
    PROTO_METHOD(toString);
    PROTO_METHOD(toUpperCase);
    PROTO_METHOD(trim);
    PROTO_METHOD(valueOf);

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
