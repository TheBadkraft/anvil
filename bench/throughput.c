/* ********************************************************************** *
 * Copyright (c) 2026 Quantum Override. All rights reserved.              *
 *                                                                        *
 * This software is proprietary and confidential. Unauthorized copying,   *
 * distribution, modification, or use of this software, via any medium,   *
 * is strictly prohibited without express written permission from the     *
 * copyright holder.                                                      *
 *                                                                        *
 * SPDX-License-Identifier: Proprietary                                   *
 * ---------------------------------------------------------------------- *
 * bench/throughput.c - Parser throughput on a realistic AML config shape *
 * ---------------------------------------------------------------------- *
 * Description:                                                          *
 * Real, reproducible parse-throughput numbers for a typical hand-authored
 * AML configuration document -- nested service blocks with inheritance,
 * attributes, arrays, and mixed scalar kinds, not a synthetic one-value
 * stress shape (see notes/flywire-parse-scaling-benchmark.md for that
 * different, message-shaped investigation). Links against the built
 * release library artifact (lib/release/libanvil.a), same "prove the
 * shipped thing, not just the sources" discipline as test/functional --
 * these numbers are meant to be quoted on the website, so they must
 * measure exactly what a real caller gets.
 *
 * Measures anvil_load_buffer end to end (parse + resolve), the real cost
 * a caller pays, not an isolated sub-phase -- warmed (repeated calls on
 * the same already-generated document, discarding the first) so results
 * reflect steady-state throughput, not one-time process startup cost.
 * ********************************************************************** */

#include "anvil_flat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ----------------------------------------------------------------- *
 * Document generator -- a realistic microservice-style AML config:
 * a shared base template, N service blocks each inheriting from it,
 * each carrying an environment attribute, scalar fields of every kind
 * (string, numeric, bool), a tags array, and a nested settings object.
 * ----------------------------------------------------------------- */
static char *generate_config(int service_count, size_t *out_len) {
   size_t cap = 256 + (size_t)service_count * 512;
   char *buf = malloc(cap);
   size_t pos = 0;

   pos += (size_t)snprintf(buf + pos, cap - pos,
                            "#!aml\n\n"
                            "base_service := {\n"
                            "   timeout_ms := 5000;\n"
                            "   retries := 3;\n"
                            "   enabled := true;\n"
                            "};\n\n");

   for (int i = 0; i < service_count; i++) {
      pos += (size_t)snprintf(
          buf + pos, cap - pos,
          "service_%d : base_service @[env=production, region=us-east-%d] := {\n"
          "   name := \"worker-%d\";\n"
          "   host := \"10.0.%d.%d\";\n"
          "   port := %d;\n"
          "   weight := %d.%d;\n"
          "   tags := [ backend, worker, shard-%d ];\n"
          "   settings := {\n"
          "      max_connections := %d;\n"
          "      keepalive := true;\n"
          "      idle_timeout_ms := %d;\n"
          "   };\n"
          "};\n\n",
          i, (i % 4) + 1, i, (i / 256) % 256, i % 256, 8000 + (i % 1000), i % 10, i % 100,
          i % 16, 50 + (i % 200), 30000 + (i % 5000));

      if (pos + 512 >= cap) {
         cap *= 2;
         buf = realloc(buf, cap);
      }
   }

   *out_len = pos;
   return buf;
}

static double now_ms(void) {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void run_case(const char *label, int service_count, int iterations) {
   size_t len = 0;
   char *source = generate_config(service_count, &len);

   /* Warm-up: confirms the generated document is genuinely valid, and
    * primes caches before timing. */
   anvil_document warm = anvil_load_buffer(source, len);
   if (anvil_has_errors(warm)) {
      fprintf(stderr, "generated document for '%s' failed to parse -- bug in the generator, "
                       "not the parser\n",
              label);
      anvil_dispose(warm);
      free(source);
      exit(1);
   }
   anvil_dispose(warm);

   double t0 = now_ms();
   for (int i = 0; i < iterations; i++) {
      anvil_document doc = anvil_load_buffer(source, len);
      anvil_dispose(doc);
   }
   double t1 = now_ms();

   double total_ms = t1 - t0;
   double per_call_ms = total_ms / iterations;
   double mb_per_sec = (per_call_ms > 0) ? ((double)len / 1e6) / (per_call_ms / 1000.0) : 0.0;

   printf("%-22s  %8zu bytes  %6d services  %8.4f ms/call  %9.2f MB/s\n", label, len,
          service_count, per_call_ms, mb_per_sec);

   free(source);
}

int main(void) {
   printf("Anvil Native parse throughput -- anvil_load_buffer, warmed, realistic AML config "
          "shape\n");
   printf("(%s)\n\n", anvil_get_version());

   /* Small: a handful of services, the shape of a real small app's config. */
   run_case("small", 5, 5000);
   /* Medium: a moderate microservice fleet. */
   run_case("medium", 100, 1000);
   /* Large: a big, generated environment-wide config. */
   run_case("large", 1000, 200);

   return 0;
}
