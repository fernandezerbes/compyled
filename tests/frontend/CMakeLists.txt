enable_testing()

add_executable(frontend_test TokenizerTest.cpp)
target_link_libraries(frontend_test GTest::gtest_main cpy_frontend)

gtest_discover_tests(frontend_test)
