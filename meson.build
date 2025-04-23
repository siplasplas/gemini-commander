# meson.build
project('gemini-commander', 'cpp',
        version : '1.0.0',
        default_options : ['warning_level=3', 'cpp_std=c++20'])

# --- Qt5 Configuration Start ---

# Define required Qt5 modules
qt5_modules = ['Core', 'Gui', 'Widgets'] # QApplication needs Core+Gui, QWidget needs Widgets

# Find the Qt5 dependency using pkg-config or other methods
# Checks if version is 5.15 or newer. Adjust if needed.
qt5_dep = dependency('qt5', modules : qt5_modules, version : '>=5.15', required : true)

# Import the qt5 module provided by Meson
# This gives access to functions like qt5.preprocess() for moc, uic, rcc
# Although not strictly needed for *this* very basic main.cpp without Q_OBJECT etc.,
# it's essential for any real Qt project, so include it now.
qt5_mod = import('qt5')

# --- Qt5 Configuration End ---


# Define the executable
gemini_commander_exe = executable(
        'gemini_commander',
        'src/main.cpp',               # Source file is now inside the src/ directory
        dependencies : [qt5_dep],     # Link the executable against the found Qt5 libraries
        install : true
)

# The previous 'test' is removed. Running a GUI app directly as a simple test
# usually doesn't work well or verify much. Proper UI testing is more complex.
# You might add tests later using Qt Test framework or other methods.
# test('basic_compilation_test', gemini_commander_exe) # Example if you just want to check compilation