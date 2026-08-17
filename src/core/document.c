/* *********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.               *
 *                                                                         *
 * This software is proprietary and confidential. Unauthorized copying,    *
 * distribution, modification, or use of this software, via any medium,    *
 * is strictly prohibited without express written permission from the      *
 * copyright holder.                                                       *
 *                                                                         *
 * SPDX-License-Identifier: Proprietary                                    *
 * ----------------------------------------------------------------------- *
 * document.c - Implementation of Anvil document                           *
 * ----------------------------------------------------------------------- *
 * Author: BadKraft                                                        *
 * Created: 2026-07-13                                                     *
 * File: src/core/document.c                                               *
 * *********************************************************************** */

#include "std.h"
#include "types.h"
#include "internal/module.h"
#include "internal/source.h"
#include "internal/source_registry.h"
//
#include <sigma/memory.h>
#include <sigma/strings.h>

static ssize_t doc_size = sizeof(struct anvl_mod_doc_t);
static ssize_t header_size = sizeof(struct anvl_doc_header_t);
static ssize_t import_size = sizeof(struct anvl_doc_import_t);
static ssize_t attribute_size = sizeof(struct anvl_doc_attribute_t);
static ssize_t list_ptr_size = sizeof(void *);

/* ----------------------------------------------------------------------- *
 * module_document management
 * ----------------------------------------------------------------------- */
anvl_result doc_initialize(module_document *out_doc, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;
   module_document doc = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!out_doc) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }
   *out_doc = NULL;

   doc = Allocator.alloc(doc_size);
   if (!doc) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   memset(doc, 0, doc_size);

   // finish initializing document struct
   Source.create(&doc->source, &err_code);
   if (err_code != ANVL_ERR_NONE) {
      goto error;
   }

   doc->header = Allocator.alloc(header_size);
   if (!doc->header) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }
   memset(doc->header, 0, header_size);

   *out_doc = doc;
   return ANVL_RES_OK;

error:
   doc_dispose(doc);
   doc = NULL;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (out_doc) {
      *out_doc = doc;
   }
   return ANVL_RES_ERR;
}
void doc_dispose(module_document doc) {
   if (!doc) {
      return;
   }

   if (doc->source) {
      Registry.remove(Source.hash(doc->source));
      Source.dispose(doc->source);
      doc->source = NULL;
   }

   if (doc->filepath) {
      String.dispose(doc->filepath);
      doc->filepath = NULL;
   }

   if (doc->header) {
      if (doc->header->imports) {
         for (usize i = 0; i < List.size(doc->header->imports); i++) {
            anvl_doc_import imp = NULL;
            List.get(doc->header->imports, i, (object *)&imp);
            Allocator.dispose(imp);
         }
         List.dispose(doc->header->imports);
      }
      if (doc->header->attributes) {
         for (usize i = 0; i < List.size(doc->header->attributes); i++) {
            anvl_doc_attribute attr = NULL;
            List.get(doc->header->attributes, i, (object *)&attr);
            Allocator.dispose(attr);
         }
         List.dispose(doc->header->attributes);
      }
      Allocator.dispose(doc->header);
      doc->header = NULL;
   }

   doc->context = NULL;
   Allocator.dispose(doc);
}

/* ----------------------------------------------------------------------- *
 * Header scanning helpers
 * ----------------------------------------------------------------------- */
