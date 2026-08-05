/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * The \p{...} / \P{...} and v-mode class-set pattern translator.  PCRE
 * executes the patterns but knows nothing about Unicode properties or
 * ClassSetExpressions (the vendored build has no UCP, and its property
 * model predates ECMAScript's anyway) — so property escapes expand
 * here, before compilation, into explicit code-point classes from the
 * generated tables (ejs-unicode-tables.h, Unicode 17), and v-mode
 * classes are parsed outright: the set algebra (union, "--"
 * difference, "&&" intersection, nested classes, \q{...} and
 * properties-of-strings) is evaluated at translation time and emitted
 * as plain alternations and range classes.  Engine-neutral: any future
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

/* ---------- range-set algebra ---------- */
/* Sorted, non-overlapping, non-adjacent (start,end) pairs — the same
 * invariant the generated tables hold, so table ranges splice in
 * directly. */
typedef struct {
    EJSUnicodeRange *r;
    uint32_t n, cap;
} RangeSet;

static EJSBool rs_push (RangeSet *s, uint32_t start, uint32_t end) {
    if (s->n == s->cap) {
        uint32_t ncap = s->cap ? s->cap * 2 : 16;
        EJSUnicodeRange *nr = (EJSUnicodeRange*)realloc(s->r, ncap * sizeof(EJSUnicodeRange));
        if (!nr) return EJS_FALSE;
        s->r = nr;
        s->cap = ncap;
    }
    s->r[s->n].start = start;
    s->r[s->n].end = end;
    s->n++;
    return EJS_TRUE;
}

static int rs_cmp (const void *a, const void *b) {
    uint32_t sa = ((const EJSUnicodeRange*)a)->start, sb = ((const EJSUnicodeRange*)b)->start;
    return sa < sb ? -1 : sa > sb ? 1 : 0;
}

/* restore the invariant after arbitrary appends */
static void rs_normalize (RangeSet *s) {
    if (s->n < 2) return;
    qsort(s->r, s->n, sizeof(EJSUnicodeRange), rs_cmp);
    uint32_t w = 0;
    for (uint32_t i = 1; i < s->n; i++) {
        if (s->r[i].start <= s->r[w].end + 1) {
            if (s->r[i].end > s->r[w].end) s->r[w].end = s->r[i].end;
        } else {
            s->r[++w] = s->r[i];
        }
    }
    s->n = w + 1;
}

static EJSBool rs_add_ranges (RangeSet *s, const EJSUnicodeRange *ranges, uint32_t n) {
    for (uint32_t i = 0; i < n; i++)
        if (!rs_push(s, ranges[i].start, ranges[i].end)) return EJS_FALSE;
    return EJS_TRUE;
}

/* a := a ∩ b, both normalized */
static EJSBool rs_intersect (RangeSet *a, const RangeSet *b) {
    RangeSet out = { NULL, 0, 0 };
    uint32_t i = 0, j = 0;
    while (i < a->n && j < b->n) {
        uint32_t lo = a->r[i].start > b->r[j].start ? a->r[i].start : b->r[j].start;
        uint32_t hi = a->r[i].end < b->r[j].end ? a->r[i].end : b->r[j].end;
        if (lo <= hi && !rs_push(&out, lo, hi)) { free(out.r); return EJS_FALSE; }
        if (a->r[i].end < b->r[j].end) i++; else j++;
    }
    free(a->r);
    *a = out;
    return EJS_TRUE;
}

/* a := a \ b, both normalized */
static EJSBool rs_subtract (RangeSet *a, const RangeSet *b) {
    RangeSet out = { NULL, 0, 0 };
    uint32_t j = 0;
    for (uint32_t i = 0; i < a->n; i++) {
        uint32_t lo = a->r[i].start, hi = a->r[i].end;
        while (j > 0 && b->r[j-1].end >= lo) j--;  /* ranges can re-overlap the cursor */
        for (uint32_t k = j; k < b->n && b->r[k].start <= hi; k++) {
            if (b->r[k].end < lo) { j = k + 1; continue; }
            if (b->r[k].start > lo)
                if (!rs_push(&out, lo, b->r[k].start - 1)) { free(out.r); return EJS_FALSE; }
            if (b->r[k].end >= hi) { lo = hi + 1; break; }
            lo = b->r[k].end + 1;
        }
        if (lo <= hi && !rs_push(&out, lo, hi)) { free(out.r); return EJS_FALSE; }
    }
    free(a->r);
    *a = out;
    return EJS_TRUE;
}

/* a := [0,10FFFF] \ a, normalized */
static EJSBool rs_complement (RangeSet *a) {
    RangeSet out = { NULL, 0, 0 };
    uint32_t prev = 0;
    for (uint32_t i = 0; i < a->n; i++) {
        if (a->r[i].start > prev)
            if (!rs_push(&out, prev, a->r[i].start - 1)) { free(out.r); return EJS_FALSE; }
        prev = a->r[i].end + 1;
    }
    if (prev <= 0x10FFFF)
        if (!rs_push(&out, prev, 0x10FFFF)) { free(out.r); return EJS_FALSE; }
    free(a->r);
    *a = out;
    return EJS_TRUE;
}

/* ---------- string sets (v mode) ---------- */
/* Multi-code-point class members: \q{...} alternatives and the
 * properties-of-strings.  Owned copies, [len]+units. */
typedef struct {
    jschar **items;   /* items[i][0] = length, units follow */
    uint32_t n, cap;
    EJSBool has_empty; /* the empty string is a legal member */
} StrSet;

typedef struct {
    RangeSet ranges;
    StrSet strings;
} ClassSet;

static void cs_free (ClassSet *cs) {
    free(cs->ranges.r);
    for (uint32_t i = 0; i < cs->strings.n; i++) free(cs->strings.items[i]);
    free(cs->strings.items);
    memset(cs, 0, sizeof(*cs));
}

static EJSBool ss_push_units (StrSet *s, const jschar *units, uint32_t len) {
    /* dedupe by value — unions repeat members freely */
    for (uint32_t i = 0; i < s->n; i++) {
        if (s->items[i][0] == (jschar)len &&
            memcmp(s->items[i] + 1, units, len * sizeof(jschar)) == 0)
            return EJS_TRUE;
    }
    if (s->n == s->cap) {
        uint32_t ncap = s->cap ? s->cap * 2 : 8;
        jschar **ni = (jschar**)realloc(s->items, ncap * sizeof(jschar*));
        if (!ni) return EJS_FALSE;
        s->items = ni;
        s->cap = ncap;
    }
    jschar *copy = (jschar*)malloc((len + 1) * sizeof(jschar));
    if (!copy) return EJS_FALSE;
    copy[0] = (jschar)len;
    memcpy(copy + 1, units, len * sizeof(jschar));
    s->items[s->n++] = copy;
    return EJS_TRUE;
}

static EJSBool ss_contains (const StrSet *s, const jschar *item) {
    for (uint32_t i = 0; i < s->n; i++) {
        if (s->items[i][0] == item[0] &&
            memcmp(s->items[i] + 1, item + 1, item[0] * sizeof(jschar)) == 0)
            return EJS_TRUE;
    }
    return EJS_FALSE;
}

/* keep only members satisfying (in b) == keep_in */
static void ss_filter (StrSet *a, const StrSet *b, EJSBool keep_in, EJSBool b_has_empty) {
    uint32_t w = 0;
    for (uint32_t i = 0; i < a->n; i++) {
        if (ss_contains(b, a->items[i]) == keep_in) a->items[w++] = a->items[i];
        else free(a->items[i]);
    }
    a->n = w;
    if (a->has_empty && (b_has_empty != keep_in)) a->has_empty = EJS_FALSE;
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

static const EJSUnicodeStringProperty*
lookup_string_property (const char *name)
{
    uint32_t lo = 0, hi = _ejs_unicode_string_properties_count;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        int cmp = strcmp(name, _ejs_unicode_string_properties[mid].name);
        if (cmp == 0) return &_ejs_unicode_string_properties[mid];
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

/* a ClassSet, emitted standalone: strings longest-first (the order the
 * spec's alternation semantics require), then the range class */
static int ss_len_desc (const void *pa, const void *pb) {
    const jschar *a = *(jschar* const*)pa, *b = *(jschar* const*)pb;
    return (int)b[0] - (int)a[0];
}

static EJSBool emit_class_set (PatBuf *out, ClassSet *cs) {
    EJSBool wrap = cs->strings.n > 0 || cs->strings.has_empty;
    if (wrap) {
        if (!buf_puts(out, "(?:")) return EJS_FALSE;
        qsort(cs->strings.items, cs->strings.n, sizeof(jschar*), ss_len_desc);
        for (uint32_t i = 0; i < cs->strings.n; i++) {
            if (i && !buf_putc(out, '|')) return EJS_FALSE;
            const jschar *item = cs->strings.items[i];
            for (uint32_t u = 1; u <= item[0]; u++) {
                jschar c = item[u];
                /* surrogate pairs pass through raw (UTF-16 mode reads
                 * them as one code point); BMP units get escaped */
                if (c >= 0xD800 && c <= 0xDFFF) {
                    if (!buf_putc(out, c)) return EJS_FALSE;
                } else {
                    if (!emit_cp(out, c)) return EJS_FALSE;
                }
            }
        }
        if (cs->ranges.n) {
            if (cs->strings.n && !buf_putc(out, '|')) return EJS_FALSE;
            if (!buf_putc(out, '[') || !emit_ranges(out, cs->ranges.r, cs->ranges.n) || !buf_putc(out, ']'))
                return EJS_FALSE;
        }
        /* the empty string is a real member: an empty final alternative */
        if (cs->strings.has_empty && !buf_putc(out, '|')) return EJS_FALSE;
        return buf_puts(out, ")");
    }
    /* ranges only — "[]" never matches, which is what the empty set means */
    return buf_putc(out, '[') && emit_ranges(out, cs->ranges.r, cs->ranges.n) && buf_putc(out, ']');
}

/* ---------- \p{...} parsing ---------- */
/* UnicodePropertyValueExpression:
 *   LoneUnicodePropertyNameOrValue          (binary property or a
 *                                            General_Category value;
 *                                            under v also a property
 *                                            of strings)
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
 * the closing brace.  Fills exactly one of *prop / *sprop (the latter
 * only when strings_ok).  Returns EJS_FALSE on bad syntax or an
 * unknown name. */
static EJSBool
parse_property (const jschar *chars, uint32_t len, uint32_t *ip,
                EJSBool strings_ok,
                const EJSUnicodeProperty **prop,
                const EJSUnicodeStringProperty **sprop)
{
    uint32_t i = *ip;
    *prop = NULL;
    *sprop = NULL;
    if (i >= len || chars[i] != '{') return EJS_FALSE;
    i++;

    char name[64];
    char *value = NULL;
    uint32_t n = 0;
    while (i < len && chars[i] != '}') {
        jschar c = chars[i];
        EJSBool word = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                       (c >= '0' && c <= '9') || c == '_';
        if (!word && !(c == '=' && !value)) return EJS_FALSE;
        if (n + 1 >= sizeof(name)) return EJS_FALSE;
        if (c == '=') { name[n++] = 0; value = &name[n]; }
        else name[n++] = (char)c;
        i++;
    }
    if (i >= len || n == 0) return EJS_FALSE;         /* unterminated or empty */
    if (value && !*value) return EJS_FALSE;           /* "Name=" with no value */
    name[n] = 0;
    i++; /* consume '}' */

    *prop = resolve_property(name, value);
    if (!*prop && !value && strings_ok)
        *sprop = lookup_string_property(name);
    if (!*prop && !*sprop) return EJS_FALSE;
    *ip = i;
    return EJS_TRUE;
}

/* ---------- v-mode class-set parsing ---------- */

/* JS WhiteSpace + LineTerminator, the \s set */
static const EJSUnicodeRange WS_RANGES[] = {
    {0x9,0xD},{0x20,0x20},{0xA0,0xA0},{0x1680,0x1680},{0x2000,0x200A},
    {0x2028,0x2029},{0x202F,0x202F},{0x205F,0x205F},{0x3000,0x3000},{0xFEFF,0xFEFF},
};
static const EJSUnicodeRange DIGIT_RANGES[] = { {'0','9'} };
static const EJSUnicodeRange WORD_RANGES[] = { {'0','9'},{'A','Z'},{'_','_'},{'a','z'} };

typedef struct {
    const jschar *chars;
    uint32_t len;
    uint32_t i;
    const char *err;
} VParse;

static EJSBool v_class_set (VParse *vp, ClassSet *out); /* fwd: nested classes */

/* one escaped or literal code point; -1 with err set on failure, -2 if
 * the escape is a SET (\d, \p, ...) the caller must handle */
static int64_t v_char (VParse *vp) {
    const jschar *c = vp->chars;
    uint32_t i = vp->i, len = vp->len;
    jschar ch = c[i];

    if (ch != '\\') {
        /* ClassSetSyntaxCharacters can't appear bare */
        if (ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '/' || ch == '|' ||
            ch == '[' || ch == ']' || ch == '-') {
            vp->err = "unescaped syntax character in v-mode class";
            return -1;
        }
        /* raw surrogate pair in the source = one code point */
        if (ch >= 0xD800 && ch < 0xDC00 && i + 1 < len && c[i+1] >= 0xDC00 && c[i+1] <= 0xDFFF) {
            vp->i += 2;
            return 0x10000 + (((uint32_t)(ch - 0xD800) << 10) | (c[i+1] - 0xDC00));
        }
        vp->i++;
        return ch;
    }

    if (i + 1 >= len) { vp->err = "trailing backslash in class"; return -1; }
    jschar esc = c[i + 1];
    switch (esc) {
        case 'd': case 'D': case 's': case 'S': case 'w': case 'W':
        case 'p': case 'P': case 'q':
            return -2;
        case 'n': vp->i += 2; return '\n';
        case 'r': vp->i += 2; return '\r';
        case 't': vp->i += 2; return '\t';
        case 'f': vp->i += 2; return '\f';
        case 'v': vp->i += 2; return '\v';
        case 'b': vp->i += 2; return 0x8; /* backspace inside classes */
        case '0': vp->i += 2; return 0;
        case 'c': {
            if (i + 2 < len) {
                jschar l = c[i + 2];
                if ((l >= 'A' && l <= 'Z') || (l >= 'a' && l <= 'z')) {
                    vp->i += 3;
                    return l % 32;
                }
            }
            vp->err = "invalid \\c escape in class";
            return -1;
        }
        case 'x': {
            uint32_t v = 0, j = i + 2;
            for (int k = 0; k < 2; k++, j++) {
                if (j >= len) { vp->err = "invalid \\x escape"; return -1; }
                jschar h = c[j];
                if (h >= '0' && h <= '9') v = v * 16 + (h - '0');
                else if (h >= 'a' && h <= 'f') v = v * 16 + (h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') v = v * 16 + (h - 'A' + 10);
                else { vp->err = "invalid \\x escape"; return -1; }
            }
            vp->i = j;
            return v;
        }
        case 'u': {
            uint32_t j = i + 2;
            if (j < len && c[j] == '{') {
                uint32_t v = 0;
                j++;
                uint32_t start = j;
                while (j < len && c[j] != '}') {
                    jschar h = c[j];
                    uint32_t d;
                    if (h >= '0' && h <= '9') d = h - '0';
                    else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
                    else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
                    else { vp->err = "invalid \\u{...} escape"; return -1; }
                    v = v * 16 + d;
                    if (v > 0x10FFFF) { vp->err = "\\u{...} out of range"; return -1; }
                    j++;
                }
                if (j >= len || j == start) { vp->err = "invalid \\u{...} escape"; return -1; }
                vp->i = j + 1;
                return v;
            }
            uint32_t v = 0;
            for (int k = 0; k < 4; k++, j++) {
                if (j >= len) { vp->err = "invalid \\u escape"; return -1; }
                jschar h = c[j];
                if (h >= '0' && h <= '9') v = v * 16 + (h - '0');
                else if (h >= 'a' && h <= 'f') v = v * 16 + (h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') v = v * 16 + (h - 'A' + 10);
                else { vp->err = "invalid \\u escape"; return -1; }
            }
            vp->i = j;
            /* a \uXXXX high surrogate followed by a \uXXXX low one is
             * one code point, same as the raw pair */
            if (v >= 0xD800 && v < 0xDC00 && vp->i + 6 <= len &&
                c[vp->i] == '\\' && c[vp->i+1] == 'u') {
                uint32_t lo = 0, m = vp->i + 2;
                EJSBool ok = EJS_TRUE;
                for (int k = 0; k < 4 && ok; k++, m++) {
                    jschar h = m < len ? c[m] : 0;
                    if (h >= '0' && h <= '9') lo = lo * 16 + (h - '0');
                    else if (h >= 'a' && h <= 'f') lo = lo * 16 + (h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') lo = lo * 16 + (h - 'A' + 10);
                    else ok = EJS_FALSE;
                }
                if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
                    vp->i = m;
                    return 0x10000 + (((v - 0xD800) << 10) | (lo - 0xDC00));
                }
            }
            return v;
        }
        default:
            /* identity escape: any punctuator, including the class-set
             * syntax and reserved ones */
            if ((esc >= 'A' && esc <= 'Z') || (esc >= 'a' && esc <= 'z') ||
                (esc >= '0' && esc <= '9')) {
                vp->err = "invalid escape in v-mode class";
                return -1;
            }
            vp->i += 2;
            return esc;
    }
}

/* a set-valued escape (\d \D \s \S \w \W \p{..} \P{..}) into `out`;
 * expects vp->i at the backslash */
static EJSBool v_class_escape_set (VParse *vp, ClassSet *out) {
    jschar esc = vp->chars[vp->i + 1];
    vp->i += 2;
    const EJSUnicodeRange *base = NULL;
    uint32_t nbase = 0;
    EJSBool negate = EJS_FALSE;
    switch (esc) {
        case 'D': negate = EJS_TRUE; /* fallthrough */
        case 'd': base = DIGIT_RANGES; nbase = 1; break;
        case 'S': negate = EJS_TRUE; /* fallthrough */
        case 's': base = WS_RANGES; nbase = sizeof(WS_RANGES)/sizeof(WS_RANGES[0]); break;
        case 'W': negate = EJS_TRUE; /* fallthrough */
        case 'w': base = WORD_RANGES; nbase = sizeof(WORD_RANGES)/sizeof(WORD_RANGES[0]); break;
        case 'P': negate = EJS_TRUE; /* fallthrough */
        case 'p': {
            const EJSUnicodeProperty *prop;
            const EJSUnicodeStringProperty *sprop;
            if (!parse_property(vp->chars, vp->len, &vp->i, !negate, &prop, &sprop)) {
                vp->err = "invalid property name in \\p{...}";
                return EJS_FALSE;
            }
            if (sprop) {
                /* a property of strings: ranges hold its single-code-point
                 * members, the string list its sequences */
                if (!rs_add_ranges(&out->ranges, sprop->ranges, sprop->nranges)) goto oom;
                {
                    const uint16_t *p = sprop->strings;
                    for (uint32_t k = 0; k < sprop->nstrings; k++) {
                        uint32_t slen = *p++;
                        jschar tmp[32];
                        for (uint32_t u = 0; u < slen; u++) tmp[u] = (jschar)p[u];
                        if (!ss_push_units(&out->strings, tmp, slen)) goto oom;
                        p += slen;
                    }
                }
                rs_normalize(&out->ranges);
                return EJS_TRUE;
            }
            base = prop->ranges;
            nbase = prop->nranges;
            break;
        }
        default:
            vp->err = "unexpected class escape";
            return EJS_FALSE;
    }
    if (!rs_add_ranges(&out->ranges, base, nbase)) goto oom;
    rs_normalize(&out->ranges);
    if (negate && !rs_complement(&out->ranges)) goto oom;
    return EJS_TRUE;
 oom:
    vp->err = "out of memory";
    return EJS_FALSE;
}

/* \q{alt|alt|...} — string literals as class members */
static EJSBool v_q_strings (VParse *vp, ClassSet *out) {
    vp->i += 2; /* past \q */
    if (vp->i >= vp->len || vp->chars[vp->i] != '{') {
        vp->err = "\\q must be followed by {...}";
        return EJS_FALSE;
    }
    vp->i++;
    jschar buf[64];
    uint32_t blen = 0;
    for (;;) {
        if (vp->i >= vp->len) { vp->err = "unterminated \\q{...}"; return EJS_FALSE; }
        jschar ch = vp->chars[vp->i];
        if (ch == '}' || ch == '|') {
            if (blen == 0) out->strings.has_empty = EJS_TRUE;
            else if (blen == 1 || (blen == 2 && buf[0] >= 0xD800 && buf[0] < 0xDC00)) {
                /* single code point: an ordinary class member */
                uint32_t cp = blen == 1 ? buf[0]
                    : 0x10000 + (((uint32_t)(buf[0] - 0xD800) << 10) | (buf[1] - 0xDC00));
                if (!rs_push(&out->ranges, cp, cp)) { vp->err = "out of memory"; return EJS_FALSE; }
            } else {
                if (!ss_push_units(&out->strings, buf, blen)) { vp->err = "out of memory"; return EJS_FALSE; }
            }
            blen = 0;
            vp->i++;
            if (ch == '}') break;
            continue;
        }
        int64_t cp = v_char(vp);
        if (cp == -1) return EJS_FALSE;
        if (cp == -2) { vp->err = "class escapes are not allowed in \\q{...}"; return EJS_FALSE; }
        if (blen + 2 > sizeof(buf)/sizeof(buf[0])) { vp->err = "\\q{...} alternative too long"; return EJS_FALSE; }
        if (cp > 0xFFFF) {
            uint32_t v = (uint32_t)cp - 0x10000;
            buf[blen++] = (jschar)(0xD800 + (v >> 10));
            buf[blen++] = (jschar)(0xDC00 + (v & 0x3FF));
        } else {
            buf[blen++] = (jschar)cp;
        }
    }
    rs_normalize(&out->ranges);
    return EJS_TRUE;
}

/* one ClassSetOperand into `out` */
static EJSBool v_operand (VParse *vp, ClassSet *out) {
    jschar ch = vp->chars[vp->i];
    if (ch == '[') {
        vp->i++;
        return v_class_set(vp, out);
    }
    if (ch == '\\' && vp->i + 1 < vp->len) {
        jschar esc = vp->chars[vp->i + 1];
        if (esc == 'q') return v_q_strings(vp, out);
        if (esc == 'd' || esc == 'D' || esc == 's' || esc == 'S' ||
            esc == 'w' || esc == 'W' || esc == 'p' || esc == 'P')
            return v_class_escape_set(vp, out);
    }
    int64_t cp = v_char(vp);
    if (cp < 0) {
        if (cp == -2) vp->err = "unexpected escape";
        return EJS_FALSE;
    }
    if (!rs_push(&out->ranges, (uint32_t)cp, (uint32_t)cp)) { vp->err = "out of memory"; return EJS_FALSE; }
    return EJS_TRUE;
}

static EJSBool v_at (VParse *vp, jschar a) {
    return vp->i < vp->len && vp->chars[vp->i] == a;
}
static EJSBool v_at2 (VParse *vp, jschar a, jschar b) {
    return vp->i + 1 < vp->len && vp->chars[vp->i] == a && vp->chars[vp->i+1] == b;
}

/* ClassSetExpression, from just past '[' through the matching ']'.
 * Union is juxtaposition (with a-b ranges); "--" difference and "&&"
 * intersection are uniform chains — mixing them without brackets is a
 * SyntaxError, as is a chain operand that is a range. */
static EJSBool v_class_set (VParse *vp, ClassSet *out) {
    EJSBool negated = EJS_FALSE;
    memset(out, 0, sizeof(*out));
    if (v_at(vp, '^')) { negated = EJS_TRUE; vp->i++; }

    enum { OP_NONE, OP_UNION, OP_DIFF, OP_INTER } op = OP_NONE;
    EJSBool first = EJS_TRUE;

    while (!v_at(vp, ']')) {
        if (vp->i >= vp->len) { vp->err = "unterminated character class"; goto fail; }

        if (!first) {
            if (v_at2(vp, '-', '-')) {
                if (op == OP_NONE) op = OP_DIFF;
                else if (op != OP_DIFF) { vp->err = "mixed set operators need brackets"; goto fail; }
                vp->i += 2;
            } else if (v_at2(vp, '&', '&')) {
                if (op == OP_NONE) op = OP_INTER;
                else if (op != OP_INTER) { vp->err = "mixed set operators need brackets"; goto fail; }
                vp->i += 2;
            } else {
                if (op == OP_NONE) op = OP_UNION;
                else if (op != OP_UNION) { vp->err = "mixed set operators need brackets"; goto fail; }
            }
            if (v_at(vp, ']')) { vp->err = "trailing set operator"; goto fail; }
        }

        ClassSet operand;
        memset(&operand, 0, sizeof(operand));
        if (!v_operand(vp, &operand)) { cs_free(&operand); goto fail; }

        /* a-b range: only as a union member, only between single
         * characters (the operand so far is one code point, no strings) */
        if ((op == OP_NONE || op == OP_UNION) &&
            v_at(vp, '-') && !v_at2(vp, '-', '-')) {
            if (operand.ranges.n != 1 || operand.ranges.r[0].start != operand.ranges.r[0].end ||
                operand.strings.n || operand.strings.has_empty) {
                vp->err = "invalid range in class";
                cs_free(&operand);
                goto fail;
            }
            vp->i++;
            ClassSet hi_op;
            memset(&hi_op, 0, sizeof(hi_op));
            if (!v_operand(vp, &hi_op)) { cs_free(&hi_op); cs_free(&operand); goto fail; }
            if (hi_op.ranges.n != 1 || hi_op.ranges.r[0].start != hi_op.ranges.r[0].end ||
                hi_op.strings.n || hi_op.strings.has_empty ||
                hi_op.ranges.r[0].start < operand.ranges.r[0].start) {
                vp->err = "invalid range in class";
                cs_free(&hi_op);
                cs_free(&operand);
                goto fail;
            }
            operand.ranges.r[0].end = hi_op.ranges.r[0].start;
            cs_free(&hi_op);
            if (op == OP_NONE) op = OP_UNION;
        }

        if (first) {
            *out = operand; /* move */
            first = EJS_FALSE;
            continue;
        }

        switch (op) {
            case OP_UNION:
                if (!rs_add_ranges(&out->ranges, operand.ranges.r, operand.ranges.n)) { vp->err = "out of memory"; cs_free(&operand); goto fail; }
                rs_normalize(&out->ranges);
                for (uint32_t i = 0; i < operand.strings.n; i++)
                    if (!ss_push_units(&out->strings, operand.strings.items[i] + 1, operand.strings.items[i][0])) { vp->err = "out of memory"; cs_free(&operand); goto fail; }
                if (operand.strings.has_empty) out->strings.has_empty = EJS_TRUE;
                break;
            case OP_DIFF:
                if (!rs_subtract(&out->ranges, &operand.ranges)) { vp->err = "out of memory"; cs_free(&operand); goto fail; }
                ss_filter(&out->strings, &operand.strings, EJS_FALSE, operand.strings.has_empty);
                break;
            case OP_INTER:
                if (!rs_intersect(&out->ranges, &operand.ranges)) { vp->err = "out of memory"; cs_free(&operand); goto fail; }
                ss_filter(&out->strings, &operand.strings, EJS_TRUE, operand.strings.has_empty);
                break;
            default:
                vp->err = "internal: no operator";
                cs_free(&operand);
                goto fail;
        }
        cs_free(&operand);
    }
    vp->i++; /* consume ']' */

    if (negated) {
        /* MayContainStrings in a negated class is a SyntaxError */
        if (out->strings.n || out->strings.has_empty) {
            vp->err = "negated character class may not contain strings";
            goto fail;
        }
        if (!rs_complement(&out->ranges)) { vp->err = "out of memory"; goto fail; }
    }
    return EJS_TRUE;

 fail:
    cs_free(out);
    return EJS_FALSE;
}

/* ---------- the translator ---------- */
/* Expands \p{...}/\P{...} under /u and /v, and parses whole v-mode
 * classes into evaluated sets; outside /u and /v, \p and \P are Annex
 * B identity escapes (matching the literal 'p'/'P'), which PCRE's
 * UCP-less build would otherwise reject.  Everything else is copied
 * verbatim.  Returns a malloc'd NUL-terminated buffer the caller
 * frees, or NULL with *err set. */
jschar*
_ejs_regexp_translate_pattern (const jschar *chars, uint32_t len, EJSBool unicode,
                               EJSBool unicode_sets, EJSBool dot_all,
                               uint32_t *out_len, const char **err)
{
    PatBuf out = { NULL, 0, 0 };
    EJSBool in_class = EJS_FALSE;
    EJSBool property_mode = unicode || unicode_sets;
    *err = NULL;

    /* effective dot-matches-all at the current position: the /s flag,
     * toggled by inline modifier groups ((?s:...), (?-s:...)).  One
     * entry per open group so ')' restores the right state. */
    EJSBool dot_stack[64];
    uint32_t dot_depth = 0;
    EJSBool dot_now = dot_all;

    for (uint32_t i = 0; i < len; ) {
        jschar c = chars[i];

        if (c == '[' && unicode_sets) {
            /* a v-mode class: parse and evaluate the whole set */
            VParse vp = { chars, len, i + 1, NULL };
            ClassSet cs;
            if (!v_class_set(&vp, &cs)) {
                *err = vp.err ? vp.err : "invalid character class";
                free(out.data);
                return NULL;
            }
            EJSBool ok = emit_class_set(&out, &cs);
            cs_free(&cs);
            if (!ok) goto oom;
            i = vp.i;
            continue;
        }

        if (c != '\\') {
            if (c == '[' && !in_class) in_class = EJS_TRUE;
            else if (c == ']' && in_class) in_class = EJS_FALSE;
            else if (c == '(' && !in_class) {
                /* track group nesting for the inline-s state; a
                 * modifier group ((?ims-ims: ...) toggles it for its
                 * extent.  pcre applies the same semantics to any `.`
                 * we leave bare, so tracking alone keeps us aligned. */
                EJSBool entered = dot_now;
                if (i + 1 < len && chars[i+1] == '?') {
                    uint32_t j = i + 2;
                    EJSBool removing = EJS_FALSE, adds = EJS_FALSE, removes = EJS_FALSE, wellformed = EJS_FALSE;
                    for (; j < len; j++) {
                        jschar m = chars[j];
                        if (m == ':') { wellformed = EJS_TRUE; break; }
                        if (m == '-' && !removing) { removing = EJS_TRUE; continue; }
                        if (m == 'i' || m == 'm' || m == 's') {
                            if (m == 's') { if (removing) removes = EJS_TRUE; else adds = EJS_TRUE; }
                            continue;
                        }
                        break; /* (?=, (?!, (?<... — not a modifier group */
                    }
                    if (wellformed) {
                        if (adds) entered = EJS_TRUE;
                        if (removes) entered = EJS_FALSE;
                    }
                }
                if (dot_depth < sizeof(dot_stack)/sizeof(dot_stack[0]))
                    dot_stack[dot_depth] = dot_now;
                dot_depth++;
                dot_now = entered;
            }
            else if (c == ')' && !in_class) {
                if (dot_depth > 0) {
                    dot_depth--;
                    if (dot_depth < sizeof(dot_stack)/sizeof(dot_stack[0]))
                        dot_now = dot_stack[dot_depth];
                }
            }
            else if (c == '.' && !in_class && !dot_now) {
                // JS `.` excludes all four LineTerminators; pcre's
                // excludes only \n.  (Under /s PCRE_DOTALL matches
                // everything, where the engines agree.)
                if (!buf_puts(&out, "[^\\u000A\\u000D\\u2028\\u2029]")) goto oom;
                i++;
                continue;
            }
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

        if (!property_mode) {
            /* Annex B: IdentityEscape — \p matches "p" */
            if (!buf_putc(&out, esc)) goto oom;
            i += 2;
            continue;
        }

        EJSBool negated = (esc == 'P');
        uint32_t j = i + 2;
        const EJSUnicodeProperty *prop;
        const EJSUnicodeStringProperty *sprop;
        /* properties of strings need /v, and may not be negated */
        EJSBool strings_ok = unicode_sets && !negated && !in_class;
        if (!parse_property(chars, len, &j, strings_ok, &prop, &sprop)) {
            *err = "invalid property name in \\p{...}";
            free(out.data);
            return NULL;
        }

        EJSBool ok;
        if (sprop) {
            /* top-level \p{RGI_Emoji} and friends: an alternation of
             * the sequences plus the single-code-point members */
            ClassSet cs;
            memset(&cs, 0, sizeof(cs));
            ok = rs_add_ranges(&cs.ranges, sprop->ranges, sprop->nranges);
            const uint16_t *p = sprop->strings;
            for (uint32_t k = 0; ok && k < sprop->nstrings; k++) {
                uint32_t slen = *p++;
                jschar tmp[32];
                for (uint32_t u = 0; u < slen; u++) tmp[u] = (jschar)p[u];
                ok = ss_push_units(&cs.strings, tmp, slen);
                p += slen;
            }
            if (ok) ok = emit_class_set(&out, &cs);
            cs_free(&cs);
        } else if (in_class) {
            /* splice bare into the surrounding (u-mode) class */
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
