#!/bin/sh
# The fuzz harness must stay bounded: 200,000 cases, native against itself, under a 1 GiB cgroup limit (no swap, 300 s),
# in the foreground. Reports the peak RSS of the harness process. Fails if the run is killed or finds a difference.
# Usage: conformance/fuzz/test-memory.sh <anvil-dump>
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
dump=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
log=$(mktemp) || exit 2
trap 'rm -f "$log"' EXIT INT TERM
systemd-run --user --scope -q -p MemoryMax=1G -p MemorySwapMax=0 -p RuntimeMaxSec=300 -- \
   /usr/bin/time -v python3 "$root/conformance/fuzz/fuzz.py" --a "$dump" --b "$dump" --cases 200000 --seed 31 --jobs 4 > "$log" 2>&1
rc=$?
grep -E '^fuzz:|Maximum resident' "$log" | cut -c1-80
[ $rc -eq 0 ] && grep -q 'differences=0' "$log" && echo "fuzz memory test OK (200000 cases under MemoryMax=1G)" || { echo "fuzz memory test FAILED (exit $rc)"; tail -5 "$log"; exit 1; }
