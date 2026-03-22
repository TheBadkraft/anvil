# Configuration file for Anvil — Bash Build System (BBS)
# Sourced by cbuild, ctest, and rtest

CC=gcc
STD=c2x

# Memory checking configuration
VALGRIND_ENABLED=true
VALGRIND_OPTS="--leak-check=full --show-leak-kinds=all --track-origins=yes --verbose"

ASAN_ENABLED=false
ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1:detect_invalid_pointer_pairs=1"

# Base compiler flags
# Library (production): optimized, no debug symbols
BASE_CFLAGS="-Wall -Wextra -O2 -fPIC -std=$STD -I./include -I/usr/local/include"

if [ "$ASAN_ENABLED" = true ]; then
    BASE_CFLAGS="$BASE_CFLAGS -fsanitize=address -fsanitize=undefined"
fi

CFLAGS="$BASE_CFLAGS"
# Test binaries: keep debug symbols for valgrind/GDB; don't inherit NDEBUG so asserts fire
TST_CFLAGS="-Wall -Wextra -g -fPIC -std=$STD -I./include -I/usr/local/include -DTSTDBG -I./test -D_POSIX_C_SOURCE=200809L"
LDFLAGS="-shared"
TST_LDFLAGS="-lm"  # -lm for bench stddev

SRC_DIR=src
BUILD_DIR=build
BIN_DIR=bin
LIB_DIR="$BIN_DIR/lib"
TEST_DIR=test
TST_BUILD_DIR="$BUILD_DIR/test"
LIB_NAME="anvil"

# Runtime dependencies (cbuild resolves these as /usr/local/packages/<name>.o)
REQUIRES=("sigma.memory" "sigma.core.module" "sigma.core.text" "sigma.collections")

# Test dependencies (rtest resolves these — adds sigma.test on top of REQUIRES)
TST_REQUIRES=("sigma.memory" "sigma.core.module" "sigma.core.text" "sigma.core.utils" "sigma.collections" "sigma.test")

