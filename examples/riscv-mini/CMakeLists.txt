# RV32I Mini CMakeLists.txt
# ISA 组件验证：RegFile + ALU + Decoder + Branch + Load/Store

add_executable(rv32i_phase1_test rv32i_phase1_test.cpp)
target_link_libraries(rv32i_phase1_test PRIVATE cpphdl)
target_include_directories(rv32i_phase1_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv)
set_target_properties(rv32i_phase1_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(rv32i_phase2_test rv32i_phase2_test.cpp)
target_link_libraries(rv32i_phase2_test PRIVATE cpphdl)
target_include_directories(rv32i_phase2_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv)
set_target_properties(rv32i_phase2_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(rv32i_phase3_test rv32i_phase3_test.cpp)
target_link_libraries(rv32i_phase3_test PRIVATE cpphdl)
target_include_directories(rv32i_phase3_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv)
set_target_properties(rv32i_phase3_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# Pipeline 集成测试
add_executable(test_pipeline_exec tests/test_pipeline_exec.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_pipeline_exec PRIVATE cpphdl)
target_include_directories(test_pipeline_exec PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_pipeline_exec PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# SoC Hello World 测试
add_executable(test_soc_hello_world tests/test_soc_hello_world.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_soc_hello_world PRIVATE cpphdl)
target_include_directories(test_soc_hello_world PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_soc_hello_world PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# RISC-V Tests ISA 集成测试
add_executable(test_riscv_tests tests/test_riscv_tests.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_riscv_tests PRIVATE cpphdl)
target_include_directories(test_riscv_tests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_riscv_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# Address Decoder + TCM 测试 (验证地址映射修复)
add_executable(test_address_decoder tests/test_riscv_tests_soc.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_address_decoder PRIVATE cpphdl)
target_include_directories(test_address_decoder PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_address_decoder PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

install(TARGETS rv32i_phase1_test rv32i_phase2_test rv32i_phase3_test test_pipeline_exec test_soc_hello_world test_riscv_tests test_address_decoder RUNTIME DESTINATION bin/riscv-mini)
