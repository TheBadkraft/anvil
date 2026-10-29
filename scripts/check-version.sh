#!/bin/sh
# check-version.sh - BR-2610-anvl-version-001, end to end through the real library.
#
# Builds the release archive in a scratch directory with different build-number inputs and asks the
# library what version it reports:
#   ANVL_BUILD=176   -> <x.y.z>-<tag>+176   (an explicit number wins, and the object is rebuilt for it)
#   ANVL_BUILD=0     -> <x.y.z>-<tag>       (0 is "unknown": no "+0")
#   (nothing)        -> <x.y.z>-<tag>+<git rev-list --count HEAD>
# and that the reported string is the constants' ANVL_VERSION_STR plus the optional +BUILD.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
scratch=build/version-check
rm -rf "$scratch"; mkdir -p "$scratch"
cat > "$scratch/v.c" <<'EOC'
#include "anvil_flat.h"
#include "constants.h"
#include <stdio.h>
int main(void) { printf("%s %s\n", anvil_get_version(), ANVL_VERSION_STR); return 0; }
EOC
build_and_ask() { # $1 = make arguments
   # shellcheck disable=SC2086
   make $1 RELEASE_DIR="$scratch/lib" RELEASE_OBJ="$scratch/obj" lib-release >/dev/null 2>&1 || { echo "FAIL: build ($1)"; exit 1; }
   gcc -std=c2x -Iinclude -I/usr/local/include "$scratch/v.c" "$scratch/lib/libanvil.a" -o "$scratch/v" || { echo "FAIL: link"; exit 1; }
   "$scratch/v"
}
check() { # $1 label, $2 "reported base", $3 expected suffix
   set -- "$1" "$2" "$3"
   reported=${2%% *}; base=${2##* }
   if [ "$reported" != "$base$3" ]; then echo "FAIL: $1: reported '$reported', expected '$base$3'"; exit 1; fi
}
out=$(build_and_ask "ANVL_BUILD=176");  check "ANVL_BUILD=176" "$out" "+176"
out=$(build_and_ask "ANVL_BUILD=0");    check "ANVL_BUILD=0" "$out" ""
count=$(git rev-list --count HEAD 2>/dev/null || echo 0)
suffix=""; [ "$count" -gt 0 ] && suffix="+$count"
out=$(build_and_ask "");                check "default (git count)" "$out" "$suffix"
out=$(ANVL_BUILD=7 build_and_ask "");   check "ANVL_BUILD from the environment" "$out" "+7"
out=$(build_and_ask "BUILD_NUM=9");     check "BUILD_NUM=9" "$out" "+9"
rm -rf "$scratch"
echo "version OK: semver MAJOR.MINOR.PATCH-TAG[+BUILD]; an explicit build number wins; 0/unknown leaves no +BUILD"
