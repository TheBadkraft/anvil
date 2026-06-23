/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * Extracted from sigma.text (MIT) — adapted to use local Allocator
 * ------------------------------------------------------------------
 * strings.c — String and StringBuilder implementation
 * ------------------------------------------------------------------
 */

#include <sigma.core/strings.h>
#include <sigma.memory/memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* String                                                             */
/* ------------------------------------------------------------------ */
static string str_alloc_copy(const char *src, usize len) {
    if (!src || !len) return NULL;
    string r = Allocator.alloc(len + 1);
    if (r) { memcpy(r, src, len); r[len] = '\0'; }
    return r;
}

static usize  str_length (string s)               { return s ? strlen(s) : 0; }
static string str_copy   (string s)               { return str_alloc_copy(s, str_length(s)); }
static string str_dupe   (const char *s)          { return str_alloc_copy(s, s ? strlen(s) : 0); }
static int    str_compare(string a, string b)     {
    if (a == b) return 0;
    if (!a || !b) return a ? 1 : -1;
    return strcmp(a, b);
}
static string str_concat(string a, string b) {
    if (!a || !b) return NULL;
    usize la = strlen(a), lb = strlen(b);
    string r = Allocator.alloc(la + lb + 1);
    if (r) { memcpy(r, a, la); memcpy(r + la, b, lb); r[la+lb] = '\0'; }
    return r;
}
static string str_format(string fmt, ...) {
    if (!fmt) return NULL;
    va_list ap, ap2;
    va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n < 0) { va_end(ap); return NULL; }
    string r = Allocator.alloc((usize)n + 1);
    if (r) vsnprintf(r, (usize)n + 1, fmt, ap);
    va_end(ap);
    return r;
}
static char  *str_to_array(string s)  { return str_alloc_copy(s, str_length(s)); }
static void   str_dispose (string s)  { if (s) Allocator.dispose(s); }

const sc_string_i String = {
    .length   = str_length,
    .copy     = str_copy,
    .dupe     = str_dupe,
    .concat   = str_concat,
    .format   = str_format,
    .compare  = str_compare,
    .to_array = str_to_array,
    .dispose  = str_dispose,
};

/* ------------------------------------------------------------------ */
/* StringBuilder                                                      */
/* ------------------------------------------------------------------ */
struct string_builder_s {
    char  *buf;
    usize  cap;
    usize  len;
};

static void sb_grow(struct string_builder_s *sb, usize need) {
    if (need <= sb->cap) return;
    usize nc = sb->cap ? sb->cap * 2 : 16;
    while (nc < need) nc *= 2;
    char *nb = Allocator.realloc(sb->buf, nc);
    if (!nb) return;
    sb->buf = nb;
    sb->cap = nc;
}

static string_builder sb_new(usize cap) {
    string_builder sb = Allocator.alloc(sizeof(struct string_builder_s));
    if (!sb) return NULL;
    if (!cap) cap = 16;
    sb->buf = Allocator.alloc(cap);
    if (!sb->buf) { Allocator.dispose(sb); return NULL; }
    sb->cap = cap;
    sb->len = 0;
    sb->buf[0] = '\0';
    return sb;
}
static string_builder sb_snew(string s) {
    usize len = s ? strlen(s) : 0;
    string_builder sb = sb_new(len + 1);
    if (sb && len) {
        memcpy(sb->buf, s, len);
        sb->buf[len] = '\0';
        sb->len = len;
    }
    return sb;
}
static void sb_append(string_builder sb, string s) {
    if (!sb || !s) return;
    usize l = strlen(s);
    sb_grow(sb, sb->len + l + 1);
    if (!sb->buf) return;
    memcpy(sb->buf + sb->len, s, l);
    sb->len += l;
    sb->buf[sb->len] = '\0';
}
static void sb_appendf(string_builder sb, string fmt, ...) {
    if (!sb || !fmt) return;
    va_list ap, ap2;
    va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n < 0) { va_end(ap); return; }
    sb_grow(sb, sb->len + (usize)n + 1);
    if (!sb->buf) { va_end(ap); return; }
    vsnprintf(sb->buf + sb->len, (usize)n + 1, fmt, ap);
    sb->len += (usize)n;
    va_end(ap);
}
static void sb_appendl (string_builder sb, string s) { if (sb) { if (s) sb_append(sb, s); sb_append(sb, "\n"); } }
static void sb_lappends(string_builder sb, string s) { if (sb) { sb_append(sb, "\n"); if (s) sb_append(sb, s); } }
static void sb_lappendf(string_builder sb, string fmt, ...) {
    if (!sb || !fmt) return;
    sb_append(sb, "\n");
    va_list ap;
    va_start(ap, fmt);
    char tmp[1024];
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    sb_append(sb, tmp);
}
static void   sb_clear      (string_builder sb)           { if (sb) { sb->len = 0; if (sb->buf) sb->buf[0] = '\0'; } }
static string sb_toString   (string_builder sb)           {
    if (!sb || !sb->buf) return NULL;
    string r = Allocator.alloc(sb->len + 1);
    if (r) memcpy(r, sb->buf, sb->len + 1);
    return r;
}
static void   sb_toStream   (string_builder sb, FILE *f)  { if (sb && sb->buf && f && sb->len) fwrite(sb->buf, 1, sb->len, f); }
static usize  sb_length     (string_builder sb)           { return sb ? sb->len : 0; }
static usize  sb_capacity   (string_builder sb)           { return sb ? sb->cap : 0; }
static void   sb_setCapacity(string_builder sb, usize nc) { if (sb && nc > sb->cap) sb_grow(sb, nc); }
static void   sb_dispose    (string_builder sb)           {
    if (!sb) return;
    if (sb->buf) Allocator.dispose(sb->buf);
    Allocator.dispose(sb);
}

const sc_stringbuilder_i StringBuilder = {
    .new         = sb_new,
    .snew        = sb_snew,
    .append      = sb_append,
    .appendf     = sb_appendf,
    .appendl     = sb_appendl,
    .lappends    = sb_lappends,
    .lappendf    = sb_lappendf,
    .clear       = sb_clear,
    .toString    = sb_toString,
    .toStream    = sb_toStream,
    .length      = sb_length,
    .capacity    = sb_capacity,
    .setCapacity = sb_setCapacity,
    .dispose     = sb_dispose,
};
