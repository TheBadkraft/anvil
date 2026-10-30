// Probe: includes only anvil_builder_types.h (types only, no functions to call).
#include "anvil_builder_types.h"
int main() {
   anvil_builder b = nullptr;
   anvil_node n = nullptr;
   anvil_member m = nullptr;
   return (b || n || m) ? 1 : 0;
}