static void header_scan_set_error(module_document doc, anvl_err_code code) {
   if (!doc || !doc->source) {
      return;
   }
   Source.set_error(doc->source, code, Source.line(doc->source), Source.column(doc->source),
                    doc->filepath, NULL);
}
static bool header_skip_ws_comments(module_document doc, anvl_err_code *err_code) {
   anvl_source src = doc->source;
   const char *data = Source.data(src);

   while (!Source.is_eof(src)) {
      char c = Source.peek(src);

      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
         Source.consume(src, 1);
         continue;
      }

      if (c == '/' && Source.peek_offset(src, 1) == '/') {
         while (!Source.is_eof(src) && Source.peek(src) != '\n') {
            Source.consume(src, 1);
         }
         continue;
      }

      if (c == '/' && Source.peek_offset(src, 1) == '*') {
         Source.consume(src, 2);
         while (!Source.is_eof(src)) {
            if (Source.peek(src) == '*' && Source.peek_offset(src, 1) == '/') {
               Source.consume(src, 2);
               break;
            }
            Source.consume(src, 1);
         }
         if (Source.is_eof(src) &&
             !(Source.peek(src) == '*' && Source.peek_offset(src, 1) == '/')) {
            *err_code = ANVL_ERR_PARSER_UNTERMINATED_COMMENT;
            return false;
         }
         continue;
      }

      break;
   }

   (void)data;
   return true;
}
static bool header_scan_imports(module_document doc, anvl_err_code *err_code) {
   anvl_source src = doc->source;

   while (true) {
      if (!header_skip_ws_comments(doc, err_code)) {
         return false;
      }

      if (Source.match_length(src, "import", 6) != 6 ||
          Source.is_identifier_part(Source.peek_offset(src, 6))) {
         break;
      }

      if (Source.dialect(src) == ANVL_DIALECT_AMP) {
         *err_code = ANVL_ERR_IMPORT_AMP_FORBIDDEN;
         return false;
      }

      usize decl_start = Source.position(src);
      Source.consume(src, 6); // "import"

      if (!header_skip_ws_comments(doc, err_code)) {
         return false;
      }

      if (Source.peek(src) != '"') {
         *err_code = ANVL_ERR_PARSER_UNEXPECTED_TOKEN;
         return false;
      }

      usize path_start = Source.position(src);
      Source.consume(src, 1); // opening quote
      while (!Source.is_eof(src) && Source.peek(src) != '"') {
         Source.consume(src, 1);
      }
      if (Source.peek(src) != '"') {
         *err_code = ANVL_ERR_PARSER_UNTERMINATED_STRING;
         return false;
      }
      usize path_end = Source.position(src); // index of closing quote
      Source.consume(src, 1);                // closing quote
      usize decl_end = Source.position(src); // after closing quote, before ';'

      if (!header_skip_ws_comments(doc, err_code)) {
         return false;
      }
      if (Source.peek(src) != ';') {
         *err_code = ANVL_ERR_PARSER_UNEXPECTED_TOKEN;
         return false;
      }
      Source.consume(src, 1); // semicolon

      anvl_doc_import imp = Allocator.alloc(import_size);
      if (!imp) {
         *err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
         return false;
      }
      // Store the positions of the declaration and path slices in the import struct.
      imp->decl.data = src->buffer.bucket;
      imp->decl.start = src->buffer.bucket + decl_start;
      imp->decl.end = src->buffer.bucket + decl_end;
      imp->path.data = src->buffer.bucket;
      imp->path.start = src->buffer.bucket + path_start;
      imp->path.end = src->buffer.bucket + path_end + 1;
      List.append(doc->header->imports, (object)imp);
   }

   return true;
}
static bool header_scan_attributes(module_document doc, anvl_err_code *err_code) {
   anvl_source src = doc->source;

   while (true) {
      if (!header_skip_ws_comments(doc, err_code)) {
         return false;
      }

      if (Source.match_length(src, "@[", 2) != 2) {
         break;
      }

      if (Source.dialect(src) == ANVL_DIALECT_AMP) {
         *err_code = ANVL_ERR_PARSER_UNEXPECTED_TOKEN;
         return false;
      }

      Source.consume(src, 2); // "@["

      while (true) {
         if (!header_skip_ws_comments(doc, err_code)) {
            return false;
         }

         if (!Source.is_identifier_start(Source.peek(src))) {
            *err_code = ANVL_ERR_PARSER_INVALID_IDENTIFIER;
            return false;
         }

         usize key_start = Source.position(src);
         Source.consume(src, 1);
         while (Source.is_identifier_part(Source.peek(src))) {
            Source.consume(src, 1);
         }
         usize key_end = Source.position(src);

         anvl_doc_attribute attr = Allocator.alloc(attribute_size);
         if (!attr) {
            *err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
            return false;
         }
         attr->key.data = src->buffer.bucket;
         attr->key.start = src->buffer.bucket + key_start;
         attr->key.end = src->buffer.bucket + key_end;

         if (!header_skip_ws_comments(doc, err_code)) {
            Allocator.dispose(attr);
            return false;
         }

         if (Source.peek(src) == '=') {
            Source.consume(src, 1); // '='
            if (!header_skip_ws_comments(doc, err_code)) {
               Allocator.dispose(attr);
               return false;
            }
            usize value_start = Source.position(src);
            bool in_string = false;
            while (!Source.is_eof(src)) {
               char c = Source.peek(src);
               if (!in_string && (c == ',' || c == ']')) {
                  break;
               }
               if (c == '"') {
                  in_string = !in_string;
               }
               Source.consume(src, 1);
            }
            usize value_end = Source.position(src);
            attr->value.data = src->buffer.bucket;
            attr->value.start = src->buffer.bucket + value_start;
            attr->value.end = src->buffer.bucket + value_end;
         }

         if (!header_skip_ws_comments(doc, err_code)) {
            Allocator.dispose(attr);
            return false;
         }

         List.append(doc->header->attributes, (object)attr);

         if (Source.peek(src) == ',') {
            Source.consume(src, 1);
            continue;
         }
         if (Source.peek(src) == ']') {
            Source.consume(src, 1);
            break;
         }
         *err_code = ANVL_ERR_PARSER_UNEXPECTED_TOKEN;
         return false;
      }
   }

   return true;
}
/* ----------------------------------------------------------------------- *
 * Document header scanning
 * ----------------------------------------------------------------------- */
