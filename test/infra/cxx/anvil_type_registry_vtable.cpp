// Probe: includes only anvil_type_registry_vtable.h (the TypeRegistry vtable object).
#include "anvil_type_registry_vtable.h"
int main() {
   anvil_type_registry r = TypeRegistry.load(nullptr);
   anvil_type_def d = TypeRegistry.find(r, "int32");
   TypeRegistry.dispose(r);
   return d ? 1 : 0;
}
