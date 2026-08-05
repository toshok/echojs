/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * The \p{...} / \P{...} pattern translator.  PCRE executes the
 * patterns but knows nothing about Unicode properties (the vendored
 * build has no UCP, and its property model predates ECMAScript's
 * anyway) — so property escapes are expanded here, before compilation,
 * into explicit code-point classes from the generated tables
 * (ejs-unicode-tables.h, Unicode 17).  Engine-neutral: any future
 * matcher swap keeps this layer.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ejs-value.h"
#include "ejs-regexp.h"
#include "ejs-unicode-tables.h"

/* ---------- growable output buffer ---------- */
typedef struct {
    jschar  *data;
    uint32_t len;
    uint32_t cap;
} PatBuf;

static EJSBool buf_reserve(PatBuf *b, uint32_t extra) {
    if (b->len + extra <= b->cap) return EJS_TRUE;
    uint32_t ncap = b->cap ? b->cap : 256;
    while (ncap < b->len + extra) ncap *= 2;
    jschar *nd = (jschar*)realloc(b->data, ncap * sizeof(jschar));
    if (!nd) return EJS_FALSE;
    b->data = nd;
    b->cap = ncap;
    return EJS_TRUE;
}

static EJSBool buf_putc(PatBuf *b, jschar c) {
    if (!buf_reserve(b, 1)) return EJS_FALSE;
    b->data[b->len++] = c;
    return EJS_TRUE;
}

static EJSBool buf_puts(PatBuf *b, const char *s) {
    uint32_t n = (uint32_t)strlen(s);
    if (!buf_reserve(b, n)) return EJS_FALSE;
    for (uint32_t i = 0; i < n; i++) b->data[b->len++] = (jschar)s[i];
    return EJS_TRUE;
}

/* ---------- table lookup ---------- */
static const EJSUnicodeProperty*
lookup (const EJSUnicodeProperty *table, uint32_t count, const char *name)
{
    uint32_t lo = 0, hi = count;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        int cmp = strcmp(name, table[mid].name);
        if (cmp == 0) return &table[mid];
        if (cmp < 0) hi = mid; else lo = mid + 1;
    }
    return NULL;
}

/* ---------- range emission ---------- */
/* PCRE_JAVASCRIPT_COMPAT enforces JS escape syntax, so \x{...} is not
 * available.  BMP code points are emitted as \uXXXX; astral ones as
 * raw surrogate-pair code units in the (UTF-16) pattern buffer, which
 * PCRE_UTF16 reads back as single code points — range endpoints
 * included. */
static EJSBool emit_cp (PatBuf *out, uint32_t cp) {
    if (cp <= 0xFFFF) {
        char tmp[8];
        snprintf(tmp, sizeof(tmp), "\\u%04X", cp);
        return buf_puts(out, tmp);
    }
    cp -= 0x10000;
    return buf_putc(out, (jschar)(0xD800 + (cp >> 10))) &&
           buf_putc(out, (jschar)(0xDC00 + (cp & 0x3FF)));
}

static EJSBool emit_range (PatBuf *out, uint32_t start, uint32_t end) {
    if (!emit_cp(out, start)) return EJS_FALSE;
    if (start == end) return EJS_TRUE;
    return buf_putc(out, '-') && emit_cp(out, end);
}

static EJSBool emit_ranges (PatBuf *out, const EJSUnicodeRange *ranges, uint32_t n) {
    for (uint32_t i = 0; i < n; i++)
        if (!emit_range(out, ranges[i].start, ranges[i].end)) return EJS_FALSE;
    return EJS_TRUE;
}

/* the complement over the full code space, emitted without being
 * materialized: the gaps between consecutive ranges */
static EJSBool emit_complement (PatBuf *out, const EJSUnicodeRange *ranges, uint32_t n) {
    uint32_t prev = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (ranges[i].start > prev)
            if (!emit_range(out, prev, ranges[i].start - 1)) return EJS_FALSE;
        prev = ranges[i].end + 1;
    }
    if (prev <= 0x10FFFF)
        if (!emit_range(out, prev, 0x10FFFF)) return EJS_FALSE;
    return EJS_TRUE;
}

/* ---------- \p{...} parsing ---------- */
/* UnicodePropertyValueExpression:
 *   LoneUnicodePropertyNameOrValue          (binary property or a
 *                                            General_Category value)
 *   UnicodePropertyName = UnicodePropertyValue
 *                                           (gc/sc/scx only)
 * Names are matched exactly — ECMAScript has no loose matching. */
