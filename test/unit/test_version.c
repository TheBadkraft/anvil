#include "anvil.h"
#include "constants.h"
#include "testbit.h"
#include <stdio.h>
#include <string.h>

/* BR-2610-anvl-version-001: the reported version is semver, MAJOR.MINOR.PATCH[-TAG][+BUILD], with the
 * pre-release tag BEFORE the build metadata and no "+0" when the build number is unknown. This binary also
 * runs after every unit suite to print the banner. */

static char buf[64];
static const char *fmt(int major, int minor, int patch, const char *tag, int build) {
   anvl_format_version(buf, sizeof(buf), major, minor, patch, tag, build);
   return buf;
}

static void test_ver01_format(void) {
   TestBit.is_true(strcmp(fmt(0, 10, 0, "rc", 176), "0.10.0-rc+176") == 0,
                   "VER01: tag before build: 0.10.0-rc+176");
   TestBit.is_true(strcmp(fmt(0, 10, 0, "rc", 0), "0.10.0-rc") == 0, "VER01: build 0 is omitted");
   TestBit.is_true(strcmp(fmt(0, 10, 0, "rc", -1), "0.10.0-rc") == 0, "VER01: a negative build is omitted");
   TestBit.is_true(strcmp(fmt(1, 2, 3, "", 5), "1.2.3+5") == 0, "VER01: an empty tag has no '-'");
   TestBit.is_true(strcmp(fmt(1, 2, 3, NULL, 0), "1.2.3") == 0, "VER01: NULL tag and no build is just x.y.z");
   TestBit.is_true(strcmp(fmt(12, 345, 6789, "beta.1", 2000000), "12.345.6789-beta.1+2000000") == 0,
                   "VER01: multi-digit parts and a dotted tag");
   char tiny[8];
   anvl_format_version(tiny, sizeof(tiny), 0, 10, 0, "rc", 176);
   TestBit.is_true(strlen(tiny) == 7 && strncmp(tiny, "0.10.0-", 7) == 0, "VER01: truncates, NUL-terminated");
}

// What this build actually reports must be valid semver and agree with the constants.
static void test_ver02_reported_version(void) {
   const char *v = Anvl.get_version();
   int major = -1, minor = -1, patch = -1, n = 0;
   TestBit.is_true(sscanf(v, "%d.%d.%d%n", &major, &minor, &patch, &n) == 3 && n > 4,
                   "VER02: starts with MAJOR.MINOR.PATCH");
   TestBit.is_true(major == ANVL_VERSION_MAJOR && minor == ANVL_VERSION_MINOR && patch == ANVL_VERSION_PATCH,
                   "VER02: the numbers are the constants");
   const char *rest = v + n;
   TestBit.is_true(strncmp(rest, "-" ANVL_VERSION_TAG, 1 + strlen(ANVL_VERSION_TAG)) == 0,
                   "VER02: the tag follows the numbers, before any +BUILD");
   const char *plus = strchr(v, '+');
   if (ANVL_BUILD > 0) {
      char want[16];
      snprintf(want, sizeof(want), "+%d", ANVL_BUILD);
      TestBit.is_true(plus && strcmp(plus, want) == 0 && strchr(plus + 1, '-') == NULL,
                      "VER02: +BUILD is last, and is this build's number");
   } else {
      TestBit.is_true(plus == NULL, "VER02: an unknown build number leaves no '+0'");
   }
   // ANVL_VERSION_STR is the reported string minus the build metadata.
   size_t head = plus ? (size_t)(plus - v) : strlen(v);
   TestBit.is_true(strlen(ANVL_VERSION_STR) == head && strncmp(ANVL_VERSION_STR, v, head) == 0,
                   "VER02: ANVL_VERSION_STR equals the reported version minus +BUILD");
}

int main(void) {
   TestBit.run_ex("VER01_format", NULL, test_ver01_format, NULL);
   TestBit.run_ex("VER02_reported_version", NULL, test_ver02_reported_version, NULL);
   printf("Anvil %s\n", Anvl.get_version());
   return TestBit.report();
}
