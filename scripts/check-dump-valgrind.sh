#!/bin/sh
# Runs anvil-dump under valgrind (leak check full, errors fatal) over every corpus input (or the ones named
# after the dump path) and fails on any leak or memory error. Usage: check-dump-valgrind.sh <anvil-dump> [input...]
set -u
dump=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); shift
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root/conformance/cases" || exit 2
if [ $# -eq 0 ]; then set -- $(find . -type f ! -name '*.dump' | sed 's|^\./||' | sort); fi
tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT INT TERM
n=0; bad=0
for input in "$@"; do
   opts=; case $input in *.frag) opts=--fragment;; *.wscript) opts=--writer-script;; *.bscript) opts=--builder-script;; *.sscript) opts=--schema-script;; esac
   n=$((n + 1))
   valgrind -q --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=9 \
      "$dump" $opts "$input" > /dev/null 2> "$tmp/vg"
   rc=$?
   if [ $rc -ne 0 ] || [ -s "$tmp/vg" ]; then echo "VALGRIND $input (exit $rc)"; head -12 "$tmp/vg"; bad=$((bad + 1)); fi
done
if [ $bad -ne 0 ]; then echo "check-dump-valgrind FAILED: $bad of $n inputs"; exit 1; fi
echo "check-dump-valgrind OK: $n inputs, no leaks, no errors"
