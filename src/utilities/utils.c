/* ------------------------------------------------------------------
 * Author: BadKraft
 * Created: 2025-12-03
 * File: src/utilities/utils.c
 * ------------------------------------------------------------------
 * Description:
 * This file contains utility functions and definitions.
 * ------------------------------------------------------------------
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 * ------------------------------------------------------------------
 */
#include "utils.h"
#include <sigma.core/types.h>
#include <sigma.memory/memory.h>
#include <stdio.h>
#include <string.h>

anvl_dialect dialect_hint_from_ext(const char *filepath) {
   if (!filepath)
      return ANVL_DIALECT_ASL;

   usize len = strlen(filepath);
   // if the file extension is !.aml, then just return ASL
   if (len > ANVL_EXT_LEN && strcmp(filepath + len - ANVL_EXT_LEN, ANVL_EXT_AML) == 0)
      return ANVL_DIALECT_AML;
   // if (len > ANVL_EXT_LEN && strcmp(filepath + len - ANVL_EXT_LEN, ANVL_EXT_ASL) == 0)
   //    return ANVL_DIALECT_ASL;
   return ANVL_DIALECT_ASL; // default
}

bool load_source(const char *path, const char **out_source, usize *out_len) {
   FILE *f = fopen(path, "rb");
   if (!f)
      return false;

   fseek(f, 0, SEEK_END);
   long len = ftell(f);
   fseek(f, 0, SEEK_SET);

   char *filebuff = Allocator.alloc((usize)len + 1);
   if (!filebuff) {
      fclose(f);
      return false;
   }

   if (fread(filebuff, 1, (usize)len, f) != (usize)len) {
      Allocator.dispose(filebuff);
      fclose(f);
      return false;
   }
   fclose(f);

   filebuff[len] = '\0';
   *out_source = filebuff;
   *out_len = (usize)len;
   return true;
}