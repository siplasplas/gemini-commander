cmake_minimum_required(VERSION 3.16)

project(gemini-commander VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MainWindow.cpp
        src/Panel.cpp
        src/Panel.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)