#!/bin/sh
# Tests the builder-script mode of anvil-dump against hand-written scripts (test/infra/builder-script/).
# For each <name>.bscript: <name>.bscript.expected is the exact stdout of --builder-script; <name>.bscript.emit<k> the
# exact bytes of the k-th file --emit-dir writes (and no other file may appear); .err means the run prints nothing on
# stdout, exactly that on stderr, and exits 2. Usage: check-builder-script.sh <anvil-dump>
set -u
dump=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root/test/infra/builder-script" || exit 2
tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT INT TERM
n=0; bad=0
fail() { echo "FAIL $1: $2"; bad=$((bad + 1)); }
for s in *.bscript; do
   if [ -f "$s.expected" ]; then
      n=$((n + 1))
      rm -rf "$tmp/e"; mkdir "$tmp/e"
      "$dump" --builder-script "$s" --emit-dir "$tmp/e" > "$tmp/out" 2> "$tmp/err"; rc=$?
      [ $rc -eq 0 ] || fail "$s" "exit $rc"
      cmp -s "$tmp/out" "$s.expected" || { fail "$s" "run differs"; diff "$s.expected" "$tmp/out" | head -6; }
      [ -s "$tmp/err" ] && fail "$s" "stderr: $(head -c 200 "$tmp/err")"
      k=0
      for want in "$s".emit*; do
         [ -f "$want" ] || continue
         k=$((k + 1))
         cmp -s "$tmp/e/emit-$k.anvl" "$s.emit$k" || fail "$s" "emit-$k.anvl differs"
      done
      have=$(ls "$tmp/e" | wc -l)
      [ "$have" -eq "$k" ] || fail "$s" "--emit-dir wrote $have file(s), wanted $k"
      # without --emit-dir the run is the same
      "$dump" --builder-script "$s" > "$tmp/out2" 2> /dev/null
      cmp -s "$tmp/out" "$tmp/out2" || fail "$s" "the run depends on --emit-dir"
   fi
   if [ -f "$s.err" ]; then
      n=$((n + 1))
      "$dump" --builder-script "$s" > "$tmp/out" 2> "$tmp/err"; rc=$?
      [ $rc -eq 2 ] || fail "$s" "exit $rc, wanted 2"
      [ -s "$tmp/out" ] && fail "$s" "a script error printed on stdout"
      cmp -s "$tmp/err" "$s.err" || { fail "$s" "stderr differs"; diff "$s.err" "$tmp/err" | head -4; }
   fi
done
# a script that cannot be read
n=$((n + 1))
"$dump" --builder-script "$tmp/no-such-file" > "$tmp/out" 2> "$tmp/err"; rc=$?
[ $rc -eq 2 ] || fail "missing script" "exit $rc, wanted 2"
# --emit-dir needs --builder-script
n=$((n + 1))
"$dump" --emit-dir "$tmp" x.bscript > "$tmp/out" 2> "$tmp/err"; rc=$?
[ $rc -eq 2 ] || fail "--emit-dir alone" "exit $rc, wanted 2"
if [ $bad -ne 0 ]; then echo "check-builder-script FAILED: $bad failure(s) in $n checks"; exit 1; fi
echo "check-builder-script OK: $n checks"
