// New direct-construction parser (builder-free)
// Parses AML/ASL into context-owned statements/values.

#include "anvil.h"
#include "errors.h"
#include "source_internal.h"
#include "context_internal.h"

#include <sigcore/memory.h>
#include <string.h>

typedef struct {
   context ctx;
   source src;
} parser_ctx;

// Forward declarations
static void parser_error(anvl_error_code code, source s);
static void dispose_value_tree(value v);
static void dispose_statement(statement stmt);
static bool parse_source(parser_ctx *p);
static bool parse_statement(parser_ctx *p, statement stmt);
static value parse_value(parser_ctx *p);
static value parse_object(parser_ctx *p);
static value parse_array(parser_ctx *p);
static value parse_tuple(parser_ctx *p);
static bool parse_attribute_block(parser_ctx *p, usize *out_start, usize *out_count);
static bool parse_scalar_value(parser_ctx *p, usize *start, usize *len, anvl_value_type *type);
static bool read_identifier(parser_ctx *p, usize *pos, usize *len);
static attribute *copy_attributes_slice(context ctx, usize start, usize count);
static field *shrink_field_array(field *fields, usize count);
static value *shrink_value_array(value *vals, usize count);

bool anvl_parse(context ctx) {
   if (!ctx || !ctx->source) {
      return false;
   }

   anvl_error_clear();

   parser_ctx p = {
       .ctx = ctx,
       .src = ctx->source,
   };

   return parse_source(&p);
}

static void parser_error(anvl_error_code code, source s) {
   anvl_error_set(code, anvl_error_code_message(code), si_line(s), si_column(s), __FILE__);
}

static void dispose_value_tree(value v) {
   if (!v)
      return;
   if (v->type == ANVL_VALUE_OBJECT && v->data.object.fields) {
      for (usize i = 0; i < v->data.object.field_count; i++) {
         field f = v->data.object.fields[i];
         if (!f)
            continue;
         if (f->attributes)
            Memory.dispose(f->attributes);
         dispose_value_tree(f->val);
         Memory.dispose(f);
      }
      Memory.dispose(v->data.object.fields);
   } else if ((v->type == ANVL_VALUE_ARRAY || v->type == ANVL_VALUE_TUPLE) && v->data.collection.elements) {
      for (usize i = 0; i < v->data.collection.element_count; i++) {
         dispose_value_tree(v->data.collection.elements[i]);
      }
      Memory.dispose(v->data.collection.elements);
   }
   Memory.dispose(v);
}

static void dispose_statement(statement stmt) {
   if (!stmt)
      return;
   if (stmt->attributes)
      Memory.dispose(stmt->attributes);
   dispose_value_tree(stmt->val);
   Memory.dispose(stmt);
}

static bool parse_source(parser_ctx *p) {
   si_skip_whitespace_and_comments(p->src);

   // Module attributes (prefix only)
   while (si_match_length(p->src, "@[", 2) == 2) {
      if (!parse_attribute_block(p, NULL, NULL))
         return false;
      si_skip_whitespace_and_comments(p->src);
   }

   while (!si_is_eof(p->src)) {
      // Shebang is illegal after statements
      if (si_match_length(p->src, "#!", 2) == 2) {
         parser_error(ANVL_ERR_PARSER_SHEBANG_AFTER_STATEMENTS, p->src);
         return false;
      }

      statement stmt = ci_new_statement(p->ctx, ANVL_STMT_ASSN);
      if (!stmt) {
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, p->src);
         return false;
      }

      if (!parse_statement(p, stmt)) {
         dispose_statement(stmt);
         return false;
      }

      ci_add_statement(p->ctx, stmt);
      si_skip_whitespace_and_comments(p->src);
   }

   si_skip_whitespace_and_comments(p->src);
   if (si_match_length(p->src, "@[", 2) == 2) {
      parser_error(ANVL_ERR_PARSER_UNEXPECTED_MODULE_ATTRIBUTES, p->src);
      return false;
   }

   return true;
}

