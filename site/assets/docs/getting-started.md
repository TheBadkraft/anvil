# Getting Started with Anvil Native

A practical, task-oriented walkthrough of the C API — load a document, read what's in it,
clean up. Every example below has been compiled and run against the real parser, not just
written to look right. For the grammar itself (what's legal to write in a `.anvl` file), see
[`language-reference.md`](language-reference.md); this page is about the C side.

## 1. Prerequisites

Anvil Native has zero external dependencies and builds with a C23 compiler (`-std=c2x`). A real
distributable — static and shared, Linux x86_64 — is [downloadable directly](/download); this
walkthrough compiles the core sources directly into your own binary instead, the same way every
test suite in the repo does, which works identically either way once you have the headers and
either the library or the sources:

```sh
gcc -std=c2x -Iinclude your_program.c \
  src/core/*.c src/sigma/*.c \
  -o your_program
```

Add `src/anvil_types.c` to that list if you're using the opt-in type system (§9).

## 2. Your First Parse

```c
#include "anvil_flat.h"
#include <string.h>

int main(void) {
   const char *source = "#!aml\nname := David;\n";
   anvil_document doc = anvil_load_buffer(source, strlen(source));

   if (!doc) {
      return 1; // the handle itself failed to allocate
   }
   if (anvil_has_errors(doc)) {
      // handle the failure — see §3
   }

   anvil_dispose(doc);
   return 0;
}
```

One detail worth internalizing early: `anvil_load_buffer`'s `length` is read literally — exactly
that many bytes, nothing inferred. Passing `strlen(source)` (as above) is what you want for a
NUL-terminated C string; passing `0` parses an *empty* buffer, not "figure out the length
yourself" — an easy mistake to make once, since an empty parse never reports an error either.

## 3. Loading a Document

Two ways in, otherwise identical:

```c
anvil_document from_file   = anvil_load("config.anvl");
anvil_document from_buffer = anvil_load_buffer(source, strlen(source));
```

A **failed** parse still gives you a real, usable handle — `anvil_load`/`anvil_load_buffer` only
return `NULL` for the one truly foundational failure (the handle itself couldn't be allocated).
Check `anvil_has_errors`, and read the detail with `anvil_document_get_error`:

```c
if (anvil_has_errors(doc)) {
   anvil_error err = anvil_document_get_error(doc);
   char message[256] = {0};
   anvil_error_get_message(err, message, sizeof(message));
   size_t line = anvil_error_get_line(err);
   size_t column = anvil_error_get_column(err);
   fprintf(stderr, "parse failed at %zu:%zu — %s\n", line, column, message);
}
```

`anvil_get_error(doc)` alone gives you the stable category (`ANVIL_ERR_SYNTAX`,
`ANVIL_ERR_IMPORT`, ...) without the detail object — see `language-reference.md`'s §12 for the
full category table.

The examples below assume this document, loaded once:

```c
const char *config =
   "#!aml\n"
   "@[env=production]\n"
   "\n"
   "host := localhost;\n"
   "port := 8080;\n"
   "tags := [ alpha, beta ];\n";
anvil_document doc = anvil_load_buffer(config, strlen(config));
```

## 4. Walking Statements

`anvil_document_get_statements` returns a heapless iterator over a document's own top-level
statements, in declaration order:

```c
anvil_statement_iterator it = anvil_document_get_statements(doc);
anvil_statement stmt = NULL;
while (anvil_statement_iterator_next(it, &stmt)) {
   char name[64] = {0};
   anvil_statement_get_name(stmt, name, sizeof(name));
   printf("statement: %s\n", name);
}
anvil_statement_iterator_dispose(it);
```

```
statement: host
statement: port
statement: tags
```

`anvil_statement_get_name` follows the same buffer-sizing convention used throughout the public
API: it always returns the full length, regardless of whether it fit — call once with `buf` `NULL`
to size a buffer first if you don't already know a safe upper bound.

## 5. Reading Values

Every statement has a value; every value has a kind (`anvil_value_get_type`). Scalars
(bool/numeric/string/blob/identifier) read their content as text via `anvil_value_get_text`;
arrays and tuples are indexed via `anvil_value_get_count`/`anvil_value_get_element`; objects are
walked the same way `anvil_statement`s are (`anvil_value_get_count`/`anvil_value_get_statement`):

```c
void print_value(anvil_value val, int depth) {
   switch (anvil_value_get_type(val)) {
   case ANVIL_VALUE_ARRAY:
   case ANVIL_VALUE_TUPLE: {
      size_t count = anvil_value_get_count(val);
      for (size_t i = 0; i < count; i++) {
         print_value(anvil_value_get_element(val, i), depth + 1);
      }
      break;
   }
   case ANVIL_VALUE_OBJECT: {
      size_t count = anvil_value_get_count(val);
      for (size_t i = 0; i < count; i++) {
         anvil_statement field = anvil_value_get_statement(val, i);
         print_value(anvil_statement_get_value(field), depth + 1);
      }
      break;
   }
   default: { // NULL, BOOL, NUMERIC, STRING, BLOB, IDENTIFIER — all scalar text
      char text[128] = {0};
      anvil_value_get_text(val, text, sizeof(text));
      printf("%*s%s\n", depth * 2, "", text);
      break;
   }
   }
}
```

Running that over `config`'s three statements:

```
host:
  localhost
port:
  8080
tags:
  alpha
  beta
```

A resolved `$identifier` VarRef needs no special case here — `anvil_value_get_type` reports
whatever the reference resolved *to*, transparently (see `language-reference.md`'s §4).

## 6. Looking Up a Statement by Name

When you know what you want, skip the walk:

```c
anvil_statement port = anvil_statement_get(doc, "port");
if (port) {
   char text[32] = {0};
   anvil_value_get_text(anvil_statement_get_value(port), text, sizeof(text));
   printf("port = %s\n", text); // port = 8080
}
```

## 7. Reading Attributes

Module-level attributes (in the document header) and statement-level ones (`@[...]` on an
individual statement) are read the same shape of way, just off a different handle:

```c
anvil_attribute env = anvil_document_find_attribute(doc, "env");
if (env) {
   char value[32] = {0};
   anvil_attribute_get_value(env, value, sizeof(value));
   printf("env = %s\n", value); // env = production
}
```

A **flag** attribute (`@[active]`, no `=value`) is present the same way — `find_attribute` still
returns a real handle, `anvil_attribute_get_value` just reports zero length for it. Use
`anvil_document_find_attribute`/`anvil_statement_find_attribute` when you know the key you're
after (as above); use the `_count`/indexed-`_get` pair on either level to enumerate every
attribute a document or statement carries.

## 8. Walking Imports

`anvil_document_get_imports` walks a document's own *direct* imports (not the whole transitive
graph) — useful any time you need to reach into what a document pulled in, the same iterator
shape as statements:

```c
anvil_document_iterator it = anvil_document_get_imports(doc);
anvil_document imported = NULL;
while (anvil_document_iterator_next(it, &imported)) {
   anvil_statement_iterator sit = anvil_document_get_statements(imported);
   anvil_statement s = NULL;
   while (anvil_statement_iterator_next(sit, &s)) {
      char name[32] = {0};
      anvil_statement_get_name(s, name, sizeof(name));
      printf("imported statement: %s\n", name);
   }
   anvil_statement_iterator_dispose(sit);
   anvil_dispose(imported); // see §10 — this is safe, and yours to do
}
anvil_document_iterator_dispose(it);
```

## 9. Opting Into Types

A brief taste — full reference at [`types-reference.md`](types-reference.md). Given a document
carrying the `@[types]` module attribute:

```c
const char *types_doc = "#!aml\n@[types]\n\nVIN := {\n   type := String;\n   size := 17;\n};\n";
anvil_document tdoc = anvil_load_buffer(types_doc, strlen(types_doc));

anvil_type_registry reg = anvil_type_registry_load(tdoc);
anvil_type_def vin = anvil_type_resolve(reg, "VIN");
if (vin) {
   long long size = 0;
   anvil_type_def_get_size(vin, &size);
   printf("VIN is kind %d, size %lld\n", anvil_type_def_get_kind(vin), size); // kind 2, size 17
}
anvil_type_registry_dispose(reg);
anvil_dispose(tdoc);
```

`anvil_type_resolve` also recognizes the native primitives (`Numeric`, `String`, ...) and `enum`
directly, with no registry needed at all — pass `NULL` as the first argument for that case.

## 10. Cleaning Up

- `anvil_dispose(doc)` on every handle you got from `anvil_load`/`anvil_load_buffer` — this tears
  down the whole underlying parse context, including every statement/value reached from it.
- A document yielded by `anvil_document_get_imports`'s iterator is real and fully usable, but
  shares its owner's underlying context. Dispose it with `anvil_dispose` like any other document
  when you're done with it (as §8 does) — safe, since it only ever frees the small handle itself,
  never the shared context the owner still needs.
- Iterators (`anvil_statement_iterator`, `anvil_document_iterator`) are disposed separately from
  whatever they yielded, and don't need to outlive what they produced — dispose one as soon as
  you're done walking it.
- `anvil_type_registry_dispose` is separate again — it owns the type definitions it loaded, not
  the document it read them from; dispose the document and the registry independently.

## 11. Where to Go Next

- [`language-reference.md`](language-reference.md) — the full grammar: dialects, statement/value
  forms, attributes, inheritance, imports, errors, and the public API/bindings surface.
- [`types-reference.md`](types-reference.md) — the full opt-in type system.
- [`schema/README.md`](schema/README.md) — AnvilSchema, fully implemented: loading, required-field
  presence, type-kind validation, and full constraint checking.
