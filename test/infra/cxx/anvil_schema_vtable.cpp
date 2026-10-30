// Probe: includes only anvil_schema_vtable.h (the Schema vtable object).
#include "anvil_schema_vtable.h"
int main() {
   anvil_schema s = Schema.load(nullptr);
   size_t n = Schema.get_violation_count(s);
   Schema.dispose(s);
   return n == 0 ? 0 : 1;
}