static bool parse_statement(parser_ctx *p, statement stmt) {
   source s = p->src;
   usize start_pos = si_position(s);

   si_skip_whitespace_and_comments(s);

   usize ident_pos = 0, ident_len = 0;
   if (!read_identifier(p, &ident_pos, &ident_len))
      return false;

   si_skip_whitespace_and_comments(s);

   // Statement attributes (optional)
   usize attrib_start = p->ctx->attr_list.count;
   while (si_match_length(s, "@[", 2) == 2) {
      if (!parse_attribute_block(p, NULL, NULL))
         return false;
      si_skip_whitespace_and_comments(s);
   }
   usize attrib_count = p->ctx->attr_list.count - attrib_start;

   // Optional inheritance base
   usize base_pos = 0, base_len = 0;
   bool saw_assign = false;
   if (si_match_length(s, ":=", 2) == 2) {
      si_consume(s, 2);
      saw_assign = true;
   } else if (si_match_length(s, ":", 1) == 1) {
      si_consume(s, 1);
      si_skip_whitespace_and_comments(s);
      if (!read_identifier(p, &base_pos, &base_len))
         return false;
      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, ":=", 2) != 2) {
         parser_error(ANVL_ERR_PARSER_EXPECTED_ASSIGN, s);
         return false;
      }
      si_consume(s, 2);
      saw_assign = true;
   }

   if (!saw_assign) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_ASSIGN, s);
      return false;
   }

   si_skip_whitespace_and_comments(s);

   value val = parse_value(p);
   if (!val)
      return false;

   // Validate attribute applicability
   if (attrib_count > 0 && val->type != ANVL_VALUE_OBJECT && val->type != ANVL_VALUE_ARRAY && val->type != ANVL_VALUE_TUPLE) {
      dispose_value_tree(val);
      parser_error(ANVL_ERR_PARSER_ATTRIBUTES_NOT_ALLOWED_ON_TYPE, s);
      return false;
   }

   // Populate statement fields
   stmt->meta[STMT_META_TYPE] = (base_len > 0) ? (usize)ANVL_STMT_INHERITANCE : (usize)ANVL_STMT_ASSN;
   stmt->meta[STMT_META_IDENT_POS] = ident_pos;
   stmt->meta[STMT_META_IDENT_LEN] = ident_len;
   stmt->meta[STMT_META_VALUE_TYPE] = (usize)val->type;
   stmt->meta[STMT_META_LEN] = si_position(s) - start_pos;

   stmt->base[0] = base_pos;
   stmt->base[1] = base_len;

   if (attrib_count > 0) {
      stmt->attrib_count = attrib_count;
      stmt->attributes = copy_attributes_slice(p->ctx, attrib_start, attrib_count);
   } else {
      stmt->attrib_count = 0;
      stmt->attributes = NULL;
   }

   stmt->val = val;

   si_skip_whitespace_and_comments(s);
   if (si_match_length(s, ",", 1) == 1) {
      si_consume(s, 1);
   }

   return true;
}

static value parse_value(parser_ctx *p) {
   source s = p->src;
   if (si_match_length(s, "{", 1) == 1)
      return parse_object(p);
   if (si_match_length(s, "[", 1) == 1)
      return parse_array(p);
   if (si_match_length(s, "(", 1) == 1)
      return parse_tuple(p);

   usize start = 0, len = 0;
   anvl_value_type type = ANVL_VALUE_SCALAR;
   if (!parse_scalar_value(p, &start, &len, &type))
      return NULL;

   value v = ci_new_value(p->ctx, type);
   if (!v) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }
   v->data.scalar.pos = start;
   v->data.scalar.len = len;
   return v;
}

