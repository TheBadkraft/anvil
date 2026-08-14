/* *********************************************************************** *
 * Copyright (c) 2025 Quantum Override. All rights reserved.               *
 * ----------------------------------------------------------------------- *
 * files.c - File system utilities                                         *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2026-07-18                                                     *
 * File: src/core/files.c                                                  *
 * ----------------------------------------------------------------------- *
 * Description:                                                            *
 * General file operations                                                 *
 * *********************************************************************** */

#include "constants.h"
#include "errors.h"
#include "std.h"
#include "types.h"
#include "internal/files.h"
// -------------------------
#include <sigma/memory.h>

static anvl_result files_load(const char *path, const char **out_source, size_t *out_len,
                              anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (!path) {
      // invalid path - set
      err_code = ANVL_ERR_IO_INVALID_PATH;
      goto error;
   }

   FILE *f = fopen(path, "rb");
   if (!f) {
      err_code = ANVL_ERR_IO_FILE_NOT_FOUND;
      goto error;
   }

   fseek(f, 0, SEEK_END);
   long len = ftell(f);
   fseek(f, 0, SEEK_SET);

   char *filebuff = Allocator.alloc((size_t)len + 1);
   if (!filebuff) {
      fclose(f);
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   if (fread(filebuff, 1, (size_t)len, f) != (size_t)len) {
      Allocator.dispose(filebuff);
      fclose(f);
      err_code = ANVL_ERR_IO_FILE_READ;
      goto error;
   }

   fclose(f);

   filebuff[len] = '\0';
   *out_source = filebuff;
   *out_len = (size_t)len;
   if (out_err_code) {
      *out_err_code = ANVL_ERR_NONE;
   }
   return ANVL_RES_OK;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }
   return ANVL_RES_ERR;
}
static const char *files_filename(const char *path) {
   if (!path)
      return NULL;
   const char *filename = strrchr(path, '/');
   if (!filename)
      filename = strrchr(path, '\\');
   return filename ? filename + 1 : path;
}
static anvl_dialect files_get_dialect_hint(const char *filepath) {
   anvl_dialect dialect = ANVL_DIALECT_AML; // default to AML if no extension is found
   // retrieves dialect hint from the file's extension
   if (!filepath)
      return dialect;

   // find the last dot in the filepath to get the extension
   const char *ext = strrchr(filepath, '.');
   if (!ext) {
      return ANVL_DIALECT_AML;
   }

   if (strcmp(ext, ANVL_EXT_AMP) == 0)
      return ANVL_DIALECT_AMP;
   if (strcmp(ext, ANVL_EXT_AML) == 0 || strcmp(ext, ANVL_EXT_ANVL) == 0)
      return ANVL_DIALECT_AML;
   if (strcmp(ext, ANVL_EXT_ASL) == 0)
      return ANVL_DIALECT_ASL;

   return dialect; // default
}
static usize files_get_length(const char *path) {
   if (!path)
      return 0;

   FILE *f = fopen(path, "rb");
   if (!f)
      return 0;

   fseek(f, 0, SEEK_END);
   long len = ftell(f);
   fclose(f);

   return (len >= 0) ? (usize)len : 0;
}

const anvl_files_i Files = {
   .load = files_load,
   .filename = files_filename,
   .dialect_hint = files_get_dialect_hint,
   .length = files_get_length,
};