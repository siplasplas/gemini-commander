# meson.build
project('gemini-commander', 'cpp',
        version : '0.1.0',
        default_options : ['warning_level=3', 'cpp_std=c++20'])

# --- Qt5 Configuration Start ---
qt5_modules = ['Core', 'Gui', 'Widgets']
qt5_dep = dependency('qt5', modules : qt5_modules, version : '>=5.15', required : true)
qt5_mod = import('qt5')
# --- Qt5 Configuration End ---

project_sources = [
        'src/main.cpp',
        'src/MainWindow.cpp'
]

# Processing MOC for header files with Q_OBJECT
project_moc_files = qt5_mod.preprocess(
        moc_headers : ['src/MainWindow.h']
)

gemini_commander_exe = executable(
        'gemini_commander',
        project_sources,
        project_moc_files,
        dependencies : [qt5_dep],
        install : true
)

# Testing  - future implementation