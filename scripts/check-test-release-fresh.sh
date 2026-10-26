#!/bin/sh
# check-test-release-fresh.sh
#
# SR-2610-anvl-build-001: `make test_release` (test/unit) links lib/release/libanvil.a. It must never
# link a stale one, so the target has to ask the top-level make (which tracks sources and headers) to
# bring the archive up to date every time, and must not rebuild anything when nothing changed.
#
#  1. after touching a core source, test_release rebuilds the archive first (visible in the output);
#  2. with nothing changed, test_release recompiles no library object.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
make lib-release >/dev/null 2>&1 || { echo "FAIL: lib-release does not build"; exit 1; }
cd test/unit

# Make sure test_release itself is runnable first.
make test_release >/dev/null 2>&1 || { echo "FAIL: test_release does not build/run"; exit 1; }

# 2. nothing changed: no library object is recompiled.
out=$(make test_release 2>&1)
if echo "$out" | grep -q "build/release/.*\.o"; then
   echo "FAIL: an unchanged tree recompiled library objects:"; echo "$out" | grep "build/release/.*\.o" | head -3; exit 1
fi

# 1. touch a core source: test_release must rebuild the archive before linking.
touch "$root/src/core/errors.c"
out=$(make test_release 2>&1)
if ! echo "$out" | grep -q "build/release/core/errors.o"; then
   echo "FAIL: after touching src/core/errors.c, test_release did not rebuild the archive (it linked a stale one)"
   exit 1
fi
echo "test_release freshness OK: rebuilds when a source changes, does nothing when it hasn't"
