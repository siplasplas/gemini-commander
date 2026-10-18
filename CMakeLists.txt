cmake_minimum_required(VERSION 3.24)

project(gemini-commander VERSION 2.1.1 LANGUAGES CXX)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Werror=return-type>)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets DBus Concurrent Network)

# backward-cpp declares compatibility with older CMake releases.  CMake 4
# requires opting into that legacy policy level while configuring it.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

include(FetchContent)
# Fetched dependencies are only built into gemini-commander; their own install
# rules would add libraries, headers and CMake packages to ours.
set(fetch_exclude)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
    set(fetch_exclude EXCLUDE_FROM_ALL)
endif()

FetchContent_Declare(
        backward
        GIT_REPOSITORY https://github.com/bombela/backward-cpp.git
        GIT_TAG 0bfd0a07a61551413ccd2ab9a9099af3bad40681
        GIT_SHALLOW TRUE
        ${fetch_exclude}
)
FetchContent_MakeAvailable(backward)

# MruTabWidget, QxFileDialog and other shared widgets come from qt-extra
# (github.com/siplasplas/qt-extra). An installed qt-extra 2.7 or later is used
# (qxMigrateToSiplasplas() moves older versions' files); otherwise CMake fetches
# the tag below and builds it into gemini-commander.
FetchContent_Declare(
        qt-extra
        GIT_REPOSITORY https://github.com/siplasplas/qt-extra.git
        GIT_TAG v2.7.0
        GIT_SHALLOW TRUE
        ${fetch_exclude}
        # The rest of the line goes to find_package(), so this comes last.
        FIND_PACKAGE_ARGS 2.7
)
FetchContent_MakeAvailable(qt-extra)

# The text editor and the viewer for small files are qcodeedit widgets
# (github.com/siplasplas/qcodeedit, LGPL-3.0-or-later), highlighted with Kate
# syntax definitions that qcodeedit-katedata downloads at run time, and files
# in any encoding (UTF or legacy code pages) through qcodeedit-encoding, which
# needs cpg (fetched by qcodeedit when not installed). Installed qcodeedit
# packages 1.13.0 or later are used (Kate data in the shared
# ~/.local/share/kate-data, migrateDataRoot()); otherwise the tag below is
# fetched and built into gemini-commander, without its demo and tests.
# FETCHCONTENT_SOURCE_DIR_QCODEEDIT (a local qcodeedit checkout) skips the
# installed packages: they would bring targets with the same names.
if(NOT FETCHCONTENT_SOURCE_DIR_QCODEEDIT)
    find_package(qcodeedit-katedata 1.13.0 QUIET)
    find_package(qcodeedit-encoding 1.13.0 QUIET)
endif()
if(NOT TARGET qcodeedit::katedata OR NOT TARGET qcodeedit::encoding)
    set(QCE_BUILD_DEMO OFF)
    set(QCE_BUILD_TESTS OFF)
    set(QCE_BUILD_KATE ON)
    set(QCE_BUILD_KATE_DOWNLOADER ON)
    set(QCE_BUILD_ENCODING ON)
    FetchContent_Declare(
            qcodeedit
            GIT_REPOSITORY https://github.com/siplasplas/qcodeedit.git
            GIT_TAG v1.13.0
            GIT_SHALLOW TRUE
            ${fetch_exclude}
    )
    FetchContent_MakeAvailable(qcodeedit)
endif()

# Header-only TOML parser; an installed toml++ 3 is used, otherwise it is fetched.
FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG e7aaccca3fa3dbde9818ab8313250f3da4976e37
        ${fetch_exclude}
        FIND_PACKAGE_ARGS 3
)
FetchContent_MakeAvailable(tomlplusplus)
# toml++ is used header-only. Some distributions package it as a compiled
# library, so only its headers are taken to avoid a needless runtime dependency.
add_library(toml_headers INTERFACE)
target_include_directories(toml_headers INTERFACE
        $<TARGET_PROPERTY:tomlplusplus::tomlplusplus,INTERFACE_INCLUDE_DIRECTORIES>)

# Viewer for large files (github.com/ideorg/textviewers). Its sources are
# compiled into gemini-commander; its own CMakeLists.txt builds demo programs,
# so SOURCE_SUBDIR points at a directory without one to only download it.
FetchContent_Declare(
        textviewers
        GIT_REPOSITORY https://github.com/ideorg/textviewers.git
        GIT_TAG 99ff988242b4992223176c850ed7ec0d18a007bf
        SOURCE_SUBDIR none
)
FetchContent_MakeAvailable(textviewers)
set(TEXTVIEWERS_DIR ${textviewers_SOURCE_DIR})