static value parse_object(parser_ctx *p) {
   source s = p->src;
   si_consume(s, 1); // consume '{'
   si_skip_whitespace_and_comments(s);

   if (si_peek(s) == '}') {
      parser_error(ANVL_ERR_PARSER_EMPTY_OBJECT_NOT_ALLOWED, s);
      return NULL;
   }

   value v = ci_new_value(p->ctx, ANVL_VALUE_OBJECT);
   if (!v) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }

   usize capacity = 4;
   field *fields = Memory.alloc(sizeof(field) * capacity, true);
   usize count = 0;

   while (!si_is_eof(s) && si_peek(s) != '}') {
      usize key_pos = 0, key_len = 0;
      if (!read_identifier(p, &key_pos, &key_len)) {
         dispose_value_tree(v);
         Memory.dispose(fields);
         return NULL;
      }

      si_skip_whitespace_and_comments(s);

      usize attrib_start = p->ctx->attr_list.count;
      while (si_match_length(s, "@[", 2) == 2) {
         if (!parse_attribute_block(p, NULL, NULL)) {
            dispose_value_tree(v);
            Memory.dispose(fields);
            return NULL;
         }
         si_skip_whitespace_and_comments(s);
      }
      usize attrib_count = p->ctx->attr_list.count - attrib_start;

      if (si_match_length(s, ":=", 2) != 2) {
         dispose_value_tree(v);
         Memory.dispose(fields);
         parser_error(ANVL_ERR_PARSER_EXPECTED_ASSIGN, s);
         return NULL;
      }
      si_consume(s, 2);
      si_skip_whitespace_and_comments(s);

      value field_val = parse_value(p);
      if (!field_val) {
         dispose_value_tree(v);
         Memory.dispose(fields);
         return NULL;
      }

      if (count >= capacity) {
         capacity *= 2;
         fields = Memory.realloc(fields, sizeof(field) * capacity);
      }

      field f = ci_new_field(p->ctx);
      f->key_pos = key_pos;
      f->key_len = key_len;
      f->val = field_val;
      f->attrib_count = attrib_count;
      f->attributes = (attrib_count > 0) ? copy_attributes_slice(p->ctx, attrib_start, attrib_count) : NULL;
      fields[count++] = f;

      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, ",", 1) == 1) {
         si_consume(s, 1);
         si_skip_whitespace_and_comments(s);
      } else if (si_match_length(s, "}", 1) == 1) {
         break;
      } else {
         dispose_value_tree(v);
         for (usize i = 0; i < count; i++) {
            if (fields[i]) {
               if (fields[i]->attributes)
                  Memory.dispose(fields[i]->attributes);
               dispose_value_tree(fields[i]->val);
               Memory.dispose(fields[i]);
            }
         }
         Memory.dispose(fields);
         parser_error(ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES, s);
         return NULL;
      }
   }

   if (si_match_length(s, "}", 1) != 1) {
      dispose_value_tree(v);
      for (usize i = 0; i < count; i++) {
         if (fields[i]) {
            if (fields[i]->attributes)
               Memory.dispose(fields[i]->attributes);
            dispose_value_tree(fields[i]->val);
            Memory.dispose(fields[i]);
         }
      }
      Memory.dispose(fields);
      parser_error(ANVL_ERR_PARSER_EXPECTED_OBJECT_CLOSE, s);
      return NULL;
   }

   si_consume(s, 1);

   v->data.object.field_count = count;
   v->data.object.fields = shrink_field_array(fields, count);
   return v;
}

static value parse_array(parser_ctx *p) {
   source s = p->src;
   si_consume(s, 1); // consume '['
   si_skip_whitespace_and_comments(s);

   if (si_peek(s) == ']') {
      parser_error(ANVL_ERR_PARSER_EMPTY_ARRAY_NOT_ALLOWED, s);
      return NULL;
   }

   value v = ci_new_value(p->ctx, ANVL_VALUE_ARRAY);
   if (!v) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }

   usize capacity = 4;
   value *elements = Memory.alloc(sizeof(value) * capacity, true);
   usize count = 0;

   while (!si_is_eof(s) && si_peek(s) != ']') {
      value elem = parse_value(p);
      if (!elem) {
         for (usize i = 0; i < count; i++)
            dispose_value_tree(elements[i]);
         Memory.dispose(elements);
         dispose_value_tree(v);
         return NULL;
      }

      if (count >= capacity) {
         capacity *= 2;
         elements = Memory.realloc(elements, sizeof(value) * capacity);
      }
      elements[count++] = elem;

      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, ",", 1) == 1) {
         si_consume(s, 1);
         si_skip_whitespace_and_comments(s);
      } else if (si_match_length(s, "]", 1) == 1) {
         break;
      } else {
         for (usize i = 0; i < count; i++)
            dispose_value_tree(elements[i]);
         Memory.dispose(elements);
         dispose_value_tree(v);
         parser_error(ANVL_ERR_PARSER_MISSING_COMMA_IN_ARRAY, s);
         return NULL;
      }
   }

   if (si_match_length(s, "]", 1) != 1) {
      for (usize i = 0; i < count; i++)
         dispose_value_tree(elements[i]);
      Memory.dispose(elements);
      dispose_value_tree(v);
      parser_error(ANVL_ERR_PARSER_EXPECTED_ARRAY_CLOSE, s);
      return NULL;
   }

   si_consume(s, 1);

   v->data.collection.element_count = count;
   v->data.collection.elements = shrink_value_array(elements, count);
   return v;
}

