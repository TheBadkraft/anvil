#!/usr/bin/env bash
# check-v0141.sh core <extracted-core-dir> | walk <extracted-core-dir> <core-repo-at-the-tag> | js <anvil.js-tarball> | py <extracted-py-dir> | net <extracted-net-dir>
#   (v0.14.1-rc additions; run on the PACKAGED files. Every probe runs under `timeout`: an endless walk is a FAIL.)
#   A `$` reference that leads back to a value that contains it is refused at load (4054, category RESOLVE) at the documented
#   position (the `$` of the first on-cycle reference); `s1 := { a := $s1; };` is refused at once; the plain chain cycle
#   `a := $b; b := $a;` and a reference to a scalar still load; the builder refuses the graph at emit and write with writer
#   code 24. walk: the conformance-corpus walker (tools/anvil-walk.c) compiled against the packaged library walks every
#   document that loads clean and finds no loop.
set -u
kind=${1:?core|walk|js|py|net}; arg=${2:?package}; here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d /tmp/anvl-release-v0141.XXXXXX); trap 'rm -rf "$work"' EXIT
rc=0
ck() { if [ "$2" = "$3" ]; then echo "PASS v0141 $kind: $1"; else echo "FAIL v0141 $kind: $1: got '$2', want '$3'"; rc=1; fi; }
M="A reference leads back to a value that contains it"
case $kind in
  core)
    pkg=$(cd "$arg" && pwd)
    cc "$here/probes/probe-core-v0141.c" -I"$pkg/include" -L"$pkg/lib" -lanvil -Wl,-rpath,"$pkg/lib" -o "$work/p" || { echo "FAIL v0141 core: build probe"; exit 2; }
    out=$(timeout 30 "$work/p" 2>&1) || { echo "FAIL v0141 core: probe exited $? (30s limit)"; exit 1; }
    f() { printf '%s\n' "$out" | sed -n "s/^$1=//p" | head -1; }
    rcat=$(f resolve_category)
    for c in "self 2:14" "deep 2:21" "array 2:8" "pair 2:13" "viabase 2:13"; do set -- $c
      ck "$1: refused (resolver, 4054) at $2" "$(f ${1}_msg)/$(f ${1}_cat)/$(f ${1}_pos)" "$M/$rcat/$2"; done
    ck "a reference to a scalar loads" "$(f scalar_msg)" "<parsed without error>"
    ck "the plain chain cycle still loads" "$(f chain_msg)" "<parsed without error>"
    ck "builder: emit refuses with code 24" "$(f builder_emit_null)/$(f builder_emit_code)" "1/24"
    ck "builder: write refuses with code 24" "$(f builder_write_ok)/$(f builder_write_code)" "0/24"
    ck "builder: a reference to a scalar emits" "$(f builder_ok_emit)/$(f builder_ok_code)" "1/0"
    ck "streaming writer does not see it (documented)" "$(f streaming_writer_code)" "0" ;;
  walk)
    pkg=$(cd "$arg" && pwd); repo=$(cd "${3:?core repo at the tag}" && pwd)
    cc -std=c2x -O1 -I"$pkg/include" "$repo/tools/anvil-walk.c" -L"$pkg/lib" -lanvil -Wl,-rpath,"$pkg/lib" -o "$work/anvil-walk" || { echo "FAIL v0141 walk: build"; exit 2; }
    out=$(timeout 600 sh "$repo/scripts/check-walk-corpus.sh" "$work/anvil-walk" 2>&1); r=$?
    printf '%s\n' "$out" | tail -2
    [ $r -eq 0 ] && printf '%s\n' "$out" | grep -q " 0 loops" && echo "PASS v0141 walk: packaged core, corpus walked, 0 loops" || { echo "FAIL v0141 walk"; rc=1; } ;;
  js)
    tgz=$(readlink -f "$arg"); mkdir "$work/proj" && cd "$work/proj" && npm init -y >/dev/null 2>&1
    npm install --no-audit --no-fund "$tgz" >npm.log 2>&1 || { cat npm.log; echo "FAIL v0141 js: npm install"; exit 1; }
    cp "$here/probes/probe-js-v0141.js" probe.js; timeout 60 node probe.js || { echo "(probe exit $?; 124 means an endless walk)"; rc=1; } ;;
  py)
    pkg=$(cd "$arg" && pwd)
    PYTHONDONTWRITEBYTECODE=1 timeout 60 python3 -I "$here/probes/probe-py-v0141.py" "$pkg" || { echo "(probe exit $?)"; rc=1; } ;;
  net)
    pkg=$(cd "$arg" && pwd); cp -r "$here/probes/probe-net" "$work/net"; rm -rf "$work/net/bin" "$work/net/obj"
    cp "$here/probes/probe-net-v0141.cs.txt" "$work/net/Program.cs"
    (cd "$work/net" && timeout 300 dotnet run -c Release -p:AnvilNetLib="$pkg/lib" 2>&1) | grep -E "^(PASS|FAIL)|error" || true
    (cd "$work/net" && timeout 300 dotnet run -c Release --no-build -p:AnvilNetLib="$pkg/lib" >/dev/null 2>&1) || { echo "FAIL v0141 net: probe returned non-zero"; rc=1; } ;;
  *) echo "unknown kind" >&2; exit 2 ;;
esac
exit $rc