# ---------------------------------------------------------------------------
# Package: anvil (mandatory core)
# Sources: src/core/*.c + src/utilities/*.c
# Artifact: anvil.o  (cpkg anvil)
# ---------------------------------------------------------------------------
shopt -s nullglob
_CORE_SRCS=("$SRC_DIR/core"/*.c)
_UTIL_SRCS=("$SRC_DIR/utilities"/*.c)
ANVIL_SOURCES=("${_CORE_SRCS[@]}" "${_UTIL_SRCS[@]}")
unset _CORE_SRCS _UTIL_SRCS

# Flatten object paths into BUILD_DIR (avoids mirroring subdirs)
ANVIL_OBJECTS=()
for _s in "${ANVIL_SOURCES[@]}"; do
    ANVIL_OBJECTS+=("$BUILD_DIR/$(basename "${_s%.c}").o")
done
unset _s

# ---------------------------------------------------------------------------
# Package: anvil.resolver (post-parse optional)
# Sources: src/resolver/*.c
# Artifact: anvil.resolver.o  (cpkg anvil.resolver)
# ---------------------------------------------------------------------------
shopt -s nullglob
_RESOLVER_SRCS=("$SRC_DIR/resolver"/*.c)
RESOLVER_SOURCES=("${_RESOLVER_SRCS[@]}")
unset _RESOLVER_SRCS

RESOLVER_OBJECTS=()
for _s in "${RESOLVER_SOURCES[@]}"; do
    RESOLVER_OBJECTS+=("$BUILD_DIR/$(basename "${_s%.c}").o")
done
unset _s

# ---------------------------------------------------------------------------
# Package: anvil.serializer (post-parse optional)
# Sources: src/serializer/*.c
# Artifact: anvil.serializer.o  (cpkg anvil.serializer)
# ---------------------------------------------------------------------------
shopt -s nullglob
_SERIALIZER_SRCS=("$SRC_DIR/serializer"/*.c)
SERIALIZER_SOURCES=("${_SERIALIZER_SRCS[@]}")
unset _SERIALIZER_SRCS

SERIALIZER_OBJECTS=()
for _s in "${SERIALIZER_SOURCES[@]}"; do
    SERIALIZER_OBJECTS+=("$BUILD_DIR/$(basename "${_s%.c}").o")
done
unset _s

# Test helper object (compiled from test/utilities/helpers.c, linked with every test)
ANVIL_HELPER_SRC="$TEST_DIR/utilities/helpers.c"
ANVIL_HELPER_OBJ="$TST_BUILD_DIR/helpers.o"

# ---------------------------------------------------------------------------
# Custom build functions
# ---------------------------------------------------------------------------

anvil_compile() {
    mkdir -p "$BUILD_DIR"
    for i in "${!ANVIL_SOURCES[@]}"; do
        local src="${ANVIL_SOURCES[$i]}"
        local obj="${ANVIL_OBJECTS[$i]}"
        if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
            echo "Compiling $src -> $obj"
            $CC $CFLAGS -c "$src" -o "$obj"
        fi
    done
}

anvil_build_lib() {
    anvil_compile
    local lib="$LIB_DIR/lib${LIB_NAME}.so"
    mkdir -p "$LIB_DIR"
    echo "Linking $lib"
    $CC "${ANVIL_OBJECTS[@]}" "${REQUIRES_OBJECTS[@]}" -o "$lib" $LDFLAGS
}

compile_resolver_objects() {
    mkdir -p "$BUILD_DIR"
    for i in "${!RESOLVER_SOURCES[@]}"; do
        local src="${RESOLVER_SOURCES[$i]}"
        local obj="${RESOLVER_OBJECTS[$i]}"
        if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
            echo "Compiling $src -> $obj"
            $CC $CFLAGS -c "$src" -o "$obj"
        fi
    done
}

compile_serializer_objects() {
    mkdir -p "$BUILD_DIR"
    for i in "${!SERIALIZER_SOURCES[@]}"; do
        local src="${SERIALIZER_SOURCES[$i]}"
        local obj="${SERIALIZER_OBJECTS[$i]}"
        if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
            echo "Compiling $src -> $obj"
            $CC $CFLAGS -c "$src" -o "$obj"
        fi
    done
}

# ---------------------------------------------------------------------------
# Package: anvil.vars (post-parse optional)
# Sources: src/vars/*.c
# Artifact: anvil.vars.o  (cpkg anvil.vars)
# ---------------------------------------------------------------------------
shopt -s nullglob
_VARS_SRCS=("$SRC_DIR/vars"/*.c)
VARS_SOURCES=("${_VARS_SRCS[@]}")
unset _VARS_SRCS

VARS_OBJECTS=()
for _s in "${VARS_SOURCES[@]}"; do
    VARS_OBJECTS+=("$BUILD_DIR/$(basename "${_s%.c}").o")
done
unset _s

compile_vars_objects() {
    mkdir -p "$BUILD_DIR"
    for i in "${!VARS_SOURCES[@]}"; do
        local src="${VARS_SOURCES[$i]}"
        local obj="${VARS_OBJECTS[$i]}"
        if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
            echo "Compiling $src -> $obj"
            $CC $CFLAGS -c "$src" -o "$obj"
        fi
    done
}

# ---------------------------------------------------------------------------
# Package: anvil.import (post-parse optional)
# Sources: src/import/*.c
# Artifact: anvil.import.o  (cpkg anvil.import)
# ---------------------------------------------------------------------------
shopt -s nullglob
_IMPORT_SRCS=("$SRC_DIR/import"/*.c)
IMPORT_SOURCES=("${_IMPORT_SRCS[@]}")
unset _IMPORT_SRCS

IMPORT_OBJECTS=()
for _s in "${IMPORT_SOURCES[@]}"; do
    IMPORT_OBJECTS+=("$BUILD_DIR/$(basename "${_s%.c}").o")
done
unset _s

compile_import_objects() {
    mkdir -p "$BUILD_DIR"
    for i in "${!IMPORT_SOURCES[@]}"; do
        local src="${IMPORT_SOURCES[$i]}"
        local obj="${IMPORT_OBJECTS[$i]}"
        if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
            echo "Compiling $src -> $obj"
            $CC $CFLAGS -c "$src" -o "$obj"
        fi
    done
}

# ---------------------------------------------------------------------------
# Package: anvil.asl (post-parse optional)
# Sources: src/asl/*.c
# Artifact: anvil.asl.o  (cpkg anvil.asl)
# ---------------------------------------------------------------------------
shopt -s nullglob
_ASL_SRCS=("$SRC_DIR/asl"/*.c)
ASL_SOURCES=("${_ASL_SRCS[@]}")
unset _ASL_SRCS

ASL_OBJECTS=()
for _s in "${ASL_SOURCES[@]}"; do
    ASL_OBJECTS+=("$BUILD_DIR/$(basename "${_s%.c}").o")
done
unset _s

compile_asl_objects() {
    mkdir -p "$BUILD_DIR"
    for i in "${!ASL_SOURCES[@]}"; do
        local src="${ASL_SOURCES[$i]}"
        local obj="${ASL_OBJECTS[$i]}"
        if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
            echo "Compiling $src -> $obj"
            $CC $CFLAGS -c "$src" -o "$obj"
        fi
    done
}

# ---------------------------------------------------------------------------
# Package: anvil.schema (post-parse optional)
# Sources: src/schema/*.c
# Artifact: anvil.schema.o  (cpkg anvil.schema)
# ---------------------------------------------------------------------------
shopt -s nullglob
_SCHEMA_SRCS=("$SRC_DIR/schema"/*.c)
SCHEMA_SOURCES=("${_SCHEMA_SRCS[@]}")
unset _SCHEMA_SRCS

SCHEMA_OBJECTS=()
for _s in "${SCHEMA_SOURCES[@]}"; do
    SCHEMA_OBJECTS+=("$BUILD_DIR/$(basename "${_s%.c}").o")
done
unset _s

compile_schema_objects() {
    mkdir -p "$BUILD_DIR"
    for i in "${!SCHEMA_SOURCES[@]}"; do
        local src="${SCHEMA_SOURCES[$i]}"
        local obj="${SCHEMA_OBJECTS[$i]}"
        if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
            echo "Compiling $src -> $obj"
            $CC $CFLAGS -c "$src" -o "$obj"
        fi
    done
}

# ---------------------------------------------------------------------------
# Build targets
# ---------------------------------------------------------------------------
declare -A BUILD_TARGETS=(
    ["all"]="anvil_compile"
    ["lib"]="anvil_build_lib"
    ["compile"]="anvil_compile"
    ["clean"]="clean"
    ["clean_all"]="clean_all"
    ["install"]="anvil_build_lib && install_lib"
    ["test"]="run_all_tests"
    ["root"]="show_project_info"
)

# ---------------------------------------------------------------------------
# cpkg package definitions
# Format: "output_name | src_basename1 src_basename2 ..."
# Usage:  cpkg anvil  |  cpkg resolver
# ---------------------------------------------------------------------------
declare -A PACKAGES=(
    ["anvil"]="anvil | anvil_impl context errors module operators parser symbols types utils"
    ["resolver"]="anvil.resolver | resolver"
    ["anvil.serializer"]="anvil.serializer | serializer"
    ["anvil.import"]="anvil.import | import"
    ["anvil.schema"]="anvil.schema | schema"
)

# ---------------------------------------------------------------------------
# Test configurations (for rtest --config or TEST_CONFIGS map)
# ---------------------------------------------------------------------------
declare -A TEST_CONFIGS=(
    ["prototype"]="with_proto_objects"
    ["resolver"]="with_resolver_objects"
    ["serializer"]="with_serializer_objects"
    ["vars"]="with_vars_objects"
    ["import"]="with_import_objects"
    ["asl"]="with_asl_objects"
    ["schema"]="with_schema_objects"
    # benchmark test configs
    ["bench_parse"]="standard"
    ["bench_resolver"]="with_resolver_objects"
    ["bench_serializer"]="with_serializer_objects"
    ["bench_asl"]="with_asl_objects"
    ["performance"]="standard"
)

# Per-test package overrides (omit sigma.test.o for standalone-main tests)
declare -A TEST_PKG_OVERRIDES=(
    ["bench_parse"]="sigma.memory sigma.core.module sigma.core.text sigma.core.utils"
    ["bench_asl"]="sigma.memory sigma.core.module sigma.core.text sigma.core.utils"
    ["bench_resolver"]="sigma.memory sigma.core.module sigma.core.text sigma.core.utils"
    ["bench_serializer"]="sigma.memory sigma.core.module sigma.core.text sigma.core.utils"
    ["performance"]="sigma.memory sigma.core.module sigma.core.text sigma.core.utils"
)

# Prototype objects (linked only for tests that need them)
_PROTO_SRC="$TEST_DIR/utilities/prototype.c"
PROTO_OBJECTS=()
if [ -f "$_PROTO_SRC" ]; then
    PROTO_OBJECTS+=("$TST_BUILD_DIR/prototype.o")
fi
unset _PROTO_SRC

declare -A TEST_COMPILE_FLAGS=()

# Per-test linker flag overrides (omit --wrap for standalone-main tests)
declare -A TEST_LDFLAGS_OVERRIDES=(
    ["bench_parse"]="-lm"
    ["bench_asl"]="-lm"
    ["bench_resolver"]="-lm"
    ["bench_serializer"]="-lm"
    ["performance"]="-lm"
)
