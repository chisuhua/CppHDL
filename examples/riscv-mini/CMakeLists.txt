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

# RISC-V RV32UI Tests through actual Rv32iPipeline hardware (Phase F Wave 3)
add_executable(test_riscv_tests_pipeline tests/test_riscv_tests_pipeline.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_riscv_tests_pipeline PRIVATE cpphdl)
target_include_directories(test_riscv_tests_pipeline PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_riscv_tests_pipeline PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# Smoke test: Pipeline + TCM instantiation in single context
add_executable(test_pipeline_smoke tests/test_pipeline_smoke.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_pipeline_smoke PRIVATE cpphdl)
target_include_directories(test_pipeline_smoke PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_pipeline_smoke PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

install(TARGETS rv32i_phase1_test rv32i_phase2_test rv32i_phase3_test test_pipeline_exec test_soc_hello_world test_riscv_tests test_address_decoder test_riscv_tests_pipeline test_pipeline_smoke RUNTIME DESTINATION bin/riscv-mini)

add_executable(test_pipeline_minimal tests/test_pipeline_minimal.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_pipeline_minimal PRIVATE cpphdl)
target_include_directories(test_pipeline_minimal PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_pipeline_minimal PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# Debug test: Incremental pipeline crash isolation
add_executable(test_pipeline_debug tests/test_pipeline_debug.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_pipeline_debug PRIVATE cpphdl)
target_include_directories(test_pipeline_debug PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_pipeline_debug PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(test_pipeline_debug3 tests/test_pipeline_debug3.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_pipeline_debug3 PRIVATE cpphdl)
target_include_directories(test_pipeline_debug3 PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_pipeline_debug3 PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(test_pipeline_debug4 tests/test_pipeline_debug4.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_pipeline_debug4 PRIVATE cpphdl)
target_include_directories(test_pipeline_debug4 PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_pipeline_debug4 PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(test_pipeline_debug5 tests/test_pipeline_debug5.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_pipeline_debug5 PRIVATE cpphdl)
target_include_directories(test_pipeline_debug5 PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_pipeline_debug5 PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(test_pipeline_debug6 tests/test_pipeline_debug6.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
target_link_libraries(test_pipeline_debug6 PRIVATE cpphdl)
target_include_directories(test_pipeline_debug6 PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(test_pipeline_debug6 PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
