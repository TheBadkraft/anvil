#!/usr/bin/env bash
# check-readme.sh <package-dir>  -- a shipped README must not link outside the package (`](../x)`):
# those paths are sibling checkouts that don't exist in an installed tarball.
set -eu
readme=${1:?package dir}/README.md
[ -f "$readme" ] || { echo "FAIL readme: $readme missing"; exit 1; }
if bad=$(grep -nE '\]\((\./)?\.\./' "$readme"); then
  echo "FAIL readme: links outside the package:"; echo "$bad" | sed 's/^/   /'; exit 1
fi
echo "PASS readme: no ../ links"
