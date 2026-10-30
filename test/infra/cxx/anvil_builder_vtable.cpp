// Probe: includes only anvil_builder_vtable.h (the Builder vtable object).
#include "anvil_builder_vtable.h"
int main() {
   anvil_builder b = Builder.create(ANVIL_WRITER_AML);
   if (!b) return 1;
   anvil_node n = Builder.int_value(b, 1);
   int rc = (n && Builder.get_error(b) == ANVIL_WRITER_OK) ? 0 : 2;
   Builder.dispose(b);
   return rc;
}
