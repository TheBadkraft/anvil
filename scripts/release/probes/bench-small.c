/* bench-small.c: small-document cost, for native-vs-native release comparisons.
 *   cold  - the first anvil_load_buffer of a five-line document in this process (one number, ns)
 *   small - median of 21 samples of 20000 loads of that document (ns per load)
 * Output: "cold_ns N" and "small_ns N". Uses only API present since v0.10.0-rc. */
#define _POSIX_C_SOURCE 200809L
#include "anvil_flat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e9 + t.tv_nsec; }
static int cmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
int main(void) {
   const char *src = "#!aml\nhost := \"localhost\";\nport := 8080;\ndebug := true;\nratio := 0.5;\n";
   size_t n = strlen(src);
   double t0 = now_ns(); anvil_document d = anvil_load_buffer(src, n); double cold = now_ns() - t0;
   if (!d || anvil_document_get_error(d)) return 1;
   double s[21];
   for (int k = 0; k < 21; k++) {
      t0 = now_ns();
      for (int i = 0; i < 20000; i++) { anvil_document x = anvil_load_buffer(src, n); if (!x) return 1; }
      s[k] = (now_ns() - t0) / 20000;
   }
   qsort(s, 21, sizeof s[0], cmp);
   printf("cold_ns %.0f\nsmall_ns %.1f\n", cold, s[10]);
   return 0;
}
