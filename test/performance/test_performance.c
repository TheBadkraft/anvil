#include "anvil.h"
#include <sigma.memory/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Performance measurement utility for parser benchmarking */

typedef struct {
   const char *filename;
   size_t file_size;
   uint64_t parse_time_ns;
   uint32_t statement_count;
   double throughput_mbps;
} ParseMetrics;

/* Read entire file into memory */
static char *read_file(const char *filename, size_t *out_size) {
   FILE *f = fopen(filename, "rb");
   if (!f) {
      fprintf(stderr, "Error: Cannot open %s\n", filename);
      return NULL;
   }

   fseek(f, 0, SEEK_END);
   size_t size = ftell(f);
   fseek(f, 0, SEEK_SET);

   char *buffer = malloc(size + 1);
   if (!buffer) {
      fclose(f);
      return NULL;
   }

   size_t read = fread(buffer, 1, size, f);
   buffer[read] = '\0';
   fclose(f);

   if (out_size)
      *out_size = read;
   return buffer;
}

/* Run benchmark on a single file */
static ParseMetrics benchmark_file(const char *filepath) {
   ParseMetrics metrics = {0};
   metrics.filename = filepath;

   /* Read file */
   size_t file_size = 0;
   char *content = read_file(filepath, &file_size);
   if (!content) {
      return metrics;
   }
   metrics.file_size = file_size;

   /* Create context with builder */
   anvl_ctx_builder_i *builder = Context.get_builder();
   if (!builder) {
      free(content);
      return metrics;
   }

   builder->set_dialect(builder, ANVL_DIALECT_AML);
   builder->set_source(builder, content, file_size);

   context ctx = builder->build(builder);
   if (!ctx) {
      free(content);
      return metrics;
   }

   /* Measure parse time */
   struct timespec start, end;
   clock_gettime(CLOCK_MONOTONIC, &start);

   bool result = Context.parse(ctx);

   clock_gettime(CLOCK_MONOTONIC, &end);

   /* Calculate metrics */
   uint64_t ns = (end.tv_sec - start.tv_sec) * 1000000000UL;
   ns += (end.tv_nsec - start.tv_nsec);
   metrics.parse_time_ns = ns;
   metrics.statement_count = result ? Context.statement_count(ctx) : 0;

   /* Calculate throughput (MB/s) */
   double seconds = (double)ns / 1e9;
   if (seconds > 0) {
      metrics.throughput_mbps = ((double)file_size / (1024.0 * 1024.0)) / seconds;
   }

   Context.dispose(ctx);
   free(content);

   return metrics;
}

/* Format bytes as human-readable string */
static const char *format_bytes(size_t bytes, char *buf, size_t buflen) {
   if (bytes < 1024) {
      snprintf(buf, buflen, "%zu B", bytes);
   } else if (bytes < 1024 * 1024) {
      snprintf(buf, buflen, "%.1f KB", (double)bytes / 1024.0);
   } else {
      snprintf(buf, buflen, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
   }
   return buf;
}

/* Format time as human-readable string */
static const char *format_time(uint64_t ns, char *buf, size_t buflen) {
   if (ns < 1000) {
      snprintf(buf, buflen, "%lu ns", ns);
   } else if (ns < 1000000) {
      snprintf(buf, buflen, "%.2f µs", (double)ns / 1000.0);
   } else if (ns < 1000000000) {
      snprintf(buf, buflen, "%.2f ms", (double)ns / 1000000.0);
   } else {
      snprintf(buf, buflen, "%.2f s", (double)ns / 1e9);
   }
   return buf;
}

int main(void) {
   printf("╔════════════════════════════════════════════════════════════════════════════════╗\n");
   printf("║                   Anvil Parser Performance Benchmark                           ║\n");
   printf("╚════════════════════════════════════════════════════════════════════════════════╝\n\n");

   /* Sample files to benchmark (excluding dialect tests) */
   const char *sample_files[] = {
       "test/samples/assignments.anvl",
       "test/samples/objects.anvl",
       "test/samples/inherits.anvl",
       "test/samples/numbers.anvl",
       "test/samples/attributes.anvl",
       "test/samples/arrays.anvl",
       "test/samples/tuples.anvl",
       "test/samples/modpack.anvl",
       "test/samples/large_dataset.anvl",
       "test/samples/complex_nested.anvl",
       "test/samples/repetitive_data.anvl",
       "test/samples/deep_nesting.anvl",
       "test/samples/blob_heavy_json.anvl",
       "test/samples/blob_heavy_xml.anvl",
       "test/samples/blob_heavy_csv.anvl",
       "test/samples/blob_heavy_mixed.anvl",
       "test/samples/amp_envelope.anvl",
       NULL};

   /* Run benchmarks */
   int file_count = 0;
   for (int i = 0; sample_files[i] != NULL; i++) {
      file_count++;
   }

   uint64_t total_time = 0;
   size_t total_bytes = 0;
   int successful = 0;

   printf("%-30s %12s %12s %8s %12s\n", "File", "Size", "Time", "Stmts", "Throughput");
   printf("%-30s %12s %12s %8s %12s\n",
          "────────────────────────────",
          "────────────",
          "────────────",
          "────────",
          "────────────");

   for (int i = 0; sample_files[i] != NULL; i++) {
      ParseMetrics m = benchmark_file(sample_files[i]);
      if (m.file_size == 0) {
         const char *basename = strrchr(sample_files[i], '/');
         basename = basename ? basename + 1 : sample_files[i];
         printf("%-30s %s\n", basename, "FAILED");
         continue;
      }

      successful++;
      total_time += m.parse_time_ns;
      total_bytes += m.file_size;

      char size_buf[32], time_buf[32];
      const char *basename = strrchr(sample_files[i], '/');
      basename = basename ? basename + 1 : sample_files[i];

      printf("%-30s %12s %12s %8u %11.2f MB/s\n",
             basename,
             format_bytes(m.file_size, size_buf, sizeof(size_buf)),
             format_time(m.parse_time_ns, time_buf, sizeof(time_buf)),
             m.statement_count,
             m.throughput_mbps);
   }

   printf("%-30s %12s %12s %8s %12s\n",
          "════════════════════════════",
          "════════════════",
          "════════════════",
          "════════",
          "════════════════");

   /* Summary */
   if (successful > 0 && total_bytes > 0) {
      char total_size_buf[32], total_time_buf[32];
      printf("\n📊 SUMMARY:\n");
      printf("   Successful files: %d\n", successful);
      printf("   Total data:       %s\n", format_bytes(total_bytes, total_size_buf, sizeof(total_size_buf)));
      printf("   Total parse time: %s\n", format_time(total_time, total_time_buf, sizeof(total_time_buf)));

      if (total_time > 0) {
         double avg_time_ms = (double)total_time / (successful * 1e6);
         double overall_throughput = ((double)total_bytes / (1024.0 * 1024.0)) / ((double)total_time / 1e9);
         printf("   Average per file:  %.2f ms\n", avg_time_ms);
         printf("   Overall throughput: %.2f MB/s\n", overall_throughput);
      }
   }

   printf("\n✅ Performance measurement complete\n");
   Anvil.cleanup();
   return 0;
}
