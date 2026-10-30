#!/bin/sh
# Tests the writer-script mode of anvil-dump against hand-written scripts (test/infra/writer-script/).
# For each <name>.wscript: <name>.wscript.expected is the exact stdout of --writer-script; .raw the exact stdout of
# --writer-output; .noraw means --writer-output prints nothing and exits 1; .err means the run prints nothing on
# stdout, exactly that on stderr, and exits 2. Usage: check-writer-script.sh <anvil-dump>
set -u
dump=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root/test/infra/writer-script" || exit 2
tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT INT TERM
n=0; bad=0
fail() { echo "FAIL $1: $2"; bad=$((bad + 1)); }
for s in *.wscript; do
   if [ -f "$s.expected" ]; then
      n=$((n + 1))
      "$dump" --writer-script "$s" > "$tmp/out" 2> "$tmp/err"; rc=$?
      [ $rc -eq 0 ] || fail "$s" "exit $rc"
      cmp -s "$tmp/out" "$s.expected" || { fail "$s" "run differs"; diff "$s.expected" "$tmp/out" | head -6; }
      [ -s "$tmp/err" ] && fail "$s" "stderr: $(head -c 200 "$tmp/err")"
   fi
   if [ -f "$s.raw" ]; then
      n=$((n + 1))
      "$dump" --writer-output "$s" > "$tmp/out" 2> "$tmp/err"; rc=$?
      [ $rc -eq 0 ] || fail "$s" "--writer-output exit $rc"
      cmp -s "$tmp/out" "$s.raw" || fail "$s" "--writer-output bytes differ"
   fi
   if [ -f "$s.noraw" ]; then
      n=$((n + 1))
      "$dump" --writer-output "$s" > "$tmp/out" 2> "$tmp/err"; rc=$?
      [ $rc -eq 1 ] || fail "$s" "--writer-output exit $rc, wanted 1"
      [ -s "$tmp/out" ] && fail "$s" "--writer-output printed bytes with no output"
   fi
   if [ -f "$s.err" ]; then
      n=$((n + 1))
      "$dump" --writer-script "$s" > "$tmp/out" 2> "$tmp/err"; rc=$?
      [ $rc -eq 2 ] || fail "$s" "exit $rc, wanted 2"
      [ -s "$tmp/out" ] && fail "$s" "a script error printed on stdout"
      cmp -s "$tmp/err" "$s.err" || { fail "$s" "stderr differs"; diff "$s.err" "$tmp/err" | head -4; }
   fi
done
# a script that cannot be read
n=$((n + 1))
"$dump" --writer-script "$tmp/no-such-file" > "$tmp/out" 2> "$tmp/err"; rc=$?
[ $rc -eq 2 ] || fail "missing script" "exit $rc, wanted 2"
if [ $bad -ne 0 ]; then echo "check-writer-script FAILED: $bad failure(s) in $n checks"; exit 1; fi
echo "check-writer-script OK: $n checks"
