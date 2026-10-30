// Probe: includes only anvil_schema.h. A null schema is a safe input.
#include "anvil_schema.h"
int main() {
   anvil_schema s = anvil_schema_load(nullptr);
   size_t n = anvil_schema_get_violation_count(s);
   anvil_schema_dispose(s);
   return n == 0 ? 0 : 1;
}
