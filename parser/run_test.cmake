execute_process(COMMAND ${COMPILER_BIN} ${TESTS_DIR}/${TEST} -o test.ll RESULT_VARIABLE COMPILER_RESULT)
if (COMPILER_RESULT)
    message(FATAL_ERROR "compiler error!")
endif()

execute_process(COMMAND llc test.ll -o test.s RESULT_VARIABLE LLC_RESULT)
if (LLC_RESULT)
    message(FATAL_ERROR "llc error!")
endif()

execute_process(COMMAND ${COMPILER} ${TESTS_DIR}/print.c test.s -o test.exe -no-pie
    RESULT_VARIABLE COMPILATION_RESULT)
if (COMPILATION_RESULT)
    message(FATAL_ERROR "compilation error!")
endif()

execute_process(COMMAND ${CMAKE_BINARY_DIR}/test.exe)