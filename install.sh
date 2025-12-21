#!/bin/bash
# Anvil Installation Script
# Installs libanvil library and headers to system locations
# Usage: sudo ./install.sh [PREFIX]
# Default PREFIX: / (installs to /usr/lib and /usr/include/anvil)

set -e

# Configuration
PREFIX="${1:=/}"
LIB_DIR="${PREFIX}usr/lib"
INCLUDE_DIR="${PREFIX}usr/include/anvil"
BIN_DIR="${PREFIX}usr/bin"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

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

# Check if running with proper permissions
check_permissions() {
    if [ "$PREFIX" = "/" ] && [ "$EUID" -ne 0 ]; then
        log_error "Installing to system directories requires root privileges"
        echo "Try: sudo ./install.sh"
        exit 1
    fi
}

# Verify build artifacts exist
verify_build() {
    if [ ! -f "bin/libanvil.so" ] && [ ! -f "bin/libanvil.a" ]; then
        log_error "Build artifacts not found. Run 'make' first"
        exit 1
    fi
    
    if [ ! -f "include/anvil.h" ]; then
        log_error "Header files not found"
        exit 1
    fi
}

# Create directories
create_directories() {
    log_info "Creating installation directories..."
    mkdir -p "$LIB_DIR"
    mkdir -p "$INCLUDE_DIR"
    mkdir -p "$BIN_DIR"
    chmod 755 "$LIB_DIR" "$INCLUDE_DIR" "$BIN_DIR"
}

# Install libraries
install_libraries() {
    log_info "Installing libraries to $LIB_DIR..."
    
    if [ -f "bin/libanvil.so" ]; then
        cp -v "bin/libanvil.so" "$LIB_DIR/libanvil.so.0.1.0"
        chmod 644 "$LIB_DIR/libanvil.so.0.1.0"
        
        # Create symlinks for version compatibility
        ln -sf "libanvil.so.0.1.0" "$LIB_DIR/libanvil.so.0"
        ln -sf "libanvil.so.0.1.0" "$LIB_DIR/libanvil.so"
        log_info "Shared library installed: libanvil.so.0.1.0"
    else
        log_warn "Shared library not built (bin/libanvil.so)"
    fi
    
    if [ -f "bin/libanvil.a" ]; then
        cp -v "bin/libanvil.a" "$LIB_DIR/libanvil.a"
        chmod 644 "$LIB_DIR/libanvil.a"
        log_info "Static library installed: libanvil.a"
    else
        log_warn "Static library not built (bin/libanvil.a)"
    fi
    
    # Update library cache on Linux systems
    if command -v ldconfig &> /dev/null; then
        ldconfig "$LIB_DIR" 2>/dev/null || true
    fi
}

# Install headers
install_headers() {
    log_info "Installing headers to $INCLUDE_DIR..."
    
    # Copy public headers
    cp -v include/anvil.h "$INCLUDE_DIR/"
    cp -v include/constants.h "$INCLUDE_DIR/"
    cp -v include/context.h "$INCLUDE_DIR/"
    cp -v include/errors.h "$INCLUDE_DIR/"
    cp -v include/operators.h "$INCLUDE_DIR/"
    cp -v include/parser.h "$INCLUDE_DIR/"
    cp -v include/symbols.h "$INCLUDE_DIR/"
    cp -v include/types.h "$INCLUDE_DIR/"
    cp -v include/utils.h "$INCLUDE_DIR/"
    
    # Copy internal headers subdirectory
    mkdir -p "$INCLUDE_DIR/internal"
    if [ -d "include/internal" ]; then
        cp -v include/internal/*.h "$INCLUDE_DIR/internal/" 2>/dev/null || true
    fi
    
    # Set permissions
    chmod 644 "$INCLUDE_DIR"/*.h
    [ -d "$INCLUDE_DIR/internal" ] && chmod 644 "$INCLUDE_DIR/internal"/*.h 2>/dev/null || true
    chmod 755 "$INCLUDE_DIR" "$INCLUDE_DIR/internal" 2>/dev/null || true
    
    log_info "Headers installed to $INCLUDE_DIR"
}

# Install CLI executable
install_cli() {
    if [ -f "bin/anvil" ]; then
        log_info "Installing CLI to $BIN_DIR..."
        cp -v "bin/anvil" "$BIN_DIR/anvil"
        chmod 755 "$BIN_DIR/anvil"
        log_info "CLI installed: anvil"
    else
        log_warn "CLI executable not built (bin/anvil)"
    fi
}

# Print installation summary
print_summary() {
    echo ""
    echo "=========================================="
    echo "Anvil Installation Complete"
    echo "=========================================="
    echo ""
    log_info "Installation directory: $PREFIX"
    echo ""
    echo "Installed files:"
    [ -f "$LIB_DIR/libanvil.so" ] && echo "  - Library (shared): $LIB_DIR/libanvil.so"
    [ -f "$LIB_DIR/libanvil.a" ] && echo "  - Library (static): $LIB_DIR/libanvil.a"
    [ -d "$INCLUDE_DIR" ] && echo "  - Headers: $INCLUDE_DIR/"
    [ -f "$BIN_DIR/anvil" ] && echo "  - CLI: $BIN_DIR/anvil"
    echo ""
    echo "Next steps:"
    echo "  1. Link against libavil: gcc myapp.c -L$LIB_DIR -lanvil -o myapp"
    echo "  2. Include headers: #include <anvil/anvil.h>"
    echo ""
}

# Uninstall function
uninstall() {
    log_warn "Uninstalling Anvil..."
    rm -fv "$LIB_DIR"/libanvil.so* "$LIB_DIR"/libanvil.a
    rm -rfv "$INCLUDE_DIR"
    rm -fv "$BIN_DIR/anvil"
    log_info "Uninstall complete"
    exit 0
}

# Main installation flow
main() {
    echo ""
    echo "Anvil Installation Script v0.1.0-alpha"
    echo ""
    
    # Parse arguments
    case "${1:-}" in
        --uninstall|-u)
            uninstall
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS] [PREFIX]"
            echo ""
            echo "Options:"
            echo "  --uninstall, -u    Uninstall Anvil"
            echo "  --help, -h          Show this help message"
            echo ""
            echo "Arguments:"
            echo "  PREFIX              Installation prefix (default: /)"
            echo "                      Libraries: PREFIX/usr/lib"
            echo "                      Headers: PREFIX/usr/include/anvil"
            echo ""
            echo "Examples:"
            echo "  sudo ./install.sh"
            echo "  sudo ./install.sh /usr/local"
            echo "  ./install.sh /opt/anvil"
            exit 0
            ;;
    esac
    
    # Main installation steps
    log_info "Anvil v0.1.0-alpha Installation"
    check_permissions
    verify_build
    create_directories
    install_libraries
    install_headers
    install_cli
    print_summary
}

# Run main
main "$@"
