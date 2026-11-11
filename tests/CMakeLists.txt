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

# Conflicts of Copy and Move: the quick scan before copying, overwrite policies.
add_executable(copy_conflicts_tests
        test_copy_conflicts.cpp
        ${CMAKE_SOURCE_DIR}/src/CopyConflicts.cpp
)

target_include_directories(copy_conflicts_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(copy_conflicts_tests
        PRIVATE
        Qt6::Core
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(copy_conflicts_tests)

# Archives without the archive module or windows: preview rules, the
# module's items as entries, directories of an archive.
add_executable(archives_tests
        test_archives.cpp
        ${CMAKE_SOURCE_DIR}/src/ArchiveUtils.cpp
        ${CMAKE_SOURCE_DIR}/src/Archives.cpp
)

target_include_directories(archives_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(archives_tests
        PRIVATE
        Qt6::Core
        siplasplas::plugin-api
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(archives_tests)

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
