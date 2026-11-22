add_executable(frontend_test TestUtils.cpp TokenizerTest.cpp ParserTest.cpp)
target_link_libraries(frontend_test GTest::gtest_main cpy_frontend)

gtest_discover_tests(frontend_test)
