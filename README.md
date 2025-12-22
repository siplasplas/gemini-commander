
# gemini-commander
Twin-panel file manager written in C++ with the Qt library.

## Features

- **Hidden file search** — Incremental search handles dot-prefixed files naturally (e.g., typing "git" finds ".gitignore")
- **Built-in editor and viewer** — Opens instantly on top of the main window, even on Wayland where window stacking can be problematic
- **Drag and drop** — Drag files from panels to other applications
- **Command line path insertion** — Like Total Commander: Ctrl+Enter inserts current file path, Ctrl+Shift+Enter inserts full path, Ctrl+P inserts current directory. Unlike Double Commander, arrow keys immediately select text in the command line for quick keyboard-only copying
- **Safe USB copying** — Every copied file is synced to disk with fsync(), preventing data loss when unmounting USB drives
- **Smart directory/file navigation** — Ctrl+Down jumps to the first file (skipping directories), Ctrl+Up jumps to the last directory (from bottom)

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

**In summary:** Your contributions to this project are licensed under Apache 2.0. However, because the project uses Qt libraries under the LGPLv3, the work as a whole (especially in binary form) is also subject to the terms of the LGPLv3 concerning the Qt parts. Users have rights and obligations stemming from both licenses, applicable to the respective components of the project.

## Prerequisites
On Ubuntu/Debian-based systems (e.g., Ubuntu, Linux Mint, Pop!_OS):
```
sudo apt update
sudo apt install extra-cmake-modules
```

On Fedora/RHEL-based systems:
```
sudo dnf install extra-cmake-modules
```

On Arch-based systems (e.g., Arch Linux, Manjaro):
```
sudo pacman -Syu extra-cmake-modules
```

On openSUSE:
```
sudo zypper install extra-cmake-modules
```

On macOS (using Homebrew):
```
brew install extra-cmake-modules
```

On Windows (using MSYS2 or Chocolatey; note that KDE development on Windows may require additional setup):
```
pacman -S mingw-w64-x86_64-extra-cmake-modules  # For MSYS2
```


## Unicode search with ICU

This project uses ICU (International Components for Unicode) to provide:

- full Unicode case folding (case-insensitive comparisons),
- diacritic-insensitive matching (accent stripping),
- better cross-language behavior than simple ASCII-only normalization.

### Installing ICU on Debian/Ubuntu

Use the system packages:

    sudo apt-get update
    sudo apt-get install libicu-dev

This installs ICU headers and libraries (uc, i18n, etc.).

### CMake integration

In your `CMakeLists.txt`, add:

    find_package(ICU REQUIRED COMPONENTS uc i18n)

    target_link_libraries(your_target
        PRIVATE
            ICU::uc
            ICU::i18n
    )

Make sure `your_target` is replaced with the actual target name of your application (e.g. `filemanager`, `gui`, etc.).

After that, ICU headers like `<unicode/unistr.h>` and `<unicode/translit.h>` will be available to your C++ code, and the linker will pick up the corresponding libraries automatically.


## Building

To build the project, you need CMake, Ninja or Make, a C++ compiler supporting C++20, 
and Qt6 development libraries installed.

```bash
# Navigate to your project root (gemini-commander/)
cd path/to/gemini-commander 

mkdir build && cd build
cmake ..
make

# Run the executable (from within the builddir)
./gemini_commander 
# On Windows, it might be: .\gemini_commander.exe