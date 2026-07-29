/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * ----------------------------------------------------------------------- *
 * files.c - File system utilities
 * ----------------------------------------------------------------------- *
 * Author: BadKraft
 * File: src/core/files.c
 */
#include "internal/files.h"
#include "std.h"

static bool files_load(const char *path, const char **out_source, size_t *out_len) {
   FILE *f = fopen(path, "rb");
   if (!f) return false;

   fseek(f, 0, SEEK_END);
   long len = ftell(f);
   fseek(f, 0, SEEK_SET);

   char *buf = malloc((size_t)len + 1);
   if (!buf) { fclose(f); return false; }

   if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
      free(buf);
      fclose(f);
      return false;
   }
   fclose(f);

   buf[len] = '\0';
   *out_source = buf;
   *out_len = (size_t)len;
   return true;
}
static const char *files_filename(const char *path) {
   if (!path) return NULL;
   const char *filename = strrchr(path, '/');
   if (!filename) filename = strrchr(path, '\\');
   return filename ? filename + 1 : path;
}

const anvl_files_i Files = {
   .load = files_load,
   .filename = files_filename,
};