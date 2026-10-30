#!/bin/sh
# Fast decimal conversion vs strtod (BR-2610-anvl-parser-004). Builds and runs, against lib/release:
#   edge.c  - hand-picked hard literals through the public API
#   fuzz.c  - generated literals through the public API (seeds 1..FUZZ_SEEDS, FUZZ_N each)
#   x87.c   - the internal fast path directly, in every rounding mode, built twice: SSE, and
#             -mfpmath=387 where the FLT_EVAL_METHOD gate must send everything to strtod
# Usage: sh scripts/check-numeric-fastpath.sh   (FUZZ_SEEDS=4 FUZZ_N=2000000 by default)
set -e
cd "$(dirname "$0")/.."
D=test/infra/numeric_fastpath
B=build/numeric-fastpath
mkdir -p $B
make -s lib-release >/dev/null
CF="-O2 -std=c2x -Iinclude -D_POSIX_C_SOURCE=200809L"
gcc $CF $D/edge.c lib/release/libanvil.a -lm -o $B/edge
gcc $CF $D/fuzz.c lib/release/libanvil.a -lm -o $B/fuzz
gcc $CF $D/x87.c -lm -o $B/x87_sse
$B/edge
s=1
while [ $s -le "${FUZZ_SEEDS:-4}" ]; do $B/fuzz $s "${FUZZ_N:-2000000}"; s=$((s+1)); done
$B/x87_sse
if gcc $CF -mfpmath=387 $D/x87.c -lm -o $B/x87_387 2>/dev/null; then
   $B/x87_387
else
   echo "x87 build SKIPPED: no -mfpmath=387 here"
fi
echo "check-numeric-fastpath OK"
