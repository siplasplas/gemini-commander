# Building Debian Packages

This directory contains scripts and configuration for building `.deb` packages for Ubuntu and Linux Mint.

## Quick Start

Build packages in containers (recommended):

```bash
cd install

# First time: build Docker images with dependencies (~5 min)
./build-in-container.sh setup

# Build both Qt5 and Qt6 packages (~2 min)
./build-in-container.sh all
```

Packages will be in `install/output/`:
- `gemini-commander_1.5.0_amd64_ubuntu22.04.deb` - Qt5/KF5 for Ubuntu 22.04, Mint 21
- `gemini-commander_1.5.0_amd64_ubuntu24.10.deb` - Qt6/KF6 for Ubuntu 24.10+

## Container Build Commands

```bash
./build-in-container.sh setup   # Build Docker images (one-time)
./build-in-container.sh qt5     # Build Qt5 version only
./build-in-container.sh qt6     # Build Qt6 version only
./build-in-container.sh all     # Build both versions
./build-in-container.sh clean   # Remove Docker images
```

Requirements: `podman` or `docker` installed.

## Target Distributions

| Package | Qt/KF Version | Target Systems |
|---------|---------------|----------------|
| `*_ubuntu22.04.deb` | Qt5 / KF5 | Ubuntu 22.04 LTS, Linux Mint 21.x |
| `*_ubuntu24.10.deb` | Qt6 / KF6 | Ubuntu 24.10+, systems with KDE Frameworks 6 |

## Installing the Package

```bash
# Ubuntu 22.04 / Mint 21
sudo apt install ./gemini-commander_*_ubuntu22.04.deb

# Ubuntu 24.10+
sudo apt install ./gemini-commander_*_ubuntu24.10.deb
```

## Directory Structure

```
install/
├── build-in-container.sh    # Main build script
├── docker/
│   ├── Dockerfile.ubuntu22.04   # Qt5/KF5 build environment
│   └── Dockerfile.ubuntu24.10   # Qt6/KF6 build environment
├── deb/                     # Qt6/KF6 debian packaging files
│   └── debian/
├── deb-qt5/                 # Qt5/KF5 debian packaging files
│   └── debian/
├── output/                  # Built .deb packages
└── logs/                    # Build logs
```

## Building Without Containers

If you prefer to build directly on your system:

```bash
# Install build dependencies (Qt6/KF6)
cd install/deb
./build-deb.sh deps

# Build the package
./build-deb.sh

# For Qt5/KF5 version
cd install/deb-qt5
./build-deb-qt5.sh deps
./build-deb-qt5.sh
```

## Troubleshooting

Build logs are saved to `install/logs/`. Check the latest log if build fails:

```bash
cat install/logs/build-qt6-*.log | tail -100
```

To rebuild from scratch:

```bash
./build-in-container.sh clean
./build-in-container.sh setup
./build-in-container.sh all
```