#!/bin/sh
# check-cxx.sh <include dir> <lib dir holding libanvil.so and libanvil.a> <probe dir>
#
# A C++ consumer must be able to include every shipped public header and link against the library.
# Without `extern "C"` the header compiles but the symbols are looked up under C++ mangled names and the
# link fails, so this gate links and runs real programs, not just -fsyntax-only.
#
# Headers are found by glob (<include dir>/anvil_*.h), so a new public header is checked automatically.
# Each one needs a probe <probe dir>/<header stem>.cpp that includes ONLY that header and calls at least
# one function or vtable object it declares (types-only headers: uses its types). A header without a probe
# fails the gate. Every probe is built with g++ and, if installed, clang++, at -std=c++11 and -std=c++17,
# with -Wall -Wextra -pedantic -Werror, against libanvil.so (run) and libanvil.a (run). One more program
# includes all the public headers together. include/anvil.h is internal and not shipped, so not checked.
set -u
inc=${1:?include dir}; lib=${2:?lib dir}; probes=${3:?probe dir}
inc=$(cd "$inc" && pwd); lib=$(cd "$lib" && pwd); probes=$(cd "$probes" && pwd)
for f in "$lib/libanvil.so" "$lib/libanvil.a"; do [ -f "$f" ] || { echo "FAIL: $f not found"; exit 1; }; done
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
fail=0; runs=0
FLAGS="-Wall -Wextra -pedantic -Werror"

compilers=""
for c in g++ clang++; do
   if command -v "$c" >/dev/null 2>&1; then compilers="$compilers $c"; else echo "check-cxx: $c not installed, skipped"; fi
done
[ -n "$compilers" ] || { echo "FAIL: no C++ compiler (g++ or clang++) found"; exit 1; }

# build_run <compiler> <std> <source> <label>
build_run() {
   cxx=$1; std=$2; src=$3; label=$4
   for kind in so a; do
      out="$work/t.$kind"
      if [ $kind = so ]; then
         $cxx -std=$std $FLAGS -I"$inc" "$src" "$lib/libanvil.so" -Wl,-rpath,"$lib" -o "$out" 2>"$work/err"
      else
         $cxx -std=$std $FLAGS -I"$inc" "$src" "$lib/libanvil.a" -lpthread -lm -o "$out" 2>"$work/err"
      fi
      if [ $? -ne 0 ]; then echo "FAIL: $label [$cxx -std=$std, lib$kind]: build"; head -5 "$work/err" | sed 's/^/   /'; fail=1; continue; fi
      "$out" >"$work/out" 2>&1; rc=$?
      runs=$((runs + 1))
      [ $rc -eq 0 ] || { echo "FAIL: $label [$cxx -std=$std, lib$kind]: ran, exit $rc"; fail=1; }
   done
}

# all.cpp: every public header together, then call into the flat API and a vtable.
{
   for h in "$inc"/anvil_*.h; do echo "#include \"$(basename "$h")\""; done
   echo 'int main() { return (anvil_get_version() && Anvil.get_version() && Writer.create && Builder.create) ? 0 : 1; }'
} > "$work/all.cpp"

headers=0
for h in "$inc"/anvil_*.h; do
   stem=$(basename "$h" .h); headers=$((headers + 1))
   if [ ! -f "$probes/$stem.cpp" ]; then echo "FAIL: public header $stem.h has no probe $probes/$stem.cpp"; fail=1; continue; fi
   for cxx in $compilers; do for std in c++11 c++17; do build_run $cxx $std "$probes/$stem.cpp" "$stem.h"; done; done
done
for cxx in $compilers; do for std in c++11 c++17; do build_run $cxx $std "$work/all.cpp" "all headers together"; done; done

if [ $fail -ne 0 ]; then echo "check-cxx FAILED"; exit 1; fi
echo "check-cxx OK: $headers headers, $runs link-and-run programs ($(echo $compilers))"
