/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 */

#include <assert.h>
#include <string.h>

#include "ejs-ops.h"
#include "ejs-value.h"
#include "ejs-regexp.h"
#include "ejs-function.h"
#include "ejs-string.h"
#include "ejs-error.h"

#include "pcre.h"

static ejsval _ejs_regexp_specop_get (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_regexp_specop_get_own_property (ejsval obj, ejsval propertyName);
static EJSPropertyDesc* _ejs_regexp_specop_get_property (ejsval obj, ejsval propertyName);
static void      _ejs_regexp_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag);
static EJSBool   _ejs_regexp_specop_can_put (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_regexp_specop_has_property (ejsval obj, ejsval propertyName);
static EJSBool   _ejs_regexp_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag);
static ejsval _ejs_regexp_specop_default_value (ejsval obj, const char *hint);
static EJSBool   _ejs_regexp_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag);
static EJSObject* _ejs_regexp_specop_allocate ();
static void      _ejs_regexp_specop_finalize (EJSObject* obj);
static void      _ejs_regexp_specop_scan (EJSObject* obj, EJSValueFunc scan_func);

EJSSpecOps _ejs_regexp_specops = {
    "RegExp",
    _ejs_regexp_specop_get,
    _ejs_regexp_specop_get_own_property,
    _ejs_regexp_specop_get_property,
    _ejs_regexp_specop_put,
    _ejs_regexp_specop_can_put,
    _ejs_regexp_specop_has_property,
    _ejs_regexp_specop_delete,
    _ejs_regexp_specop_default_value,
    _ejs_regexp_specop_define_own_property,
    NULL, /* [[HasInstance]] */

    _ejs_regexp_specop_allocate,
    _ejs_regexp_specop_finalize,
    _ejs_regexp_specop_scan
};

static ejsval _ejs_RegExp_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args);

ejsval
_ejs_regexp_new (ejsval pattern, ejsval flags)
{
    EJSRegExp* rv = _ejs_gc_new(EJSRegExp);

    _ejs_init_object ((EJSObject*)rv, _ejs_RegExp_proto, &_ejs_regexp_specops);

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

            fprintf (stderr, "substring match is %s\n", ucs2_to_utf8(_ejs_string_flatten(substr_match)->data.flat));
            fprintf (stderr, "capture is %s\n", ucs2_to_utf8(_ejs_string_flatten(capture)->data.flat));

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


ejsval _ejs_RegExp;
ejsval _ejs_RegExp_proto;

static ejsval
_ejs_RegExp_impl (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSRegExp *re;

    if (EJSVAL_IS_UNDEFINED(_this)) {
        // called as a function
        _this = _ejs_object_new(_ejs_RegExp_proto, &_ejs_regexp_specops);
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

    const unsigned char* pcre16_tables = pcre16_maketables();
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
            _ejs_throw_nativeerror_utf8 (EJS_SYNTAX_ERROR, "Invalid flag supplied to RegExp constructor");
        }
    }

    return _this;
}

// ECMA262: 15.10.6.2
static ejsval
_ejs_RegExp_prototype_exec (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJS_NOT_IMPLEMENTED();
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
_ejs_RegExp_prototype_get_source (ejsval env, ejsval _this, uint32_t argc, ejsval *args)
{
    EJSRegExp* re = (EJSRegExp*)EJSVAL_TO_OBJECT(_this);
    return re->pattern;
}


void
_ejs_regexp_init(ejsval global)
{
    _ejs_RegExp = _ejs_function_new_without_proto (_ejs_null, _ejs_atom_RegExp, (EJSClosureFunc)_ejs_RegExp_impl);
    _ejs_object_setprop (global, _ejs_atom_RegExp, _ejs_RegExp);

    _ejs_gc_add_root (&_ejs_RegExp_proto);
    _ejs_RegExp_proto = _ejs_object_new(_ejs_null, &_ejs_object_specops);
    _ejs_object_setprop (_ejs_RegExp,       _ejs_atom_prototype,  _ejs_RegExp_proto);

#define OBJ_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_RegExp, x, _ejs_RegExp_##x)
#define PROTO_METHOD(x) EJS_INSTALL_ATOM_FUNCTION(_ejs_RegExp_proto, x, _ejs_RegExp_prototype_##x)
#define PROTO_GETTER(x) EJS_INSTALL_ATOM_GETTER(_ejs_RegExp_proto, x, _ejs_RegExp_prototype_get_##x)

    PROTO_METHOD(exec);
    PROTO_METHOD(test);
    PROTO_METHOD(toString);

    PROTO_GETTER(global);
    PROTO_GETTER(ignoreCase);
    PROTO_GETTER(lastIndex);
    PROTO_GETTER(multiline);
    PROTO_GETTER(source);

#undef OBJ_METHOD
#undef PROTO_METHOD
}


static ejsval
_ejs_regexp_specop_get (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get (obj, propertyName);
}

static EJSPropertyDesc*
_ejs_regexp_specop_get_own_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_own_property (obj, propertyName);
}

static EJSPropertyDesc*
_ejs_regexp_specop_get_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.get_property (obj, propertyName);
}

static void
_ejs_regexp_specop_put (ejsval obj, ejsval propertyName, ejsval val, EJSBool flag)
{
    _ejs_object_specops.put (obj, propertyName, val, flag);
}

static EJSBool
_ejs_regexp_specop_can_put (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.can_put (obj, propertyName);
}

static EJSBool
_ejs_regexp_specop_has_property (ejsval obj, ejsval propertyName)
{
    return _ejs_object_specops.has_property (obj, propertyName);
}

static EJSBool
_ejs_regexp_specop_delete (ejsval obj, ejsval propertyName, EJSBool flag)
{
    return _ejs_object_specops._delete (obj, propertyName, flag);
}

static ejsval
_ejs_regexp_specop_default_value (ejsval obj, const char *hint)
{
    return _ejs_object_specops.default_value (obj, hint);
}

static EJSBool
_ejs_regexp_specop_define_own_property (ejsval obj, ejsval propertyName, EJSPropertyDesc* propertyDescriptor, EJSBool flag)
{
    return _ejs_object_specops.define_own_property (obj, propertyName, propertyDescriptor, flag);
}

static EJSObject*
_ejs_regexp_specop_allocate()
{
    return (EJSObject*)_ejs_gc_new(EJSRegExp);
}


static void
_ejs_regexp_specop_finalize (EJSObject* obj)
{
    _ejs_object_specops.finalize (obj);
}

static void
_ejs_regexp_specop_scan (EJSObject* obj, EJSValueFunc scan_func)
{
    EJSRegExp *re = (EJSRegExp*)obj;
    scan_func (re->pattern);
    scan_func (re->flags);

    _ejs_object_specops.scan (obj, scan_func);
}
