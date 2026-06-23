/*
 * Copyright (c) 2026 Quantum Override. All rights reserved.
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * sigma.core/strings.h — String and StringBuilder vtable interfaces
 * Extracted from sigma.core / sigma.text (MIT)
 * ------------------------------------------------------------------
 */
#pragma once

#include <sigma.core/types.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct string_builder_s *string_builder;

typedef struct sc_string_i {
    usize  (*length)  (string);
    string (*copy)    (string);
    string (*dupe)    (const char *);
    string (*concat)  (string, string);
    string (*format)  (string, ...);
    int    (*compare) (string, string);
    char * (*to_array)(string);
    void   (*dispose) (string);
} sc_string_i;

typedef struct sc_stringbuilder_i {
    string_builder (*new)        (usize capacity);
    string_builder (*snew)       (string);
    void           (*append)     (string_builder, string);
    void           (*appendf)    (string_builder, string, ...);
    void           (*appendl)    (string_builder, string);
    void           (*lappends)   (string_builder, string);
    void           (*lappendf)   (string_builder, string, ...);
    void           (*clear)      (string_builder);
    string         (*toString)   (string_builder);
    void           (*toStream)   (string_builder, FILE *);
    usize          (*length)     (string_builder);
    usize          (*capacity)   (string_builder);
    void           (*setCapacity)(string_builder, usize);
    void           (*dispose)    (string_builder);
} sc_stringbuilder_i;

extern const sc_string_i        String;
extern const sc_stringbuilder_i StringBuilder;