static value parse_tuple(parser_ctx *p) {
   source s = p->src;
   si_consume(s, 1); // consume '('
   si_skip_whitespace_and_comments(s);

   value v = ci_new_value(p->ctx, ANVL_VALUE_TUPLE);
   if (!v) {
      parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
      return NULL;
   }

   usize capacity = 4;
   value *elements = Memory.alloc(sizeof(value) * capacity, true);
   usize count = 0;

   while (!si_is_eof(s) && si_peek(s) != ')') {
      value elem = parse_value(p);
      if (!elem) {
         for (usize i = 0; i < count; i++)
            dispose_value_tree(elements[i]);
         Memory.dispose(elements);
         dispose_value_tree(v);
         return NULL;
      }

      if (count >= capacity) {
         capacity *= 2;
         elements = Memory.realloc(elements, sizeof(value) * capacity);
      }
      elements[count++] = elem;

      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, ",", 1) == 1) {
         si_consume(s, 1);
         si_skip_whitespace_and_comments(s);
      } else if (si_peek(s) == ')') {
         break;
      } else {
         for (usize i = 0; i < count; i++)
            dispose_value_tree(elements[i]);
         Memory.dispose(elements);
         dispose_value_tree(v);
         if (si_is_eof(s))
            parser_error(ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE, s);
         else
            parser_error(ANVL_ERR_PARSER_EXPECTED_COMMA_IN_TUPLE, s);
         return NULL;
      }
   }

   if (si_match_length(s, ")", 1) != 1) {
      for (usize i = 0; i < count; i++)
         dispose_value_tree(elements[i]);
      Memory.dispose(elements);
      dispose_value_tree(v);
      parser_error(ANVL_ERR_PARSER_EXPECTED_TUPLE_CLOSE, s);
      return NULL;
   }

   si_consume(s, 1);

   v->data.collection.element_count = count;
   v->data.collection.elements = shrink_value_array(elements, count);
   return v;
}

static bool parse_scalar_value(parser_ctx *p, usize *start, usize *len, anvl_value_type *type) {
   source s = p->src;
   *start = si_position(s);
   *type = ANVL_VALUE_SCALAR;

   char first = si_peek(s);
   if (first == '{' || first == '[' || first == '(') {
      parser_error(ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE, s);
      return false;
   }

   if (si_match_length(s, "\"", 1) == 1) {
      si_consume(s, 1); // opening quote
      bool closed = false;
      while (!si_is_eof(s)) {
         char c = si_peek(s);
         if (c == '"') {
            si_consume(s, 1);
            closed = true;
            break;
         } else if (c == '\\') {
            si_consume(s, 1);
            if (!si_is_eof(s))
               si_consume(s, 1);
         } else {
            si_consume(s, 1);
         }
      }
      if (!closed) {
         parser_error(ANVL_ERR_PARSER_UNTERMINATED_STRING, s);
         return false;
      }
   } else if (si_match_length(s, "@", 1) == 1 || si_match_length(s, "`", 1) == 1) {
      *type = ANVL_VALUE_BLOB;
      usize attr_len = 0;
      if (si_match_length(s, "@", 1) == 1) {
         si_consume(s, 1);
         while (!si_is_eof(s) && Source.is_identifier_part(si_peek(s))) {
            si_consume(s, 1);
            attr_len++;
         }
         if (attr_len == 0) {
            parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, s);
            return false;
         }
      }
      if (si_peek(s) != '`') {
         parser_error(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, s);
         return false;
      }
      si_consume(s, 1); // consume backtick
      while (!si_is_eof(s)) {
         if (si_peek(s) == '`')
            break;
         si_consume(s, 1);
      }
      if (si_peek(s) != '`') {
         parser_error(ANVL_ERR_PARSER_UNTERMINATED_BLOB, s);
         return false;
      }
      si_consume(s, 1);
   } else {
      while (!si_is_eof(s)) {
         char c = si_peek(s);
         if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == '}' || c == ']' || c == ')')
            break;
         si_consume(s, 1);
      }
   }

   *len = si_position(s) - *start;
   return true;
}

