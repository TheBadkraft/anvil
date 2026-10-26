#!/usr/bin/env bash
# Grammar + version check for one release artifact (BR-2610-anvl-distribution-001).
#
#   check-grammar.sh <core|node|wasm|net|py> <extracted-package-dir> <release-version> [build-count]
#
# <release-version> is MAJOR.MINOR.PATCH-TAG (e.g. 0.10.0-rc). The native library reports it as
# semver, MAJOR.MINOR.PATCH-TAG+BUILD (BR-2610-anvl-version-001), where BUILD is the commit
# count of the core release tag, computed once by the release and passed to every build. The probe
# runs against the *extracted tarball* (never a build tree), requiring:
#   1. the reported version is exactly <release-version>+<build-count>, with build-count > 0 and, when
#      [build-count] is given, equal to it. A string with no +BUILD (or +0) cannot identify the
#      source, so it fails. LEGACY_VERSION_FORMAT=1 reads the pre-fix MAJOR.MINOR.PATCH+BUILD-TAG
#      form; only audit-published.sh sets it, for the old 0.8.0-rc files;
#   2. `include "missing.anvl";` fails with an include-not-found error, never "Expected ':='"
#      (that is the pre-rename grammar, where `include` was a plain identifier);
#   3. `name := "David";` parses and reads back as David.
# Exit 0 only if all three hold.
set -u
kind=${1:?kind}; pkg=$(cd "${2:?package dir}" && pwd); want=${3:?release version}; wantbuild=${4:-}
if [[ $want =~ ^([0-9]+\.[0-9]+\.[0-9]+)-(.+)$ ]]; then
  base=${BASH_REMATCH[1]}; tag=${BASH_REMATCH[2]}
  if [ "${LEGACY_VERSION_FORMAT:-0}" = 1 ]; then wantre="^${base//./\\.}\\+([0-9]+)-${tag}$"
  else wantre="^${base//./\\.}-${tag}\\+([0-9]+)$"; fi
else echo "bad release version: $want" >&2; exit 2; fi
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT

case $kind in
  core) cc "$here/probes/probe-core.c" -I"$pkg/include" -L"$pkg/lib" -lanvil \
           -Wl,-rpath,"$pkg/lib" -o "$work/probe" || { echo "FAIL build probe"; exit 2; }
        out=$("$work/probe" 2>&1) ;;
  node) out=$(node "$here/probes/probe-node.js" "$pkg" 2>&1) ;;
  wasm) out=$(node "$here/probes/probe-wasm.js" "$pkg" 2>&1) ;;
  py)   out=$(python3 -I "$here/probes/probe-py.py" "$pkg" 2>&1) ;;
  net)  cp -r "$here/probes/probe-net" "$work/net"
        out=$(cd "$work/net" && dotnet run -c Release -p:AnvilNetLib="$pkg/lib" 2>&1) ;;
  *) echo "unknown kind: $kind" >&2; exit 2 ;;
esac

field() { printf '%s\n' "$out" | sed -n "s/^$1=//p" | head -1; }
version=$(field version); include=$(field include); name=$(field name)
rc=0
if [[ $version =~ $wantre ]] && [ "${BASH_REMATCH[1]}" -gt 0 ] && { [ -z "$wantbuild" ] || [ "${BASH_REMATCH[1]}" = "$wantbuild" ]; }
then echo "PASS version: $version"
else echo "FAIL version: reported '${version:-<none>}', expected $want+${wantbuild:-N>0}"; rc=1; fi

if [ -n "$include" ] && [[ $include != *"Expected ':='"* ]] && [[ ${include,,} == *"include"*"not found"* || ${include,,} == *"not found"* ]]
then echo "PASS include: $include"
else echo "FAIL include: '${include:-<no output>}' (want an include-not-found error)"; rc=1; fi

if [ "$name" = "David" ]; then echo "PASS name: David"
else echo "FAIL name: '${name:-<none>}'"; rc=1; fi
[ $rc -ne 0 ] && [ -z "$version$include$name" ] && printf '%s\n' "$out" | head -5
exit $rc