static const EJSUnicodeProperty*
resolve_property (const char *name, const char *value /* NULL for lone */)
{
    if (!value)
        return lookup(_ejs_unicode_lone_properties, _ejs_unicode_lone_properties_count, name);
    if (!strcmp(name, "General_Category") || !strcmp(name, "gc"))
        return lookup(_ejs_unicode_gc_values, _ejs_unicode_gc_values_count, value);
    if (!strcmp(name, "Script") || !strcmp(name, "sc"))
        return lookup(_ejs_unicode_sc_values, _ejs_unicode_sc_values_count, value);
    if (!strcmp(name, "Script_Extensions") || !strcmp(name, "scx"))
        return lookup(_ejs_unicode_scx_values, _ejs_unicode_scx_values_count, value);
    return NULL;
}

/* parses the {Name} or {Name=Value} after \p or \P; advances *ip past
 * the closing brace.  Returns the property, or NULL (bad syntax /
 * unknown name). */
static const EJSUnicodeProperty*
parse_property (const jschar *chars, uint32_t len, uint32_t *ip)
{
    uint32_t i = *ip;
    if (i >= len || chars[i] != '{') return NULL;
    i++;

    char name[64];
    char *value = NULL;
    uint32_t n = 0;
    while (i < len && chars[i] != '}') {
        jschar c = chars[i];
        EJSBool word = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                       (c >= '0' && c <= '9') || c == '_';
        if (!word && !(c == '=' && !value)) return NULL;
        if (n + 1 >= sizeof(name)) return NULL;
        if (c == '=') { name[n++] = 0; value = &name[n]; }
        else name[n++] = (char)c;
        i++;
    }
    if (i >= len || n == 0) return NULL;         /* unterminated or empty */
    if (value && !*value) return NULL;           /* "Name=" with no value */
    name[n] = 0;
    i++; /* consume '}' */

    const EJSUnicodeProperty *prop = resolve_property(name, value);
    if (!prop) return NULL;
    *ip = i;
    return prop;
}

/* ---------- the translator ---------- */
/* Expands \p{...}/\P{...} under /u; outside /u they are Annex B
 * identity escapes (matching the literal 'p'/'P'), which PCRE's
 * UCP-less build would otherwise reject.  Everything else is copied
 * verbatim.  Returns a malloc'd NUL-terminated buffer the caller
 * frees, or NULL with *err set. */
jschar*
_ejs_regexp_translate_pattern (const jschar *chars, uint32_t len, EJSBool unicode,
                               uint32_t *out_len, const char **err)
{
    PatBuf out = { NULL, 0, 0 };
    EJSBool in_class = EJS_FALSE;
    *err = NULL;

    for (uint32_t i = 0; i < len; ) {
        jschar c = chars[i];
        if (c != '\\') {
            if (c == '[' && !in_class) in_class = EJS_TRUE;
            else if (c == ']' && in_class) in_class = EJS_FALSE;
            if (!buf_putc(&out, c)) goto oom;
            i++;
            continue;
        }
        /* an escape: chars[i] == '\\' */
        if (i + 1 >= len) { /* trailing backslash: let pcre reject it */
            if (!buf_putc(&out, c)) goto oom;
            i++;
            continue;
        }
        jschar esc = chars[i + 1];
        if (esc != 'p' && esc != 'P') {
            /* any other escape passes through untouched (including \[,
             * \], which must not toggle class state) */
            if (!buf_putc(&out, '\\') || !buf_putc(&out, esc)) goto oom;
            i += 2;
            continue;
        }

        if (!unicode) {
            /* Annex B: IdentityEscape — \p matches "p" */
            if (!buf_putc(&out, esc)) goto oom;
            i += 2;
            continue;
        }

        EJSBool negated = (esc == 'P');
        uint32_t j = i + 2;
        const EJSUnicodeProperty *prop = parse_property(chars, len, &j);
        if (!prop) {
            *err = "invalid property name in \\p{...}";
            free(out.data);
            return NULL;
        }

        EJSBool ok;
        if (in_class) {
            /* splice bare into the surrounding class */
            ok = negated ? emit_complement(&out, prop->ranges, prop->nranges)
                         : emit_ranges(&out, prop->ranges, prop->nranges);
        } else {
            ok = buf_putc(&out, '[') &&
                 (negated ? emit_complement(&out, prop->ranges, prop->nranges)
                          : emit_ranges(&out, prop->ranges, prop->nranges)) &&
                 buf_putc(&out, ']');
        }
        if (!ok) goto oom;
        i = j;
    }

    if (!buf_putc(&out, 0)) goto oom;
    *out_len = out.len - 1;
    return out.data;

 oom:
    free(out.data);
    *err = "out of memory translating pattern";
    return NULL;
}
