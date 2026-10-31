#!/bin/sh
# Regenerates every expected dump in conformance/cases with anvil-dump and diffs it against the committed
# one (check), or rewrites them (update). Usage: check-conformance.sh check|update <anvil-dump>
#
# Every file under conformance/cases that is not a ".dump" is an input and has its dump beside it
# ("<input>.dump"). A ".frag" input is dumped in value-fragment mode, a ".wscript" (cases/writer/) is run with
# --writer-script, a ".bscript" (cases/builder/) with --builder-script. Run from the repo root.
# check fails on: a dump that differs, an input without a dump, a dump without an input, a corpus copy of
# a test/fixtures file that differs from it. update writes every dump (new ones included) and removes
# none. Inputs are run from conformance/cases, with relative paths, so includes resolve there.
set -u
mode=$1
dump=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root/conformance/cases" || exit 2
tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT INT TERM
total=0; bad=0; changed=0

# the fixtures are seeded verbatim from test/fixtures and must stay identical to it
if [ -d "$root/test/fixtures" ]; then
   if ! diff -rq -x "*.dump" "$root/test/fixtures" fixtures >"$tmp/fx" 2>&1; then
      if [ "$mode" = check ]; then echo "FAIL corpus fixtures/ differs from test/fixtures:"; head -5 "$tmp/fx"; bad=$((bad + 1)); fi
   fi
fi

find . -type f ! -name '*.dump' | sed 's|^\./||' | LC_ALL=C sort > "$tmp/inputs"
while IFS= read -r input; do
   expected="$input.dump"
   opts=
   case $input in *.frag) opts=--fragment;; *.wscript) opts=--writer-script;; *.bscript) opts=--builder-script;; esac
   total=$((total + 1))
   "$dump" $opts "$input" > "$tmp/out" 2> "$tmp/err"
   rc=$?
   if [ -s "$tmp/err" ] || [ $rc -ne 0 ]; then
      echo "FAIL $input: anvil-dump exited $rc: $(head -c 200 "$tmp/err")"; bad=$((bad + 1)); continue
   fi
   if [ "$mode" = update ]; then
      cmp -s "$tmp/out" "$expected" 2>/dev/null || { cp "$tmp/out" "$expected"; changed=$((changed + 1)); }
   elif [ ! -f "$expected" ]; then
      echo "MISSING dump for $input (run make update-conformance alongside a deliberate change)"; bad=$((bad + 1))
   elif ! cmp -s "$tmp/out" "$expected"; then
      echo "DIFF $input"; diff "$expected" "$tmp/out" | head -${DIFF_LINES:-8}; bad=$((bad + 1))
   fi
done < "$tmp/inputs"

find . -type f -name '*.dump' | sed 's|^\./||; s|\.dump$||' | LC_ALL=C sort > "$tmp/dumped"
orphans=$(comm -13 "$tmp/inputs" "$tmp/dumped")
if [ -n "$orphans" ]; then
   if [ "$mode" = update ]; then for o in $orphans; do rm -f "$o.dump"; done; echo "update-conformance: removed dumps with no input: $orphans"
   else echo "ORPHAN dump(s) with no input: $orphans"; bad=$((bad + 1)); fi
fi

if [ "$mode" = update ]; then echo "update-conformance: $total cases, $changed rewritten, $bad failed"; [ $bad -eq 0 ]; exit; fi
if [ $bad -ne 0 ]; then echo "check-conformance FAILED: $bad problem(s) in $total cases"; exit 1; fi
echo "check-conformance OK: $total cases match native"
