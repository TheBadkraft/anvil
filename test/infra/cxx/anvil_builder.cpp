// Probe: includes only anvil_builder.h.
#include "anvil_builder.h"
int main() {
   anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
   if (!b) return 1;
   anvil_node o = anvil_builder_object(b);
   anvil_member m = o ? anvil_builder_add(b, o, "x", anvil_builder_int(b, 1)) : nullptr;
   size_t len = 0;
   const char *t = m ? anvil_builder_emit(b, &len) : nullptr;
   int rc = (m && anvil_builder_get_error(b) == ANVIL_WRITER_OK && (t || len == 0)) ? 0 : 2;
   anvil_builder_dispose(b);
   return rc;
}