anvl_result doc_scan_header(module_document doc, anvl_err_code *out_err_code) {
   anvl_err_code err_code = ANVL_ERR_NONE;

   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (!doc || !doc->source || !doc->header) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   if (!doc->header->imports) {
      doc->header->imports = List.new(4, list_ptr_size);
   }
   if (!doc->header->attributes) {
      doc->header->attributes = List.new(4, list_ptr_size);
   }
   if (!doc->header->imports || !doc->header->attributes) {
      err_code = ANVL_ERR_MEMORY_ALLOC_FAILED;
      goto error;
   }

   if (!header_scan_imports(doc, &err_code)) {
      goto error;
   }
   if (!header_scan_attributes(doc, &err_code)) {
      goto error;
   }

   // Leave source position at the first body statement.
   if (!header_skip_ws_comments(doc, &err_code)) {
      goto error;
   }

   // Enforce header ordering: no imports may follow attributes.
   if (Source.match_length(doc->source, "import", 6) == 6 &&
       !Source.is_identifier_part(Source.peek_offset(doc->source, 6))) {
      err_code = ANVL_ERR_PARSER_UNEXPECTED_TOKEN;
      goto error;
   }

   return ANVL_RES_OK;

error: {
   if (out_err_code) {
      *out_err_code = err_code;
   }
   if (doc && doc->source && err_code != ANVL_ERR_INVALID_ARGUMENT &&
       err_code != ANVL_ERR_MEMORY_ALLOC_FAILED) {
      header_scan_set_error(doc, err_code);
   }
   return ANVL_RES_ERR;
}
}

/* ----------------------------------------------------------------------- *
 * document loading and unloading
 * ----------------------------------------------------------------------- */
anvl_result doc_load_source(module_document doc, anvl_source_origin origin, const char *source,
                            size_t len, anvl_err_code *out_err_code) {
   // set up locals
   anvl_err_code err_code = ANVL_ERR_NONE;
   // ok if NULL, just won't report errors to caller
   if (out_err_code) {
      *out_err_code = err_code;
   }

   // validate parameters
   if (!doc || !source) {
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   // does document have a source object; if not, create one
   if (!doc->source) {
      if (ANVL_RES_OK != Source.create(&doc->source, &err_code)) {
         goto error;
      }
   }

   // switch on origin to load source from file or buffer
   switch (origin) {
   case ANVL_SOURCE_FROM_FILE:
      if (ANVL_RES_OK != Source.from_file(&doc->source, source, &err_code)) {
         goto error;
      }
      break;
   case ANVL_SOURCE_FROM_BUFFER:
      if (ANVL_RES_OK != Source.from_buffer(&doc->source, source, len, &err_code)) {
         goto error;
      }
      break;
   default:
      err_code = ANVL_ERR_INVALID_ARGUMENT;
      goto error;
   }

   return ANVL_RES_OK;

error:
   if (out_err_code) {
      *out_err_code = err_code;
   }

   return ANVL_RES_ERR;
}
void doc_unload_source(module_document doc) {
   // unload the source return the source object to a pristine state (creates a new source object).
   if (!doc || !doc->source) {
      return;
   }
   Source.dispose(doc->source);
   anvl_err_code err_code = ANVL_ERR_NONE;
   Source.create(&doc->source, &err_code);

   if (err_code != ANVL_ERR_NONE) {
      // if we can't create a new source object, set the document's source to NULL
      doc->source = NULL;
      // set doc error code to indicate failure to create new source object
   }
}

/* ----------------------------------------------------------------------- *
 * document error handling
 * ----------------------------------------------------------------------- */
bool doc_has_errors(module_document doc) {
   if (!doc || !doc->context) {
      return false;
   }

   return anvl_error_is_set(doc->context->errors);
}
bool doc_set_error(module_document doc, anvl_err_code code, usize line, usize col,
                   const char *file) {
   if (!doc || !doc->context) {
      return false;
   }

   return anvl_error_set(doc->context->errors, code, line, col, file, NULL) == ANVL_RES_OK;
}
