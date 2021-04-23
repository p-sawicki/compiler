add_library(lexer lexer.cpp)
target_link_libraries(lexer symbols)

add_executable(lexer_test test.cpp)
target_link_libraries(lexer_test lexer gtest_main)
add_test(NAME lexer_test COMMAND lexer_test)