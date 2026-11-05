#!/usr/bin/env bash
# bench-native.sh <core-repo> <old-tag> <new-tag> [rounds=9] [cpu=2]
# Native vs native, both built from clean exports of their tags with ANVL_BUILD fixed, the SAME benchmark sources compiled
# against each (static libanvil.a and libanvil.so, release). Prints time ratios new/old (lower is faster):
#   warm:  bench/parse_validate.c (parse_floats parse_ints parse_mixed validate_floats validate_small), rounds interleaved, one CPU
#   small: 20000 loads of a five-line document (median of 3 runs); cold: the first load in a fresh process (median of 31)
# Run it under the systemd memory cap. Everything is built and run under /tmp and removed on exit.
set -eu
repo=$(cd "${1:?core repo}" && pwd); old=${2:?old tag}; new=${3:?new tag}; rounds=${4:-9}; cpu=${5:-2}
here=$(cd "$(dirname "$0")" && pwd); work=$(mktemp -d /tmp/anvl-bench-native.XXXXXX); trap 'rm -rf "$work"' EXIT
for t in $old $new; do
  mkdir "$work/$t"; git -C "$repo" archive "$t" | tar -x -C "$work/$t"
  b=$(git -C "$repo" rev-list --count "$t")
  make -C "$work/$t" -j4 lib-release so-release ANVL_BUILD="$b" >"$work/$t.log" 2>&1 || { tail "$work/$t.log"; exit 1; }
done
src=$(mktemp -d "$work/src.XXXXXX"); git -C "$repo" archive "$new" bench/parse_validate.c | tar -x -C "$src"
CF="-O2 -std=c2x -D_POSIX_C_SOURCE=200809L"
for t in $old $new; do
  inc="$work/$t/include"
  gcc $CF -I"$inc" "$src/bench/parse_validate.c" "$work/$t/lib/release/libanvil.a" -o "$work/pv-static-$t"
  gcc $CF -I"$inc" "$src/bench/parse_validate.c" -L"$work/$t/lib/release" -lanvil -Wl,-rpath,"$work/$t/lib/release" -o "$work/pv-so-$t"
  gcc $CF -I"$inc" "$here/probes/bench-small.c" "$work/$t/lib/release/libanvil.a" -o "$work/sm-static-$t"
  gcc $CF -I"$inc" "$here/probes/bench-small.c" -L"$work/$t/lib/release" -lanvil -Wl,-rpath,"$work/$t/lib/release" -o "$work/sm-so-$t"
done
med() { sort -n | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}'; }
for kind in static so; do
  echo "== warm, $kind libanvil, $new vs $old, $rounds interleaved rounds on CPU $cpu (ratio = $new / $old, time, lower is faster)"
  sh "$repo/bench/compare.sh" "$rounds" "$cpu" "old=$work/pv-$kind-$old" "new=$work/pv-$kind-$new"
  for t in $old $new; do
    for r in 1 2 3; do taskset -c "$cpu" "$work/sm-$kind-$t" | sed -n 's/^small_ns //p'; done | med > "$work/small-$kind-$t"
    for i in $(seq 31); do taskset -c "$cpu" "$work/sm-$kind-$t" | sed -n 's/^cold_ns //p'; done | med > "$work/cold-$kind-$t"
  done
  for what in small cold; do
    o=$(cat "$work/$what-$kind-$old"); n=$(cat "$work/$what-$kind-$new")
    echo "$what ($kind): $old $o ns, $new $n ns, ratio $(echo "$n $o" | awk '{printf "%.3fx",$1/$2}')"
  done
done
