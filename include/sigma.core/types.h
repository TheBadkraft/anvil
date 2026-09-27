/* Bridges a real, pre-existing gap in sigma.core's own packaging (confirmed directly: a
 * standalone compile of sigma.core's own src/system_alloc.c fails with
 * "sigma.core/types.h: No such file or directory" even from within sigma.core's own repo --
 * neither its raw include/ nor its packaged package/include/ has types.h at this nested path,
 * only flat at include/types.h). Not a vendored copy of sigma.core's content: this is a
 * zero-logic redirect to Anvil's own pre-existing <sigma/types.h>, which real usage across this
 * codebase already confirms is byte-for-byte identical to sigma.core's real types.h -- present
 * so sigma.core/allocator.h's own #include <sigma.core/types.h> resolves when compiling Anvil's
 * code, without duplicating any actual type definitions. Report the underlying gap back to
 * sigma.core rather than fix it there from here.
 */
#pragma once

#include <sigma/types.h>
