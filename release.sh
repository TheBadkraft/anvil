#!/bin/bash
# Anvil Release Build Script
# Builds optimized production binaries without debug symbols
# Usage: ./release.sh [clean]

set -e

# Configuration
VERSION="0.1.0-alpha"
BUILD_DIR="build/release"
BIN_DIR="bin/release"
SRC="src"
INC="-Iinclude"

# Compiler settings
CC="${CC:-cc}"
CFLAGS="-std=c2x -Wall -Wextra -O3 -march=native -fPIC -s"  # -s strips debug symbols
AR="${AR:-ar}"
ARFLAGS="rcs"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Utility functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# Clean build artifacts
clean() {
    log_step "Cleaning release build artifacts..."
    rm -rf "$BUILD_DIR" "$BIN_DIR"
    log_info "Release build directory cleaned"
}

# Create directories
create_directories() {
    mkdir -p "$BUILD_DIR"
    mkdir -p "$BIN_DIR"
}

# Compile core objects
compile_core() {
    log_step "Compiling core library objects..."
    
    local core_sources=(
        "anvil.c"
        "context.c"
        "parser.c"
        "types.c"
        "resolver.c"
        "writer.c"
        "operators.c"
        "symbols.c"
        "errors.c"
    )
    
    for source in "${core_sources[@]}"; do
        local obj="$BUILD_DIR/${source%.c}.o"
        log_info "  Compiling $source → $obj"
        $CC $CFLAGS $INC -c "$SRC/core/$source" -o "$obj"
    done
    
    # Compile utilities separately
    log_info "  Compiling utils.c → $BUILD_DIR/utils.o"
    $CC $CFLAGS $INC -c "$SRC/utilities/utils.c" -o "$BUILD_DIR/utils.o"
}

# Build static library
build_static() {
    log_step "Building static library (libanvil.a)..."
    
    local objs=(
        "$BUILD_DIR/anvil.o"
        "$BUILD_DIR/context.o"
        "$BUILD_DIR/parser.o"
        "$BUILD_DIR/types.o"
        "$BUILD_DIR/resolver.o"
        "$BUILD_DIR/writer.o"
        "$BUILD_DIR/operators.o"
        "$BUILD_DIR/symbols.o"
        "$BUILD_DIR/errors.o"
        "$BUILD_DIR/utils.o"
    )
    
    $AR $ARFLAGS "$BIN_DIR/libanvil.a" "${objs[@]}"
    log_info "Static library created: $BIN_DIR/libanvil.a"
    
    # Show size
    local size=$(du -h "$BIN_DIR/libanvil.a" | cut -f1)
    log_info "  Size: $size"
}

# Build shared library
build_shared() {
    log_step "Building shared library (libanvil.so)..."
    
    local objs=(
        "$BUILD_DIR/anvil.o"
        "$BUILD_DIR/context.o"
        "$BUILD_DIR/parser.o"
        "$BUILD_DIR/types.o"
        "$BUILD_DIR/resolver.o"
        "$BUILD_DIR/writer.o"
        "$BUILD_DIR/operators.o"
        "$BUILD_DIR/symbols.o"
        "$BUILD_DIR/errors.o"
        "$BUILD_DIR/utils.o"
    )
    
    $CC -shared -o "$BIN_DIR/libanvil.so.${VERSION}" "${objs[@]}"
    log_info "Shared library created: $BIN_DIR/libanvil.so.${VERSION}"
    
    # Create symlinks
    ln -sf "libanvil.so.${VERSION}" "$BIN_DIR/libanvil.so.0"
    ln -sf "libanvil.so.${VERSION}" "$BIN_DIR/libanvil.so"
    log_info "  Symlinks created: libanvil.so, libanvil.so.0"
    
    # Show size
    local size=$(du -h "$BIN_DIR/libanvil.so.${VERSION}" | cut -f1)
    log_info "  Size: $size"
}

# Build CLI
build_cli() {
    log_step "Building CLI executable (anvil)..."
    
    $CC $CFLAGS $INC "$SRC/cli/main.c" \
        -L"$BIN_DIR" -lanvil \
        -lsigcore \
        -o "$BIN_DIR/anvil"
    
    log_info "CLI executable created: $BIN_DIR/anvil"
    
    # Show size
    local size=$(du -h "$BIN_DIR/anvil" | cut -f1)
    log_info "  Size: $size"
}

# Print summary
print_summary() {
    echo ""
    echo "=========================================="
    echo "Anvil Release Build Complete"
    echo "=========================================="
    echo ""
    log_info "Version: $VERSION"
    log_info "Build directory: $BUILD_DIR"
    log_info "Output directory: $BIN_DIR"
    echo ""
    
    echo "Build artifacts:"
    [ -f "$BIN_DIR/libanvil.a" ] && echo "  - Static library: $BIN_DIR/libanvil.a"
    [ -f "$BIN_DIR/libanvil.so" ] && echo "  - Shared library: $BIN_DIR/libanvil.so"
    [ -f "$BIN_DIR/anvil" ] && echo "  - CLI executable: $BIN_DIR/anvil"
    echo ""
    
    echo "Compiler flags:"
    echo "  $CFLAGS"
    echo ""
    
    echo "Next steps:"
    echo "  - Test: ./bin/release/anvil --help"
    echo "  - Install: sudo ./install.sh"
    echo "  - Copy libs to deployment: cp bin/release/libanvil.* <destination>"
    echo ""
}

# Measure build time
measure_build() {
    local start_time=$(date +%s%N)
    
    create_directories
    compile_core
    build_static
    build_shared
    build_cli
    
    local end_time=$(date +%s%N)
    local duration=$(( (end_time - start_time) / 1000000 ))  # Convert nanoseconds to milliseconds
    
    echo ""
    log_info "Build time: ${duration}ms"
}

# Main
main() {
    echo ""
    echo "Anvil Release Build Script v$VERSION"
    echo ""
    
    # Parse arguments
    case "${1:-}" in
        clean)
            clean
            exit 0
            ;;
        --help|-h)
            echo "Usage: $0 [COMMAND]"
            echo ""
            echo "Commands:"
            echo "  (none)      Build release binaries (default)"
            echo "  clean       Clean release build artifacts"
            echo "  --help, -h  Show this help message"
            echo ""
            echo "Build output:"
            echo "  Libraries: bin/release/libanvil.a, bin/release/libanvil.so"
            echo "  CLI: bin/release/anvil"
            echo ""
            echo "Note: Debug symbols are stripped (-s flag)"
            echo "      Maximum optimization is applied (-O3)"
            exit 0
            ;;
    esac
    
    # Check if source exists
    if [ ! -d "$SRC" ]; then
        log_error "Source directory not found: $SRC"
        exit 1
    fi
    
    # Check for sigcore library
    if ! pkg-config --exists libsigcore 2>/dev/null && [ ! -d "/usr/include/sigcore" ]; then
        log_warn "sigcore library not found (CLI will not be built)"
        log_warn "Install with: sudo apt-get install libsigcore-dev"
    fi
    
    log_info "Anvil Release Build Starting..."
    log_info "Compiler: $CC"
    log_info "Flags: $CFLAGS"
    echo ""
    
    measure_build
    print_summary
    
    log_info "Release build complete and ready for installation"
}

# Run main
main "$@"
