cmake_minimum_required(VERSION 3.16)
project(MimeExplorer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# QT_MAJOR_VERSION is inherited from parent project
find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS Core Widgets)

# Use SortedDirIterator from main project
set(MAIN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

add_executable(MimeExplorer
    main.cpp
    MainWindow.h
    MainWindow.cpp
    SearchDialog.h
    SearchDialog.cpp
    ${MAIN_SRC_DIR}/SortedDirIterator.h
    ${MAIN_SRC_DIR}/SortedDirIterator.cpp
    ${MAIN_SRC_DIR}/Archives.h
    ${MAIN_SRC_DIR}/Archives.cpp
    ${MAIN_SRC_DIR}/FileIconResolver.h
    ${MAIN_SRC_DIR}/FileIconResolver.cpp
)

target_include_directories(MimeExplorer PRIVATE ${MAIN_SRC_DIR} ${LIBARCHIVE_INCLUDE_DIRS})
target_link_libraries(MimeExplorer PRIVATE Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Widgets ${LIBARCHIVE_LIBRARIES})
