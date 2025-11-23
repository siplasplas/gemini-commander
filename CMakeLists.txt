cmake_minimum_required(VERSION 3.16)

project(gemini-commander VERSION 1.0 LANGUAGES CXX)

option(USE_QT5 "Use Qt5 and KF5 instead of Qt6 and KF6" OFF)

if(USE_QT5)
    set(QT_MAJOR_VERSION 5)
    set(KF_MAJOR_VERSION 5)
    set(QT_MIN_VERSION "5.15.0")
    set(KF_MIN_VERSION "5.78.0")
    set(ECM_MIN_VERSION "5.78.0")
    set(includeDir /usr/include/KF5/KTextEditor /usr/include/KF5/KSyntaxHighlighting
            /usr/include/KF5/KParts /usr/include/KF5/KXmlGui)
else()
    set(QT_MAJOR_VERSION 6)
    set(KF_MAJOR_VERSION 6)
    set(QT_MIN_VERSION "6.0.0")
    set(KF_MIN_VERSION "6.0.0")
    set(ECM_MIN_VERSION "6.0.0")
    set(includeDir /usr/include/KF6/KTextEditor /usr/include/KF6/KSyntaxHighlighting
            /usr/include/KF6/KParts /usr/include/KF6/KXmlGui)
endif()

find_package(ECM ${ECM_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} REQUIRED COMPONENTS Widgets)
find_package(KF${KF_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS TextEditor CoreAddons I18n WidgetsAddons WindowSystem KIO Parts)
find_package(ICU REQUIRED COMPONENTS uc i18n)

set(TOMLPLUSPLUS_INCLUDE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tomlplusplus/include
)
add_executable(${PROJECT_NAME}
        src/main.cpp
        src/MainWindow.cpp
        src/FilePanel.cpp
        src/FilePanel.h
        src/editor/editor.cpp
        src/editor/editor.h
        src/editor/EditorFrame.cpp
        src/editor/EditorFrame.h
        src/editor/mainheader.cpp
        src/widgets/mrutabwidget.cpp
        src/widgets/Ev.cpp
        src/editor/BaseViewer.cpp
        src/editor/BaseViewer.h
        src/editor/Viewer.cpp
        src/editor/Viewer.h
        external/textviewers/wid/TextViewer.cpp
        external/textviewers/wid/PaintArea.cpp
        external/textviewers/logic/AbstractView.cpp
        external/textviewers/logic/Wrap.cpp
        external/textviewers/logic/LineView.cpp
        external/textviewers/logic/ByteView.cpp
        external/textviewers/logic/LineDeque.cpp
        external/textviewers/logic/ByteDeque.cpp
        external/textviewers/logic/ByteDocument.cpp
        external/textviewers/logic/ChangeableDocument.cpp
        external/textviewers/logic/LineIndexedDocument.cpp
        external/textviewers/misc/selection.cpp
        src/Config.cpp
        src/Config.h
)

target_include_directories(${PROJECT_NAME} PRIVATE ${includeDir}
        ${CMAKE_SOURCE_DIR}/external/textviewers
        ${TOMLPLUSPLUS_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_MAJOR_VERSION}::Widgets
        KF${KF_MAJOR_VERSION}::TextEditor
        KF${KF_MAJOR_VERSION}::CoreAddons
        KF${KF_MAJOR_VERSION}::I18n
        KF${KF_MAJOR_VERSION}::WidgetsAddons
        KF${KF_MAJOR_VERSION}::WindowSystem
        KF${KF_MAJOR_VERSION}::KIOCore
        KF${KF_MAJOR_VERSION}::KIOFileWidgets
        KF${KF_MAJOR_VERSION}::KIOWidgets
        KF${KF_MAJOR_VERSION}::Parts
        ICU::uc ICU::i18n
 )