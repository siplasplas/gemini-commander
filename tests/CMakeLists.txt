find_package(GTest REQUIRED)

add_executable(sizeformat_tests
        test_SizeFormat.cpp
        test_file_hash.cpp
        test_utils.cpp
)

target_link_libraries(sizeformat_tests
        PRIVATE
        core
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(sizeformat_tests)

# The qcodeedit-based editor: loading, saving and modification state.
add_executable(editor_tests
        test_editor.cpp
        ${CMAKE_SOURCE_DIR}/src/editor/editor.cpp
        ${CMAKE_SOURCE_DIR}/src/editor/EncodingChoiceDialog.cpp
        ${CMAKE_SOURCE_DIR}/src/editor/BaseViewer.cpp
        ${CMAKE_SOURCE_DIR}/src/editor/FindBar.cpp
        ${CMAKE_SOURCE_DIR}/src/editor/SyntaxData.cpp
        ${CMAKE_SOURCE_DIR}/src/Config.cpp
        ${CMAKE_SOURCE_DIR}/src/SizeFormat.cpp
)

target_include_directories(editor_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(editor_tests
        PRIVATE
        Qt6::Widgets
        qt-extra
        qcodeedit::qcodeedit
        qcodeedit::kate
        qcodeedit::encoding
        toml_headers
        GTest::gtest
)

gtest_discover_tests(editor_tests)
