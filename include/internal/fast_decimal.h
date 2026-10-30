/* internal/fast_decimal.h - an exact fast path for decimal-literal to double conversion.
 *
 * Clinger's fast path: a decimal literal whose digits fit a double's 53-bit mantissa exactly
 * (<= 15 significant digits) and whose power of ten is exact (|e10| <= 22) converts with a single
 * correctly rounded multiply or divide, so the result is bit-identical to strtod's. Anything else
 * (more digits, a larger exponent, any other character) returns false and the caller uses strtod.
 * Schema validation reads the same number again for every min/max/integer check, so this keeps
 * that read to a handful of instructions instead of strtod's multi-precision setup.
 *
 * The single multiply or divide is only exact when it rounds once, to 53 bits:
 *  - FLT_EVAL_METHOD must be 0. On x87 (i386 without SSE) it evaluates in 64-bit extended precision
 *    and then rounds again to double, which differs from strtod in the last bit for some literals
 *    (927846616642e-19). The #if below compiles the fast path out there: it always returns false
 *    and every literal goes to strtod, so those builds pay nothing and stay exact.
 *  - The rounding mode must be round-to-nearest: strtod honours fesetround, the fast path does not.
 *    Anvil never changes the mode, but a caller may, so the fast path declines when it is not
 *    round-to-nearest. The test is two volatile adds (1 +- 2^-60 must both
 *    round back to 1, which only round-to-nearest does), not fegetround, so the static library
 *    needs no -lm.
 */
#ifndef ANVIL_INTERNAL_FAST_DECIMAL_H
#define ANVIL_INTERNAL_FAST_DECIMAL_H

#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(FLT_EVAL_METHOD) && FLT_EVAL_METHOD == 0
#define ANVL_FAST_DECIMAL 1
#else
#define ANVL_FAST_DECIMAL 0
#endif

#if ANVL_FAST_DECIMAL
static inline bool round_to_nearest(void) {
   volatile double one = 1.0, eps = 0x1p-60;
   return one + eps == 1.0 && one - eps == 1.0;
}
#endif

static inline bool fast_decimal(const char *text, size_t len, double *out) {
#if ANVL_FAST_DECIMAL
   if (!round_to_nearest()) return false;
   static const double pow10_exact[] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                        1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                        1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
   size_t i = 0;
   bool negative = false;
   if (i < len && (text[i] == '-' || text[i] == '+')) {
      negative = text[i] == '-';
      i++;
   }
   uint64_t mant = 0;
   int digits = 0;   // significant digits accumulated (leading zeros do not count)
   int e10 = 0;
   bool any = false;
   for (; i < len && text[i] >= '0' && text[i] <= '9'; i++) {
      any = true;
      if (mant != 0 || text[i] != '0') {
         if (++digits > 15) return false;
      }
      mant = mant * 10u + (uint64_t)(text[i] - '0');
   }
   if (i < len && text[i] == '.') {
      for (i++; i < len && text[i] >= '0' && text[i] <= '9'; i++) {
         any = true;
         if (mant != 0 || text[i] != '0') {
            if (++digits > 15) return false;
         }
         mant = mant * 10u + (uint64_t)(text[i] - '0');
         e10--;
      }
   }
   if (!any) return false;
   if (i < len && (text[i] == 'e' || text[i] == 'E')) {
      i++;
      bool eneg = false;
      if (i < len && (text[i] == '-' || text[i] == '+')) {
         eneg = text[i] == '-';
         i++;
      }
      if (i >= len) return false;
      int ev = 0;
      for (; i < len && text[i] >= '0' && text[i] <= '9'; i++) {
         if ((ev = ev * 10 + (text[i] - '0')) > 1000) return false;
      }
      e10 += eneg ? -ev : ev;
   }
   if (i != len || e10 < -22 || e10 > 22) return false;
   double d = (double)mant;
   d = e10 < 0 ? d / pow10_exact[-e10] : d * pow10_exact[e10];
   *out = negative ? -d : d;
   return true;
#else
   (void)text; (void)len; (void)out;
   return false;
#endif
}

#endif
