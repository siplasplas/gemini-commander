find_package(GTest REQUIRED)

add_executable(sizeformat_tests
        test_SizeFormat.cpp
        test_file_hash.cpp
)

target_link_libraries(sizeformat_tests
        PRIVATE
        core
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(sizeformat_tests)