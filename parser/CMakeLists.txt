add_library(parser parser.cpp)
target_link_libraries(parser lexer parse_tree)

add_executable(parser_test test.cpp)
target_link_libraries(parser_test parser gtest_main)
add_test(NAME parser_test COMMAND parser_test)

function(test file result)
    add_test(NAME test_${file} 
        COMMAND ${CMAKE_COMMAND}
        -DCOMPILER=${CMAKE_C_COMPILER}
        -DCOMPILER_BIN=${CMAKE_BINARY_DIR}/compiler
        -DTESTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests
        -DTEST=${file}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake)
    set_tests_properties(test_${file} 
        PROPERTIES PASS_REGULAR_EXPRESSION ${result})
endfunction(test)

test(hello_world "Hello world!\n")
test(global_variable "1\n420\n")
test(fibonacci "0\n1\n1\n2\n3\n5\n8\n13\n21\n34\n")
test(complex_arithmetic "4\n6\n-0.5\n-22\n-5\n10\n-4\n6")
test(abs "2\n3.45\n5\n")
test(type_conversion "1\n5\n1\n2\n2\n2\n0\n")
test(if_else "0\n1\n2\n5\n5\n")
test(variable_redefinition "1\n7\n5\n3\n")