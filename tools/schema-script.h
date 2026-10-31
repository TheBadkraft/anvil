/* schema-script.h - the schema-script mode of anvil-dump (conformance/FORMAT.md, "anvil-schema-script 1"). */
#pragma once

// Runs the schema script at `path` through the native type registry and schema APIs and prints the run
// (anvil-schema-run 1) on stdout. The script's virtual files are written to a private temporary directory
// that is removed before returning.
// Returns 0, or 2 for a tool error (an unreadable or malformed script; the message is on stderr).
int schema_script_run(const char *path);
