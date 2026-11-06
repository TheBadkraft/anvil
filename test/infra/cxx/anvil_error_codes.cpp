// Probe: includes only anvil_error_codes.h (the enum of specific codes, stable public ABI).
#include "anvil_error_codes.h"
int main() {
   anvl_err_code c = ANVL_ERR_RESOLVER_REFERENCE_CYCLE;
   return (c == 4054 && ANVL_ERR_NONE == 0 && ANVL_ERR_PARSER_NESTED_BASE == 4022) ? 0 : 1;
}
