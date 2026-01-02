#!/bin/bash
#
# Build Gemini Commander .deb packages in containers
# Supports both Docker and Podman
#
# Usage:
#   ./build-in-container.sh qt5       # Build Qt5 version (Ubuntu 24.04)
#   ./build-in-container.sh qt6       # Build Qt6 version (Debian Sid)
#   ./build-in-container.sh all       # Build both versions
#   ./build-in-container.sh setup     # Build Docker images (run once)
#   ./build-in-container.sh clean     # Remove Docker images
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/output"
LOG_DIR="$SCRIPT_DIR/logs"
DOCKER_DIR="$SCRIPT_DIR/docker"

IMAGE_QT5="gemini-commander-build:ubuntu24.04"
IMAGE_QT6="gemini-commander-build:debian-sid"

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

# Create directories
mkdir -p "$OUTPUT_DIR" "$LOG_DIR"

# Check if image exists
image_exists() {
    $CONTAINER_CMD image exists "$1" 2>/dev/null || $CONTAINER_CMD images -q "$1" 2>/dev/null | grep -q .
}

# Build Docker images
setup_images() {
    echo ""
    echo "=========================================="
    echo "Building Docker images (one-time setup)"
    echo "=========================================="

    echo ""
    echo "Building Qt5 image (Ubuntu 24.04)..."
    $CONTAINER_CMD build -t "$IMAGE_QT5" -f "$DOCKER_DIR/Dockerfile.ubuntu24.04" "$DOCKER_DIR"

    echo ""
    echo "Building Qt6 image (Debian Sid)..."
    $CONTAINER_CMD build -t "$IMAGE_QT6" -f "$DOCKER_DIR/Dockerfile.debian-sid" "$DOCKER_DIR"

    echo ""
    echo "Images built successfully!"
    echo "You can now run: $0 all"
}

# Clean Docker images
clean_images() {
    echo "Removing build images..."
    $CONTAINER_CMD rmi "$IMAGE_QT5" 2>/dev/null || true
    $CONTAINER_CMD rmi "$IMAGE_QT6" 2>/dev/null || true
    echo "Done."
}

build_qt5() {
    echo ""
    echo "=========================================="
    echo "Building Qt5/KF5 version (Ubuntu 2.04)"
    echo "=========================================="

    if ! image_exists "$IMAGE_QT5"; then
        echo "Image not found. Building..."
        $CONTAINER_CMD build -t "$IMAGE_QT5" -f "$DOCKER_DIR/Dockerfile.ubuntu24.04" "$DOCKER_DIR"
    fi

    local LOG_FILE="$LOG_DIR/build-qt5-$(date +%Y%m%d-%H%M%S).log"
    echo "Log file: $LOG_FILE"

    $CONTAINER_CMD run --rm \
        -v "$SOURCE_DIR:/src:ro" \
        -v "$OUTPUT_DIR:/output" \
        "$IMAGE_QT5" \
        /bin/bash -c '
            set -e
            echo "=== Preparing source ==="
            cp -r /src /tmp/gemini-commander-build
            cd /tmp/gemini-commander-build
            cp -r install/deb-qt5/debian .

            echo "=== Building package ==="
            dpkg-buildpackage -us -uc -b

            echo "=== Copying result ==="
            # Copy and rename in one step to avoid conflicts
            for f in /tmp/gemini-commander_*.deb; do
                if [[ -f "$f" ]]; then
                    base=$(basename "$f")
                    newname="${base%.deb}_ubuntu24.04.deb"
                    cp "$f" "/output/$newname"
                fi
            done

            echo "=== Done ==="
            ls -la /output/*ubuntu24.04*.deb 2>/dev/null || echo "No .deb files found"
        ' 2>&1 | tee "$LOG_FILE"

    if ls "$OUTPUT_DIR"/*ubuntu24.04*.deb 1>/dev/null 2>&1; then
        echo ""
        echo "Qt5 package built successfully!"
    else
        echo ""
        echo "ERROR: Build failed. Check log: $LOG_FILE"
        return 1
    fi
}

build_qt6() {
    echo ""
    echo "=========================================="
    echo "Building Qt6/KF6 version (Debian Sid)"
    echo "=========================================="

    if ! image_exists "$IMAGE_QT6"; then
        echo "Image not found. Building..."
        $CONTAINER_CMD build -t "$IMAGE_QT6" -f "$DOCKER_DIR/Dockerfile.debian-sid" "$DOCKER_DIR"
    fi

    local LOG_FILE="$LOG_DIR/build-qt6-$(date +%Y%m%d-%H%M%S).log"
    echo "Log file: $LOG_FILE"

    $CONTAINER_CMD run --rm \
        -v "$SOURCE_DIR:/src:ro" \
        -v "$OUTPUT_DIR:/output" \
        "$IMAGE_QT6" \
        /bin/bash -c '
            set -e
            echo "=== Preparing source ==="
            cp -r /src /tmp/gemini-commander-build
            cd /tmp/gemini-commander-build
            cp -r install/deb/debian .

            echo "=== Building package ==="
            dpkg-buildpackage -us -uc -b

            echo "=== Copying result ==="
            # Copy and rename in one step to avoid conflicts
            for f in /tmp/gemini-commander_*.deb; do
                if [[ -f "$f" ]]; then
                    base=$(basename "$f")
                    newname="${base%.deb}_debian-sid.deb"
                    cp "$f" "/output/$newname"
                fi
            done

            echo "=== Done ==="
            ls -la /output/*debian-sid*.deb 2>/dev/null || echo "No .deb files found"
        ' 2>&1 | tee "$LOG_FILE"

    if ls "$OUTPUT_DIR"/*debian-sid*.deb 1>/dev/null 2>&1; then
        echo ""
        echo "Qt6 package built successfully!"
    else
        echo ""
        echo "ERROR: Build failed. Check log: $LOG_FILE"
        return 1
    fi
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
    echo "Logs in $LOG_DIR:"
    ls -lh "$LOG_DIR"/*.log 2>/dev/null | tail -5
    echo ""
    echo "To install on target system:"
    echo "  sudo apt install ./gemini-commander_*_ubuntu24.04.deb  # For Ubuntu 24.04/Mint 21"
    echo "  sudo apt install ./gemini-commander_*_debian-sid.deb   # For Debian Sid / Ubuntu 24.10+"
}

case "${1:-help}" in
    setup)
        setup_images
        ;;
    clean)
        clean_images
        ;;
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
        echo "Usage: $0 {setup|qt5|qt6|all|clean}"
        echo ""
        echo "  setup  - Build Docker images with dependencies (run once, ~5 min)"
        echo "  qt5    - Build for Ubuntu 24.04 / Mint 22 (Qt5/KF5)"
        echo "  qt6    - Build for Debian Sid / Ubuntu 24.10+ (Qt6/KF6)"
        echo "  all    - Build both versions"
        echo "  clean  - Remove Docker images"
        echo ""
        echo "First run:  $0 setup && $0 all"
        echo "Next runs:  $0 all  (fast, uses cached images)"
        echo ""
        echo "Packages: $OUTPUT_DIR"
        echo "Logs:     $LOG_DIR"
        ;;
esac
