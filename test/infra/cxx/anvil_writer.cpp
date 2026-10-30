// Probe: includes only anvil_writer.h.
#include "anvil_writer.h"
#include <cstring>
int main() {
   anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
   if (!w) return 1;
   bool ok = anvil_writer_statement(w, "x", nullptr) && anvil_writer_int(w, 1) && anvil_writer_finish(w);
   size_t len = 0;
   const char *t = anvil_writer_data(w, &len);
   int rc = (ok && t && len && std::strstr(t, "x") && anvil_writer_get_error(w) == ANVIL_WRITER_OK) ? 0 : 2;
   anvil_writer_dispose(w);
   return rc;
}
