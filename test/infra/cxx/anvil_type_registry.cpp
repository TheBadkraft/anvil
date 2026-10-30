// Probe: includes only anvil_type_registry.h. A null document is a documented-safe no-op input.
#include "anvil_type_registry.h"
int main() {
   anvil_type_registry r = anvil_type_registry_load(nullptr);
   anvil_type_def d = anvil_type_registry_find(r, "int32");
   anvil_type_registry_dispose(r);
   return d ? 1 : 0;
}
