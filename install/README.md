# Building Debian Packages

This directory contains scripts and configuration for building `.deb` packages for Ubuntu, Linux Mint and Debian.

Gemini Commander needs only Qt6 (no KDE Frameworks). qt-extra, qcodeedit, toml++
and textviewers are fetched by CMake during the build, so the build images
contain distribution packages only and the build needs network access.

## Quick Start

Build packages in containers (recommended):

```bash
cd install

# First time: build Docker images with dependencies (~5 min)
./build-in-container.sh setup

# Build all packages
./build-in-container.sh all
```

Packages will be in `install/output/`:
- `gemini-commander_1.6.0_amd64_ubuntu24.04.deb` - Ubuntu 24.04, Mint 22
- `gemini-commander_1.6.0_amd64_ubuntu25.10.deb` - Ubuntu 25.10
- `gemini-commander_1.6.0_amd64_debian-sid.deb` - Debian Sid

## Container Build Commands

```bash
./build-in-container.sh setup       # Build Docker images (one-time)
./build-in-container.sh ubuntu2404  # Build for Ubuntu 24.04 only
./build-in-container.sh ubuntu2510  # Build for Ubuntu 25.10 only
./build-in-container.sh qt6         # Build for Debian Sid only
./build-in-container.sh all         # Build all versions
./build-in-container.sh clean       # Remove Docker images
```

Requirements: `podman` or `docker` installed.

## Target Distributions

| Package | Target Systems |
|---------|----------------|
| `*_ubuntu24.04.deb` | Ubuntu 24.04 LTS, Linux Mint 22.x |
| `*_ubuntu25.10.deb` | Ubuntu 25.10 |
| `*_debian-sid.deb` | Debian Sid |

## Installing the Package

```bash
# Ubuntu 24.04 / Mint 22
sudo apt install ./gemini-commander_*_ubuntu24.04.deb

# Ubuntu 25.10
sudo apt install ./gemini-commander_*_ubuntu25.10.deb

# Debian Sid
sudo apt install ./gemini-commander_*_debian-sid.deb
```

## Directory Structure

```
install/
├── build-in-container.sh    # Main build script
├── docker/
│   ├── Dockerfile.ubuntu24.04   # Build environment per distribution
│   ├── Dockerfile.ubuntu25.10
│   └── Dockerfile.debian-sid
├── deb/                     # Debian packaging files
│   └── debian/
├── output/                  # Built .deb packages
└── logs/                    # Build logs
```

## Building Without Containers

If you prefer to build directly on your system:

```bash
# Install build dependencies
cd install/deb
./build-deb.sh deps

# Build the package
./build-deb.sh
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
