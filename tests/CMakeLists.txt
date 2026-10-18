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

# The editor (qceditor's library) as gemini-commander uses it: loading,
# saving and modification state.
add_executable(editor_tests test_editor.cpp)

target_link_libraries(editor_tests
        PRIVATE
        Qt6::Widgets
        qceditor::editor
        GTest::gtest
)

gtest_discover_tests(editor_tests)