static bool parse_attribute_block(parser_ctx *p, usize *out_start, usize *out_count) {
   source s = p->src;
   si_consume(s, 2); // consume "@["
   si_skip_whitespace_and_comments(s);

   if (si_match_length(s, "]", 1) == 1) {
      parser_error(ANVL_ERR_PARSER_EMPTY_ATTRIBUTE_BLOCK, s);
      return false;
   }

   usize start = p->ctx->attr_list.count;

   while (!si_is_eof(s)) {
      usize key_pos = si_position(s);
      usize key_len = 0;
      while (!si_is_eof(s) && (Source.is_identifier_part(si_peek(s)) || si_peek(s) == '.')) {
         si_consume(s, 1);
         key_len++;
      }
      if (key_len == 0) {
         parser_error(ANVL_ERR_PARSER_INVALID_ATTRIBUTE, s);
         return false;
      }

      usize value_pos = 0, value_len = 0;
      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, "=", 1) == 1) {
         si_consume(s, 1);
         si_skip_whitespace_and_comments(s);
         value_pos = si_position(s);
         anvl_value_type vt;
         if (!parse_scalar_value(p, &value_pos, &value_len, &vt))
            return false;
         if (vt != ANVL_VALUE_SCALAR) {
            parser_error(ANVL_ERR_PARSER_INVALID_VALUE_IN_ATTRIBUTE, s);
            return false;
         }
      }

      attribute attr = ci_new_attribute(p->ctx);
      if (!attr) {
         parser_error(ANVL_ERR_MEMORY_ALLOCATION_FAILED, s);
         return false;
      }
      attr->key_pos = key_pos;
      attr->key_len = key_len;
      attr->value_pos = value_pos;
      attr->value_len = value_len;
      ci_add_attribute(p->ctx, attr);

      si_skip_whitespace_and_comments(s);
      if (si_match_length(s, "]", 1) == 1) {
         si_consume(s, 1);
         break;
      }
      if (si_match_length(s, ",", 1) != 1) {
         parser_error(ANVL_ERR_PARSER_MISSING_COMMA_IN_ATTRIBUTES, s);
         return false;
      }
      si_consume(s, 1);
      si_skip_whitespace_and_comments(s);
   }

   if (out_start)
      *out_start = start;
   if (out_count)
      *out_count = p->ctx->attr_list.count - start;
   return true;
}

static bool read_identifier(parser_ctx *p, usize *pos, usize *len) {
   source s = p->src;
   if (!Source.is_identifier_start(si_peek(s))) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, s);
      return false;
   }
   *pos = si_position(s);
   *len = 0;
   while (!si_is_eof(s) && Source.is_identifier_part(si_peek(s))) {
      si_consume(s, 1);
      (*len)++;
   }
   if (*len == 0) {
      parser_error(ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, s);
      return false;
   }
   return true;
}

static attribute *copy_attributes_slice(context ctx, usize start, usize count) {
   if (count == 0)
      return NULL;
   attribute *attrs = Memory.alloc(sizeof(attribute) * count, true);
   for (usize i = 0; i < count; i++) {
      attrs[i] = ctx->attr_list.attributes[start + i];
   }
   return attrs;
}

static field *shrink_field_array(field *fields, usize count) {
   if (!fields)
      return NULL;
   if (count == 0) {
      Memory.dispose(fields);
      return NULL;
   }
   return Memory.realloc(fields, sizeof(field) * count);
}

static value *shrink_value_array(value *vals, usize count) {
   if (!vals)
      return NULL;
   if (count == 0) {
      Memory.dispose(vals);
      return NULL;
   }
   return Memory.realloc(vals, sizeof(value) * count);
}
