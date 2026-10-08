# Gemini Commander
Twin-panel file manager written in C++ with the Qt library.
```shell
git clone https://github.com/siplasplas/gemini-commander
```

<p align="center">
  <a href="screenshots/main-window.png">
    <img src="screenshots/main-window.png" alt="Gemini Commander">
  </a><br>
  <em>Dual-pane file manager built with Qt6</em>
</p>

## Features

- **Hidden file search** — Incremental search handles dot-prefixed files naturally (e.g., typing "git" finds ".gitignore")
- **Built-in editor and viewer** — Opens instantly on top of the main window, even on Wayland where window stacking can be problematic. The editor uses the [qcodeedit](https://github.com/siplasplas/qcodeedit) widget with Kate syntax definitions, folding, an inline find/replace bar (regular expressions, whole words, in selection), go to line:column and recent files
- **Drag and drop** — Drag files from panels to other applications
- **Command line path insertion** — Like Total Commander: Ctrl+Enter inserts current file path, Ctrl+Shift+Enter inserts full path, Ctrl+P inserts current directory. Unlike Double Commander, arrow keys immediately select text in the command line for quick keyboard-only copying
- **Safe USB copying** — Every copied file is synced to disk with fsync(), preventing data loss when unmounting USB drives
- **Smart directory/file navigation** — Ctrl+Down jumps to the first file (skipping directories), Ctrl+Up jumps to the last directory (from bottom)
- **Smart mount filtering** — Intelligent filtering of block devices and mounted directories, hiding system internals (ZFS pools like bpool/rpool, swap, tmpfs, snap mounts) while showing user-relevant mounts like USB drives, ISOs, and VirtualBox shares

## License

The source code of this project ([Project Name, e.g., Gemini Commander]) is made available under the terms of the **Apache License 2.0**. The full text of the license can be found in the `LICENSE` file in the repository's root directory.

### Qt Dependency and LGPLv3 License

This project uses the **Qt** framework ([https://www.qt.io/](https://www.qt.io/)) for building the user interface and other functionalities.

Key licensing assumptions regarding the use of Qt in this project:

1.  **LGPLv3 Module Usage:** The project aims to exclusively use Qt modules available under the **GNU Lesser General Public License version 3 (LGPLv3)**. The full text of this license can be found in the `LICENSE.LGPLv3` file (or similar) in the repository.
2.  **GPLv3 Module Restriction:** To maintain Apache 2.0 license compliance for the project's code, Qt modules available **exclusively** under the **GNU General Public License (GPL)** (e.g., historically modules like Qt Charts, Qt Data Visualization - the current list and module licenses should be checked in the Qt documentation for the version used) **cannot** be used. Using a GPL-licensed module would require relicensing the **entire** project under the GPL or a compatible license.
3.  **Dynamic Linking:** The application **must be linked dynamically** with the Qt libraries. This is an LGPLv3 requirement that allows end-users to replace the Qt libraries used by the application with other compatible versions (e.g., versions modified by them in accordance with LGPLv3).
4.  **Information for Users and Distributors:**
    * If you distribute a **compiled version** of this application, you must comply with the LGPLv3 requirements, including:
        * Informing users that the application uses Qt under the LGPLv3.
        * Providing users with the text of the LGPLv3 license.
        * Providing a mechanism to allow replacement of the Qt libraries (ensured by dynamic linking).
        * Indicating where to obtain the source code of the Qt version used.
    * If you distribute **only the source code**, you must still:
        * Inform about the use of Qt and the LGPLv3 license.
        * Include the text of the LGPLv3 license.
        * Indicate the source of the Qt code.
        * Specify the required Qt version.

The editor widget comes from [qcodeedit](https://github.com/siplasplas/qcodeedit) (LGPL-3.0-or-later)
and the shared widgets from [qt-extra](https://github.com/siplasplas/qt-extra) (LGPL-2.1); both are
fetched by CMake and linked statically. Since the complete source of Gemini Commander is available,
users can rebuild it with modified versions of these libraries, as the LGPL requires.

**In summary:** Your contributions to this project are licensed under Apache 2.0. However, because the project uses Qt libraries under the LGPLv3, the work as a whole (especially in binary form) is also subject to the terms of the LGPLv3 concerning the Qt parts. Users have rights and obligations stemming from both licenses, applicable to the respective components of the project.

## Prerequisites
Gemini Commander needs only **Qt6** (6.2 or later) — no KDE Frameworks and no Qt5 —
plus a few small libraries. CMake 3.24 or later is required.

On Ubuntu 24.04 LTS/25.10, Mint 22, Debian, Pop!_OS
```
sudo apt update
sudo apt install git cmake g++ pkg-config qt6-base-dev libbotan-2-dev \
    libicu-dev nlohmann-json3-dev libarchive-dev libgtest-dev librsvg2-bin
```

On Fedora/RHEL-based systems:
```
sudo dnf install git cmake gcc-c++ pkgconf-pkg-config qt6-qtbase-devel botan2-devel \
    libicu-devel json-devel libarchive-devel gtest-devel librsvg2 librsvg2-tools
```

On Arch-based systems (e.g., Arch Linux, Manjaro):
```
sudo pacman -Syu git cmake pkgconf qt6-base botan \
    icu nlohmann-json libarchive gtest librsvg
```

On openSUSE:
```
sudo zypper install git cmake gcc-c++ pkgconf-pkg-config qt6-base-devel libbotan-devel \
    libicu-devel nlohmann_json-devel libarchive-devel gtest rsvg-convert
```

On Windows (using MSYS2 or Chocolatey; note that KDE development on Windows may require additional setup):
(note: on Windows not works yet)
```
pacman -S git
git config --global core.autocrlf true # for IDE like CLion
pacman -S mingw-w64-ucrt-x86_64-cmake
pacman -S mingw-w64-ucrt-x86_64-gcc
pacman -S mingw-w64-ucrt-x86_64-gdb
pacman -S mingw-w64-ucrt-x86_64-gettext-tools
pacman -S mingw-w64-ucrt-x86_64-ninja
pacman -S mingw-w64-ucrt-x86_64-qt6-base
pacman -S mingw-w64-ucrt-x86_64-libbotan
pacman -S mingw-w64-ucrt-x86_64-nlohmann-json
pacman -S libarchive
pacman -S mingw-w64-ucrt-x86_64-gtest
```

### Libraries fetched by CMake

These libraries do not have to be installed; CMake's FetchContent downloads
the pinned version during configuration and builds it into Gemini Commander.
If a suitable version is already installed, it is used instead:

| Library | Used for | Installed version used if |
|---------|----------|---------------------------|
| [qt-extra](https://github.com/siplasplas/qt-extra) | `MruTabWidget`, `QxFileDialog` | `find_package(qt-extra 2.3)` |
| [qcodeedit](https://github.com/siplasplas/qcodeedit) | editor widget, Kate syntax highlighting, syntax downloader | `find_package(qcodeedit-katedata 1.7.0)` |
| [toml++](https://github.com/marzer/tomlplusplus) | configuration files | `find_package(tomlplusplus 3)` |
| [textviewers](https://github.com/ideorg/textviewers) | viewer for large files | always fetched |
| [backward-cpp](https://github.com/bombela/backward-cpp) | crash stack traces | always fetched |

For libraries installed outside the default search paths, pass
`-DCMAKE_PREFIX_PATH=/path/to/prefix`. To always use the fetched versions,
pass `-DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER -DCMAKE_DISABLE_FIND_PACKAGE_qcodeedit-katedata=ON`.

### Syntax definitions

The editor highlights files with Kate syntax definitions and color themes. They
are not part of the program: when the editor is opened the first time it offers
to download them from kate-editor.org (**Tools > Update Syntax Definitions**
downloads or updates them later). They are stored in
`~/.local/share/qcodeedit/kate-<version>/` and shared with other qcodeedit-based
programs. Without them, only C/C++ files are highlighted (built-in rules).
The **Default** theme is Breeze Light, or Breeze Dark on a dark desktop; before
the themes are downloaded the editor shows black text on white.

## Building

To build the project, you need CMake 3.24+, Ninja or Make, a C++ compiler supporting
C++20, Qt6 development libraries and network access for the fetched libraries.

```bash
git clone https://github.com/siplasplas/gemini-commander
cd gemini-commander
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build -j$(nproc)
```

On Windows - default is ninja
```shell
git clone https://github.com/siplasplas/gemini-commander
cd gemini-commander/
mkdir build && cd build
cmake ..
ninja
```

## Run the executable (from within the builddir)
```
./gemini_commander
``` 
## Installation
```shell
sudo make install
```

## Building Debian Packages

Pre-built `.deb` packages can be created using containers:

```bash
cd install
./build-in-container.sh setup   # One-time: build Docker images
./build-in-container.sh all     # Build packages for Ubuntu 24.04, 25.10 and Debian Sid
```

See [install/README.md](install/README.md) for details.
