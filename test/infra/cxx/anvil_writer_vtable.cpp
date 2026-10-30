// Probe: includes only anvil_writer_vtable.h (the Writer vtable object).
#include "anvil_writer_vtable.h"
int main() {
   anvil_writer w = Writer.create(ANVIL_WRITER_AML);
   if (!w) return 1;
   bool ok = Writer.statement(w, "x", nullptr) && Writer.int_value(w, 1) && Writer.finish(w);
   size_t len = 0;
   const char *t = Writer.data(w, &len);
   Writer.dispose(w);
   return (ok && t) ? 0 : 2;
}
