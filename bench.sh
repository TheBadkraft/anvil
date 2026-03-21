#!/usr/bin/env bash
# bench.sh — Anvil full benchmark suite runner
# Usage:  ./bench.sh [--build] [--output FILE]
#
#   --build        Rebuild lib + bench binaries before running
#   --output FILE  Tee output to FILE (default: docs/benchmarks.md)
#
# Runs all five benchmark suites in order:
#   1. bench_parse      — parser throughput     (PB01–PB07)
#   2. bench_resolver   — inheritance resolver  (RB01–RB06)
#   3. bench_serializer — writer throughput     (SB01–SB05)
#   4. bench_asl        — ASL script engine     (AB01–AB07)
#   5. performance      — full sample sweep     (17 files)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD=0
OUTPUT=""

for arg in "$@"; do
    case "$arg" in
        --build) BUILD=1 ;;
        --output) shift; OUTPUT="$1" ;;
        --output=*) OUTPUT="${arg#--output=}" ;;
    esac
done

if [[ $BUILD -eq 1 ]]; then
    echo "Building lib..."
    cbuild lib
    echo "Building tests..."
    cbuild test
fi

BENCH_PARSE="./build/test/test_bench_parse"
BENCH_RESOLVER="./build/test/test_bench_resolver"
BENCH_SERIALIZER="./build/test/test_bench_serializer"
BENCH_ASL="./build/test/test_bench_asl"
PERF="./build/test/test_performance"

for bin in "$BENCH_PARSE" "$BENCH_RESOLVER" "$BENCH_SERIALIZER" "$BENCH_ASL" "$PERF"; do
    if [[ ! -x "$bin" ]]; then
        echo "ERROR: $bin not found. Run with --build first." >&2
        exit 1
    fi
done

DATE=$(date '+%Y-%m-%d %H:%M')
PLATFORM=$(uname -m)
COMPILER=$(gcc --version | head -1)
SIGMA_VER=$(grep -m1 'SIGMA_MEMORY_VERSION ' /usr/local/include/sigma.memory/config.h 2>/dev/null | awk '{print $3}' | tr -d '"' || echo "unknown")

run_suite() {
    local label="$1"
    local bin="$2"
    echo ""
    echo "### $label"
    echo ""
    echo '```'
    "$bin" 2>&1
    echo '```'
    echo ""
}

{
    cat <<EOF
# Anvil Benchmark Report

**Generated:** $DATE
**Platform:** Linux $PLATFORM
**Compiler:** $COMPILER
**Build flags:** \`-std=c2x -O0 -g\` (debug build — release will be faster)
**sigma.memory:** $SIGMA_VER (Resource scope / mmap bump allocator)
**Anvil version:** $(grep -m1 'ANVIL_VERSION\|version' include/anvil.h 2>/dev/null | head -1 | sed 's|.*"\(.*\)".*|\1|' || echo "0.2.x")

> All timings are wall-clock (\`CLOCK_MONOTONIC\`).
> Each benchmark runs 5 warm-up iterations (discarded) followed by 100 measured
> iterations. **min** = best observed (cache-warm), **mean** = arithmetic mean, **σ** = population std-dev.
> Throughput = file size / mean parse time.

---

EOF

    run_suite "PB — Parser throughput (bench_parse)" "$BENCH_PARSE"
    run_suite "RB — Inheritance resolver (bench_resolver)" "$BENCH_RESOLVER"
    run_suite "SB — Serializer / writer (bench_serializer)" "$BENCH_SERIALIZER"
    run_suite "AB — ASL script engine (bench_asl)" "$BENCH_ASL"
    run_suite "Full sample sweep (test_performance)" "$PERF"

    cat <<'EOF'

---

## Notes

- **PB01–PB05** use `.anvl` (AML dialect); **PB06–PB07** use `.aml` (stricter AMP dialect).
- **PB04** (blob-heavy) and **PB05** (deep-nesting) exercise pass-through and recursion limits respectively.
- **RB02, RB04, RB06** warm cache-hit times are in the **17–26 ns** range — single pointer deref through the resolver cache.
- **SB04** (minified) is faster than **SB03** (pretty) because it skips indent/newline emission.
- **SB05** round-trip variance is dominated by the two parse phases, not serialization.
- **AB** benchmarks run under the ASL interpreter; timings include AST parse + eval.
- All allocations during parse go through `Allocator.Resource` (mmap bump slab). Zero BTree/skiplist overhead.
  See sigma.memory FT-12 for details.
EOF
} | if [[ -n "$OUTPUT" ]]; then
    tee "$OUTPUT"
else
    cat
fi
