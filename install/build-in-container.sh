#!/bin/bash
#
# Build Gemini Commander .deb packages in containers
# Supports both Docker and Podman
#
# Usage:
#   ./build-in-container.sh qt5      # Build Qt5 version (Ubuntu 22.04)
#   ./build-in-container.sh qt6      # Build Qt6 version (Ubuntu 24.10)
#   ./build-in-container.sh all      # Build both versions
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/output"

# Detect container runtime
if command -v podman &> /dev/null; then
    CONTAINER_CMD="podman"
elif command -v docker &> /dev/null; then
    CONTAINER_CMD="docker"
else
    echo "ERROR: Neither podman nor docker found!"
    echo "Install with: sudo apt install podman"
    exit 1
fi

echo "Using container runtime: $CONTAINER_CMD"

# Create output directory
mkdir -p "$OUTPUT_DIR"

build_qt5() {
    echo ""
    echo "=========================================="
    echo "Building Qt5/KF5 version (Ubuntu 22.04)"
    echo "=========================================="

    $CONTAINER_CMD run --rm \
        -v "$SOURCE_DIR:/src:ro" \
        -v "$OUTPUT_DIR:/output" \
        ubuntu:22.04 \
        /bin/bash -c '
            set -e
            echo "=== Installing build dependencies ==="
            apt-get update
            DEBIAN_FRONTEND=noninteractive apt-get install -y \
                build-essential \
                debhelper \
                devscripts \
                cmake \
                g++ \
                qtbase5-dev \
                qttools5-dev \
                qttools5-dev-tools \
                libkf5texteditor-dev \
                libkf5widgetsaddons-dev \
                libkf5iconthemes-dev \
                libkf5archive-dev \
                libkf5coreaddons-dev \
                libkf5i18n-dev \
                libkf5windowsystem-dev \
                libkf5kio-dev \
                libkf5parts-dev \
                extra-cmake-modules \
                libgl1-mesa-dev \
                libicu-dev \
                libarchive-dev \
                libbotan-2-dev \
                pkg-config

            echo "=== Preparing source ==="
            cp -r /src /tmp/gemini-commander-build
            cd /tmp/gemini-commander-build
            cp -r install/deb-qt5/debian .

            echo "=== Building package ==="
            dpkg-buildpackage -us -uc -b

            echo "=== Copying result ==="
            cp /tmp/*.deb /output/

            # Rename with distro suffix
            cd /output
            for f in *.deb; do
                if [[ ! "$f" =~ ubuntu22.04 ]]; then
                    newname="${f%.deb}_ubuntu22.04.deb"
                    mv "$f" "$newname"
                fi
            done

            echo "=== Done ==="
            ls -la /output/*.deb
        '

    echo ""
    echo "Qt5 package built successfully!"
}

build_qt6() {
    echo ""
    echo "=========================================="
    echo "Building Qt6/KF6 version (Ubuntu 24.10)"
    echo "=========================================="

    $CONTAINER_CMD run --rm \
        -v "$SOURCE_DIR:/src:ro" \
        -v "$OUTPUT_DIR:/output" \
        ubuntu:24.10 \
        /bin/bash -c '
            set -e
            echo "=== Installing build dependencies ==="
            apt-get update
            DEBIAN_FRONTEND=noninteractive apt-get install -y \
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

            echo "=== Preparing source ==="
            cp -r /src /tmp/gemini-commander-build
            cd /tmp/gemini-commander-build
            cp -r install/deb/debian .

            echo "=== Building package ==="
            dpkg-buildpackage -us -uc -b

            echo "=== Copying result ==="
            cp /tmp/*.deb /output/

            # Rename with distro suffix
            cd /output
            for f in *.deb; do
                if [[ ! "$f" =~ ubuntu24.10 ]]; then
                    newname="${f%.deb}_ubuntu24.10.deb"
                    mv "$f" "$newname"
                fi
            done

            echo "=== Done ==="
            ls -la /output/*.deb
        '

    echo ""
    echo "Qt6 package built successfully!"
}

show_results() {
    echo ""
    echo "=========================================="
    echo "Build Results"
    echo "=========================================="
    echo ""
    echo "Packages in $OUTPUT_DIR:"
    ls -lh "$OUTPUT_DIR"/*.deb 2>/dev/null || echo "No packages found"
    echo ""
    echo "To install on target system:"
    echo "  sudo apt install ./gemini-commander_*_ubuntu22.04.deb  # For Ubuntu 22.04/Mint 21"
    echo "  sudo apt install ./gemini-commander_*_ubuntu24.10.deb  # For Ubuntu 24.10+"
}

case "${1:-help}" in
    qt5)
        build_qt5
        show_results
        ;;
    qt6)
        build_qt6
        show_results
        ;;
    all)
        build_qt5
        build_qt6
        show_results
        ;;
    *)
        echo "Usage: $0 {qt5|qt6|all}"
        echo ""
        echo "  qt5  - Build for Ubuntu 22.04 / Mint 21 (Qt5/KF5)"
        echo "  qt6  - Build for Ubuntu 24.10+ (Qt6/KF6)"
        echo "  all  - Build both versions"
        echo ""
        echo "Packages will be saved to: $OUTPUT_DIR"
        ;;
esac