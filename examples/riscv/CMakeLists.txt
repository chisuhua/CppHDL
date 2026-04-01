# RV32I Test CMakeLists.txt

add_executable(rv32i_test rv32i_test.cpp)

target_link_libraries(rv32i_test PRIVATE cpphdl)
target_include_directories(rv32i_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)

set_target_properties(rv32i_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS rv32i_test RUNTIME DESTINATION bin/riscv)

# RV32I Minimal System (Phase 3B)
add_executable(rv32i_minimal rv32i_minimal.cpp)
target_link_libraries(rv32i_minimal PRIVATE cpphdl)
target_include_directories(rv32i_minimal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set_target_properties(rv32i_minimal PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
