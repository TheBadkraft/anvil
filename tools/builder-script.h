/* builder-script.h - the builder-script mode of anvil-dump (conformance/FORMAT.md, "anvil-builder-script 1"). */
#pragma once
#include <stddef.h>

// Runs the builder script at `path` through the native builder and prints the run (anvil-builder-run 1) on stdout.
// With `emit_dir` non-NULL, the bytes of the k-th successful emit or write step go to <emit_dir>/emit-<k>.anvl.
// Returns 0, or 2 for a tool error (an unreadable or malformed script; the message is on stderr).
int builder_script_run(const char *path, const char *emit_dir);
