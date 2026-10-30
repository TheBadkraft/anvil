// Probe: includes only anvil_types.h (types and enums only, no functions to call).
#include "anvil_types.h"
int main() {
   anvil_numeric_t n = anvil_numeric_t();
   anvil_value_type t = ANVIL_VALUE_NULL;
   anvil_err_code e = ANVIL_OK;
   anvil_document d = nullptr;
   return (n.is_integral || t != ANVIL_VALUE_NULL || e != ANVIL_OK || d) ? 1 : 0;
}
