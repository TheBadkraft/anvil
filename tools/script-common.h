/* script-common.h - the lexical rules shared by the writer-script and builder-script modes of anvil-dump
 * (conformance/FORMAT.md): arguments, numbers, the file reader and the quoted-string output. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
   bool quoted;
   char *bytes; // NUL-terminated copy; `len` is the decoded length
   size_t len;
} script_arg;

// anvil-dump.c's quoted-string writer (FORMAT.md, "Lexical rules"), shared with the script runs' output.
void dump_put_quoted(const char *s, size_t n);

bool script_blank(char c);
int script_hexval(int c);
// Parses one argument (a quoted string or a word) at *p and advances *p. Returns NULL, or an error message.
const char *script_parse_arg(const char **p, script_arg *out);
bool script_parse_int64(const char *s, int64_t *out);
bool script_parse_double(const char *s, double *out);
char *script_read_file(const char *path, size_t *len);
// The ANVIL_WRITER_* name of an error code (the builder shares the writer's codes).
const char *script_err_name(int code);
// Prints "<tag> bytes=N text=<quoted>" and a newline.
void script_print_data(const char *tag, const char *data, size_t n);
