// Probe: includes only anvil_writer_types.h (types and enums only, no functions to call).
#include "anvil_writer_types.h"
int main() {
   anvil_writer w = nullptr;
   anvil_writer_dialect d = ANVIL_WRITER_AML;
   anvil_writer_err_code e = ANVIL_WRITER_OK;
   return (w || d != ANVIL_WRITER_AML || e != ANVIL_WRITER_OK) ? 1 : 0;
}
