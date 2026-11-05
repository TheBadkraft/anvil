#!/usr/bin/env bash
# check-v014.sh core <extracted-core-dir> | js <anvil.js-tarball>   (v0.14.0-rc additions; run on the PACKAGED files)
#   core: 174 exports; nested base `s1 := { f3 : s1 { a := 1; }; };` fails at load with the 4022 message (the probe runs under
#         `timeout`, so an endless walk is a FAIL); writer/builder code 23; block-form output and codes 21 and 22;
#         anvil_error_get_source names the included file; an include error is at the `include` keyword.
#   js:   the same, through `npm install <tarball>` in an empty directory (probes/probe-js-v014.js).
set -u
kind=${1:?core|js}; arg=${2:?package}; here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d /tmp/anvl-release-v014.XXXXXX); trap 'rm -rf "$work"' EXIT
rc=0
ck() { if [ "$2" = "$3" ]; then echo "PASS v014 $kind: $1"; else echo "FAIL v014 $kind: $1: got '$2', want '$3'"; rc=1; fi; }
case $kind in
  core)
    pkg=$(cd "$arg" && pwd)
    ex=$(sh "$here/../check-exports.sh" "$pkg/lib/libanvil.so" "$pkg/include" 2>&1); echo "$ex" | tail -1
    case $ex in *"174 public symbols"*) echo "PASS v014 core: 174 exports";; *) echo "FAIL v014 core: export count is not 174"; rc=1;; esac
    cc "$here/probes/probe-core-v014.c" -I"$pkg/include" -L"$pkg/lib" -lanvil -Wl,-rpath,"$pkg/lib" -o "$work/p" || { echo "FAIL v014 core: build probe"; exit 2; }
    out=$(timeout 30 "$work/p" "$work/s" 2>&1) || { echo "FAIL v014 core: probe exited $? (30s limit: an endless walk is a failure)"; exit 1; }
    f() { printf '%s\n' "$out" | sed -n "s/^$1=//p" | head -1; }
    ck "nested base (the loop reproduction) is 4022 at once" "$(f nested1)" "Only a top-level statement can have a base"
    ck "nested base in a later object statement" "$(f nested2)" "Only a top-level statement can have a base"
    ck "a top-level base still loads" "$(f topbase)" "<parsed without error>"
    ck "writer: base inside an object fails" "$(f writer_nested_ok)/$(f writer_nested_code)" "0/23"
    ck "writer: block statement with a scalar is 21" "$(f writer_block_scalar_ok)/$(f writer_block_scalar_code)" "0/21"
    ck "writer: base naming a block statement is 22" "$(f writer_base_block_ok)/$(f writer_base_block_code)" "0/22"
    ck "writer: block form text" "$(f writer_block_text)" '#!aml\n\nro {\n   a := 1;\n};\n'
    ck "writer: block form output reloads" "$(f writer_block_reloads)" 1
    ck "builder: set_base on a nested member is 23 at once" "$(f builder_nested_ok)/$(f builder_nested_code)" "0/23"
    ck "builder: set_block on a scalar is 21" "$(f builder_block_scalar_ok)/$(f builder_block_scalar_code)" "0/21"
    ck "builder: a base naming a block member is 22 at emit" "$(f builder_base_block_emit_null)/$(f builder_base_block_code)" "1/22"
    ck "include error in an included file: source names it" "$(f inc_source)" "$work/s/inc/a.anvl"
    ck "include error is at the include keyword (line 3, column 3)" "$(f inc_line):$(f inc_col)" "3:3"
    ck "include error in the root: source is empty, line 3 column 5" "$(f root_source) $(f root_line):$(f root_col)" "[] 3:5" ;;
  js)
    tgz=$(readlink -f "$arg"); mkdir "$work/proj" && cd "$work/proj" && npm init -y >/dev/null 2>&1
    npm install --no-audit --no-fund "$tgz" >npm.log 2>&1 || { cat npm.log; echo "FAIL v014 js: npm install"; exit 1; }
    cp "$here/probes/probe-js-v014.js" "$work/proj/probe.js"; timeout 60 node "$work/proj/probe.js" "$work/s" || { echo "(probe exit $?; 124 means an endless walk)"; rc=1; } ;;
  *) echo "unknown kind" >&2; exit 2 ;;
esac
exit $rc
