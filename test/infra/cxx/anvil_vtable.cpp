// Probe: includes only anvil_vtable.h (the Anvil/Document/... vtable objects).
#include "anvil_vtable.h"
int main() {
   const char src[] = "#!aml\nx := 1;\n";
   anvil_document d = Anvil.load_buffer(src, sizeof src - 1);
   if (!d || Anvil.has_errors(d)) return 1;
   int rc = Document.find_statement(d, "x") ? 0 : 2;
   Anvil.dispose(d);
   return rc;
}
