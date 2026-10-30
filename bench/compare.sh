#!/bin/sh
# Usage: bench/compare.sh ROUNDS CPU label=path/to/bench_binary ...
# Runs the binaries interleaved (A B C A B C ...) pinned to one CPU, ROUNDS times, and prints the
# median-of-medians ms per workload and the ratio against the first binary.
# Build one binary per commit: git worktree add --detach W <commit>; make -C W lib-release;
#   gcc -O2 -std=c2x -IW/include -D_POSIX_C_SOURCE=200809L bench/parse_validate.c W/lib/release/libanvil.a -o bin
rounds=$1; cpu=$2; shift 2
out=$(mktemp -d)
r=0
while [ $r -lt $rounds ]; do
   for spec in "$@"; do
      label=${spec%%=*}; bin=${spec#*=}
      taskset -c "$cpu" "$bin" >> "$out/$label.txt"
   done
   r=$((r+1))
done
first=${1%%=*}
for w in parse_floats parse_ints parse_mixed validate_floats validate_small; do
   base=$(grep "^$w " "$out/$first.txt" | awk '{print $2}' | sort -n | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}')
   line=$(printf '%-16s' "$w")
   for spec in "$@"; do
      label=${spec%%=*}
      m=$(grep "^$w " "$out/$label.txt" | awk '{print $2}' | sort -n | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}')
      line="$line $label=$m($(echo "$m $base" | awk '{printf "%.3fx",$1/$2}'))"
   done
   echo "$line"
done
