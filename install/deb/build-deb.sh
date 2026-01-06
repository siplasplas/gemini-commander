#!/bin/bash
#
# Build script for Gemini Commander .deb package
# Usage: ./build-deb.sh [source-dir]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Source is two levels up from install/deb/
SOURCE_DIR="${1:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
PACKAGE_NAME="gemini-commander"
# Extract version from CMakeLists.txt
VERSION=$(sed -n 's/.*project(gemini-commander VERSION \([0-9.]*\).*/\1/p' "$SOURCE_DIR/CMakeLists.txt")
if [ -z "$VERSION" ]; then
    echo "ERROR: Could not extract version from CMakeLists.txt"
    exit 1
fi
BUILD_DIR="/tmp/${PACKAGE_NAME}-build"

echo "=========================================="
echo "Gemini Commander .deb Package Builder"
echo "=========================================="
echo "Version: $VERSION"
echo "Source:  $SOURCE_DIR"
echo ""

# Check for required tools
check_build_deps() {
    echo "Checking build dependencies..."
    local missing=()
    
    for tool in dpkg-buildpackage debhelper dh cmake; do
        if ! command -v "$tool" &> /dev/null && ! dpkg -l | grep -q "^ii  $tool"; then
            missing+=("$tool")
        fi
    done
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo "Missing tools: ${missing[*]}"
        echo ""
        echo "Install build dependencies with:"
        echo "  sudo apt install build-essential debhelper devscripts cmake"
        exit 1
    fi
    echo "All build tools available."
}

# Install build dependencies
install_build_deps() {
    echo ""
    echo "Installing build dependencies..."
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        debhelper \
        devscripts \
        cmake \
        g++ \
        qt6-base-dev \
        qt6-tools-dev \
        qt6-tools-dev-tools \
        libkf6texteditor-dev \
        libkf6widgetsaddons-dev \
        libkf6iconthemes-dev \
        libkf6archive-dev \
        libkf6coreaddons-dev \
        libkf6i18n-dev \
        libkf6windowsystem-dev \
        libkf6kio-dev \
        libkf6parts-dev \
        extra-cmake-modules \
        libgl1-mesa-dev \
        libicu-dev \
        libarchive-dev \
        libbotan-2-dev \
        pkg-config
    echo "Build dependencies installed."
}

# Prepare source tree
prepare_source() {
    echo ""
    echo "Preparing source tree..."
    
    # Clean previous build
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    
    # Copy source
    cp -r "$SOURCE_DIR" "$BUILD_DIR/${PACKAGE_NAME}-${VERSION}"
    
    # Copy debian directory from install/deb/
    if [ ! -d "$BUILD_DIR/${PACKAGE_NAME}-${VERSION}/debian" ]; then
        if [ -d "$SCRIPT_DIR/debian" ]; then
            cp -r "$SCRIPT_DIR/debian" "$BUILD_DIR/${PACKAGE_NAME}-${VERSION}/"
        elif [ -d "$SOURCE_DIR/install/deb/debian" ]; then
            cp -r "$SOURCE_DIR/install/deb/debian" "$BUILD_DIR/${PACKAGE_NAME}-${VERSION}/"
        else
            echo "ERROR: debian/ directory not found!"
            exit 1
        fi
    fi
    
    # Make rules executable
    chmod +x "$BUILD_DIR/${PACKAGE_NAME}-${VERSION}/debian/rules"
    
    echo "Source prepared in: $BUILD_DIR/${PACKAGE_NAME}-${VERSION}"
}

# Build package
build_package() {
    echo ""
    echo "Building .deb package..."
    
    cd "$BUILD_DIR/${PACKAGE_NAME}-${VERSION}"
    
    # Build unsigned package (for local use)
    dpkg-buildpackage -us -uc -b
    
    echo ""
    echo "Build complete!"
}

# Show results
show_results() {
    echo ""
    echo "=========================================="
    echo "Build Results"
    echo "=========================================="
    
    local deb_file=$(ls "$BUILD_DIR"/*.deb 2>/dev/null | head -1)
    
    if [ -f "$deb_file" ]; then
        echo "Package created: $deb_file"
        echo ""
        echo "Package info:"
        dpkg-deb --info "$deb_file"
        echo ""
        echo "Package contents:"
        dpkg-deb --contents "$deb_file" | head -30
        echo ""
        echo "To install:"
        echo "  sudo dpkg -i $deb_file"
        echo "  sudo apt-get install -f  # Fix dependencies if needed"
        echo ""
        echo "Or use apt directly:"
        echo "  sudo apt install $deb_file"
        
        # Copy to current directory
        cp "$deb_file" "$SCRIPT_DIR/"
        echo ""
        echo "Package copied to: $SCRIPT_DIR/$(basename "$deb_file")"
    else
        echo "ERROR: No .deb file found!"
        exit 1
    fi
}

# Main
main() {
    case "${1:-build}" in
        deps)
            install_build_deps
            ;;
        check)
            check_build_deps
            ;;
        build|*)
            check_build_deps
            prepare_source
            build_package
            show_results
            ;;
    esac
}

main "$@"