find_package(ICU REQUIRED COMPONENTS uc i18n)
# MruTabWidget and other shared widgets come from the qt-extra library
# (https://github.com/siplasplas/qt-extra.git, tag v2.7.0), installed to the system.
# See README "Building" for how to build and install it first. 2.7 moves older
# versions' files to ~/.config/siplasplas (qxMigrateToSiplasplas).
find_package(qt-extra 2.7 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(BOTAN botan-2)
if(NOT BOTAN_FOUND)
    pkg_check_modules(BOTAN REQUIRED botan-3)
endif()
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
find_package(nlohmann_json 3.2.0 REQUIRED)

add_library(core
        src/utils.cpp
        src/SizeFormat.cpp
)

target_include_directories(core
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${BOTAN_INCLUDE_DIRS}
)

target_link_libraries(core PRIVATE
        ${BOTAN_LIBRARIES}
)

target_compile_options(core PRIVATE
        ${BOTAN_CFLAGS_OTHER}
)

add_executable(toml_json_converter
        tools/toml_json_converter.cpp
)

target_include_directories(toml_json_converter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(toml_json_converter PRIVATE
        core
        toml_headers
        nlohmann_json::nlohmann_json
)

add_executable(stub_generator
        tools/stub_generator.cpp
)

target_include_directories(stub_generator PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(stub_generator PRIVATE
        core
        toml_headers
)

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/MainWindow.cpp
        src/CopyDialog.cpp
        src/CopyDialog.h
        src/FilePanel.cpp
        src/FilePanel.h
        src/SearchDialog.cpp
        src/SearchDialog.h
        src/SearchWorker.cpp
        src/SearchIgnore.cpp
        src/SearchIgnore.h
        src/PanelFilter.cpp
        src/PanelFilter.h
        src/GitignoreOptionsWidget.cpp
        src/GitignoreOptionsWidget.h
        src/DistroInfo.cpp
        src/DistroInfo.h
        src/DistroInfoDialog.cpp
        src/DistroInfoDialog.h
        src/ChmodDialog.cpp
        src/ChmodDialog.h
        src/StringListEditorDialog.cpp
        src/StringListEditorDialog.h
        src/PackDialog.cpp
        src/PackDialog.h
        src/MultiRenameDialog.cpp
        src/MultiRenameDialog.h
        src/FileOperationProgressDialog.cpp
        src/FileOperationProgressDialog.h
        src/FileOperations.cpp
        src/FileOperations.h
        src/SearchWorker.h
        src/editor/editor.cpp
        src/editor/editor.h
        src/editor/FindBar.cpp
        src/editor/FindBar.h
        src/editor/ReplaceCommand.h
        src/editor/SyntaxData.cpp
        src/editor/SyntaxData.h
        src/editor/EditorFrame.cpp
        src/editor/EditorFrame.h
        src/editor/mainheader.cpp
        src/widgets/SizeCalculationWidget.cpp
        src/utils/Ev.cpp
        src/editor/BaseViewer.cpp
        src/editor/BaseViewer.h
        src/editor/ViewerFrame.cpp
        src/editor/ViewerFrame.h
        src/editor/ViewerWidget.cpp
        src/editor/ViewerWidget.h
        src/editor/HexViewWidget.cpp
        src/editor/HexViewWidget.h
        ${TEXTVIEWERS_DIR}/wid/TextViewer.cpp
        ${TEXTVIEWERS_DIR}/wid/PaintArea.cpp
        ${TEXTVIEWERS_DIR}/logic/AbstractView.cpp
        ${TEXTVIEWERS_DIR}/logic/Wrap.cpp
        ${TEXTVIEWERS_DIR}/logic/LineView.cpp
        ${TEXTVIEWERS_DIR}/logic/ByteView.cpp
        ${TEXTVIEWERS_DIR}/logic/LineDeque.cpp
        ${TEXTVIEWERS_DIR}/logic/ByteDeque.cpp
        ${TEXTVIEWERS_DIR}/logic/ByteDocument.cpp
        ${TEXTVIEWERS_DIR}/logic/ChangeableDocument.cpp
        ${TEXTVIEWERS_DIR}/logic/LineIndexedDocument.cpp
        ${TEXTVIEWERS_DIR}/misc/selection.cpp
        src/Config.cpp
        src/Config.h
        src/ConfigDialog.cpp
        src/ConfigDialog.h
        src/FilePaneWidget.cpp
        src/FilePaneWidget.h
        src/FunctionBar.cpp
        src/FunctionBar.h
        src/SearchEdit.h
        src/SortedDirIterator.cpp
        src/SortedDirIterator.h
        src/Archives.cpp
        src/Archives.h
        src/FileIconResolver.cpp
        src/FileIconResolver.h
        src/keys/KeyMap.cpp
        src/keys/KeyMap.h
        src/SearchEdit.cpp
        src/keys/KeyRouter.cpp
        src/keys/ObjectRegistry.cpp
        src/qutils.cpp
        src/quitls.h
        src/udisks/UDisksDeviceManager.cpp
        src/udisks/UDisksDeviceManager.h
        src/mounts/ProcMountsManager.cpp
        src/mounts/ProcMountsManager.h
        src/VerticalToolButton.cpp
        src/VerticalToolButton.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${TEXTVIEWERS_DIR}
        ${LIBARCHIVE_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        core
        Backward::Object
        Qt6::Widgets
        Qt6::DBus
        Qt6::Concurrent
        qt-extra
        qcodeedit::qcodeedit
        qcodeedit::kate
        qcodeedit::katedata
        qcodeedit::encoding
        toml_headers
        ICU::uc ICU::i18n
        ${LIBARCHIVE_LIBRARIES}
 )

# Keep enough information in the executable for backward-cpp to resolve source
# files and line numbers in crash reports, including optimized builds.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -g -fno-omit-frame-pointer)
endif()

# Generate git_version.h at build time (not configure time)
set(GIT_VERSION_HEADER "${CMAKE_BINARY_DIR}/git_version.h")
add_custom_target(git_version ALL
    COMMAND ${CMAKE_COMMAND}
        -DGIT_VERSION_HEADER=${GIT_VERSION_HEADER}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateGitVersion.cmake
    BYPRODUCTS ${GIT_VERSION_HEADER}
    COMMENT "Generating git version header"
)
add_dependencies(${PROJECT_NAME} git_version)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})

target_compile_definitions(${PROJECT_NAME} PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
        APP_BUILD_TYPE="$<IF:$<CONFIG:Debug>,Debug,Release>"
)

qt_add_resources(APP_RESOURCES res/resources.qrc)
target_sources(gemini-commander PRIVATE ${APP_RESOURCES})

# Installation
include(GNUInstallDirs)

# Install executable
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install .desktop file
install(FILES res/gemini-commander.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Install SVG icon (scalable)
install(FILES res/icons/gemini-commander.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME gemini-commander.svg)

# Generate and install PNG icons from SVG at install time
find_program(INKSCAPE_EXECUTABLE inkscape)
find_program(RSVG_CONVERT_EXECUTABLE rsvg-convert)

if(INKSCAPE_EXECUTABLE)
    set(SVG_CONVERTER "${INKSCAPE_EXECUTABLE}")
    set(SVG_CONVERTER_ARGS "--export-type=png --export-filename=")
    set(SVG_CONVERTER_SIZE_ARG "--export-width=")
elseif(RSVG_CONVERT_EXECUTABLE)
    set(SVG_CONVERTER "${RSVG_CONVERT_EXECUTABLE}")
    set(SVG_CONVERTER_ARGS "-o ")
    set(SVG_CONVERTER_SIZE_ARG "-w ")
endif()

if(SVG_CONVERTER)
    foreach(size 16 24 32 48 64 128 256)
        install(CODE "
            set(ICON_DIR \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps\")
            file(MAKE_DIRECTORY \"\${ICON_DIR}\")
            set(OUTPUT_FILE \"\${ICON_DIR}/gemini-commander.png\")
            set(INPUT_FILE \"${CMAKE_CURRENT_SOURCE_DIR}/res/icons/gemini-commander.svg\")
            if(\"${INKSCAPE_EXECUTABLE}\" STREQUAL \"${SVG_CONVERTER}\")
                execute_process(
                    COMMAND ${SVG_CONVERTER} \"\${INPUT_FILE}\" --export-type=png --export-filename=\${OUTPUT_FILE} --export-width=${size} --export-height=${size}
                    RESULT_VARIABLE result
                )
            else()
                execute_process(
                    COMMAND ${SVG_CONVERTER} -w ${size} -h ${size} -o \"\${OUTPUT_FILE}\" \"\${INPUT_FILE}\"
                    RESULT_VARIABLE result
                )
            endif()
            if(result EQUAL 0)
                message(STATUS \"Generated: \${OUTPUT_FILE}\")
            else()
                message(WARNING \"Failed to generate: \${OUTPUT_FILE}\")
            endif()
        ")
    endforeach()
else()
    message(WARNING "No SVG converter found (inkscape or rsvg-convert). PNG icons will not be installed.")
endif()

enable_testing()
add_subdirectory(tests)

# Tools
add_subdirectory(tools/mime)
