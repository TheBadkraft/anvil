/* writer-script - anvil-dump's writer mode (conformance/FORMAT.md, "anvil-writer-script 1"). */
#include "writer-script.h"
#include <stdio.h>

int writer_script_run(const char *path, bool output_only) {
   (void)path;
   (void)output_only;
   puts("anvil-writer-run 1");
   return 0;
}
