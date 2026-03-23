/*
 * Copyright (c) 2025 Quantum Override. All rights reserved.
 *
 * SPDX-License-Identifier: Proprietary
 * ------------------------------------------------------------------
 * types.c — Type name utilities
 * ------------------------------------------------------------------
 */

#include "types.h"

const char *anvl_value_type_name(anvl_value_type type) {
   switch (type) {
   case ANVL_VALUE_SCALAR:
      return "scalar";
   case ANVL_VALUE_OBJECT:
      return "object";
   case ANVL_VALUE_ARRAY:
      return "array";
   case ANVL_VALUE_TUPLE:
      return "tuple";
   case ANVL_VALUE_BLOB:
      return "blob";
   default:
      return "unknown";
   }
}

const char *anvl_stmt_type_name(anvl_stmt_type type) {
   switch (type) {
   case ANVL_STMT_ASSN:
      return "assignment";
   case ANVL_STMT_FUNC:
      return "function";
   case ANVL_STMT_MSSG:
      return "message";
   case ANVL_ANON_OBJECT:
      return "anon_object";
   default:
      return "unknown";
   }
}
