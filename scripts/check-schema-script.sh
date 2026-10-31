#!/bin/sh
# Tests the schema-script mode of anvil-dump against hand-written scripts (test/infra/schema-script/).
# For each <name>.sscript: <name>.sscript.expected is the exact stdout of --schema-script; .err means the run prints
# nothing on stdout, exactly that on stderr, and exits 2. Usage: check-schema-script.sh <anvil-dump>
set -u
dump=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root/test/infra/schema-script" || exit 2
tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT INT TERM
n=0; bad=0
fail() { echo "FAIL $1: $2"; bad=$((bad + 1)); }
for s in *.sscript; do
   if [ -f "$s.expected" ]; then
      n=$((n + 1))
      "$dump" --schema-script "$s" > "$tmp/out" 2> "$tmp/err"; rc=$?
      [ $rc -eq 0 ] || fail "$s" "exit $rc"
      cmp -s "$tmp/out" "$s.expected" || { fail "$s" "run differs"; diff "$s.expected" "$tmp/out" | head -6; }
      [ -s "$tmp/err" ] && fail "$s" "stderr: $(head -c 200 "$tmp/err")"
   fi
   if [ -f "$s.err" ]; then
      n=$((n + 1))
      "$dump" --schema-script "$s" > "$tmp/out" 2> "$tmp/err"; rc=$?
      [ $rc -eq 2 ] || fail "$s" "exit $rc, wanted 2"
      [ -s "$tmp/out" ] && fail "$s" "a script error printed on stdout"
      cmp -s "$tmp/err" "$s.err" || { fail "$s" "stderr differs"; diff "$s.err" "$tmp/err" | head -4; }
   fi
done
# the run leaves nothing behind in the temporary directory
n=$((n + 1))
mkdir "$tmp/t"
TMPDIR="$tmp/t" "$dump" --schema-script t01_minimal.sscript > /dev/null 2>&1
[ -z "$(ls -A "$tmp/t")" ] || fail "tmpdir" "the run left files in TMPDIR: $(ls -A "$tmp/t")"
# a script that cannot be read
n=$((n + 1))
"$dump" --schema-script "$tmp/no-such-file" > "$tmp/out" 2> "$tmp/err"; rc=$?
[ $rc -eq 2 ] || fail "missing script" "exit $rc, wanted 2"
# --schema-script excludes the other modes
n=$((n + 1))
"$dump" --schema-script --builder-script x.sscript > "$tmp/out" 2> "$tmp/err"; rc=$?
[ $rc -eq 2 ] || fail "two modes" "exit $rc, wanted 2"
if [ $bad -ne 0 ]; then echo "check-schema-script FAILED: $bad failure(s) in $n checks"; exit 1; fi
echo "check-schema-script OK: $n checks"
