/* x87.c - the real fast_decimal (include/internal/fast_decimal.h) against strtod, on 20M literals
 * around the fast path's limits, then again with the rounding mode set to each non-default mode.
 * Build with -mfpmath=387 to prove the FLT_EVAL_METHOD gate: the fast path compiles out, so it never
 * answers and nothing can differ. Exit status is the number of mismatches. See
 * scripts/check-numeric-fastpath.sh. */
#define _POSIX_C_SOURCE 200809L
#include "internal/fast_decimal.h"
#include <fenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static long run(const char *label) {
   uint64_t s = 88172645463325252ULL;
   long bad = 0, n = 0;
   char b[64];
   for (long i = 0; i < 20000000; i++) {
      s ^= s << 13; s ^= s >> 7; s ^= s << 17;
      uint64_t m = s % 1000000000000000ULL;
      int e = (int)((s >> 50) % 45) - 22;
      if (!(s >> 60 & 3)) m /= 1000;
      int l = snprintf(b, sizeof b, "%llue%d", (unsigned long long)m, e);
      double d;
      if (!fast_decimal(b, (size_t)l, &d)) continue;
      n++;
      double w = strtod(b, 0);
      if (memcmp(&d, &w, 8)) {
         if (bad++ < 5) printf("%s BAD %s %.17g %.17g\n", label, b, d, w);
      }
   }
   printf("%s: fast path answered %ld, mismatches %ld (ANVL_FAST_DECIMAL=%d)\n", label, n, bad, ANVL_FAST_DECIMAL);
   return bad;
}

int main(void) {
   long bad = run("nearest");
   int modes[] = {FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO};
   const char *names[] = {"upward", "downward", "towardzero"};
   for (int k = 0; k < 3; k++) {
      fesetround(modes[k]);
      bad += run(names[k]);
   }
   fesetround(FE_TONEAREST);
   /* the original x87 counterexample must be right in every build */
   double d, w = strtod("927846616642e-19", 0);
   if (fast_decimal("927846616642e-19", 16, &d) && memcmp(&d, &w, 8)) { printf("x87 counterexample BAD\n"); bad++; }
   return bad != 0;
}
