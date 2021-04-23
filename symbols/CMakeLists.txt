add_library(symbols symbols.cpp)

add_library(parse_tree parse_tree.cpp operations.cpp statements.cpp)
target_link_libraries(parse_tree symbols ${llvm_libs})