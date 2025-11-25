find_package(GTest REQUIRED)

add_executable(sizeformat_tests
        ${CMAKE_SOURCE_DIR}/src/SizeFormat.cpp
        test_SizeFormat.cpp
)

target_link_libraries(sizeformat_tests
        PRIVATE
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(sizeformat_tests)