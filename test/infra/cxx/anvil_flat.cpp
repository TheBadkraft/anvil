// Probe: includes only anvil_flat.h.
#include "anvil_flat.h"
#include <cstring>
int main() {
   const char src[] = "#!aml\nx := 1;\n";
   anvil_document d = anvil_load_buffer(src, sizeof src - 1);
   if (!d || anvil_has_errors(d)) return 1;
   anvil_statement s = anvil_document_find_statement(d, "x");
   int rc = s ? 0 : 2;
   anvil_dispose(d);
   return (rc == 0 && anvil_get_version() && std::strlen(anvil_get_version())) ? 0 : (rc ? rc : 3);
}
