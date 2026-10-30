/* writer-script.h - the writer-script mode of anvil-dump (conformance/FORMAT.md, "anvil-writer-script 1"). */
#pragma once
#include <stdbool.h>
#include <stddef.h>

// anvil-dump.c's quoted-string writer (FORMAT.md, "Lexical rules"), shared with the writer-script output.
void dump_put_quoted(const char *s, size_t n);

// Runs the writer script at `path` through the native writer and prints the run (anvil-writer-run 1) on stdout.
// With `output_only` it prints nothing but the finished document's bytes (and returns 1 when there is none).
// Returns 0, 1 as above, or 2 for a tool error (an unreadable or malformed script; the message is on stderr).
int writer_script_run(const char *path, bool output_only);
