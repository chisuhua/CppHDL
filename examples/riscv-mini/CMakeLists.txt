# RV32I Mini CMakeLists.txt
# ISA 组件验证:RegFile + ALU + Decoder + Branch + Load/Store
#
# 两个 helper 函数消除 19 个 add_executable 目标的样板代码,
# 模式对齐 tests/CMakeLists.txt 的 add_catch_test()。

# ISA 单元测试(自带 main(),无 Catch2)
function(riscv_mini_unit_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE cpphdl)
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv)
    set_target_properties(${TEST_NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endfunction()

# Catch2 集成测试(链接 catch_amalgamated)
# 重要: include 顺序敏感 — include/cpu/pipeline 必须在 src 之前,
# 否则 #include "rv32i_decoder.h" 优先匹配到 examples/riscv-mini/src/rv32i_decoder.h,
# 之后 include/cpu/pipeline/rv32i_decoder.h 被 include 时触发 struct 重定义。
# include/cpu/pipeline 对 4 个非-pipeline 测试是无害的多余路径。
function(riscv_mini_catch_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
    target_link_libraries(${TEST_NAME} PRIVATE cpphdl)
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(${TEST_NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endfunction()

# === ISA 组件验证 (Phase 1-3) ===
riscv_mini_unit_test(rv32i_phase1_test rv32i_phase1_test.cpp)
riscv_mini_unit_test(rv32i_phase2_test rv32i_phase2_test.cpp)
riscv_mini_unit_test(rv32i_phase3_test rv32i_phase3_test.cpp)

# === Pipeline 集成测试 (Catch2) ===
riscv_mini_catch_test(test_pipeline_exec tests/test_pipeline_exec.cpp)
riscv_mini_catch_test(test_soc_hello_world tests/test_soc_hello_world.cpp)
riscv_mini_catch_test(test_riscv_tests tests/test_riscv_tests.cpp)
riscv_mini_catch_test(test_address_decoder tests/test_riscv_tests_soc.cpp)

# === Pipeline 单元 ===
# 异常: test_riscv_tests_pipeline 额外需要 examples/riscv-mini/tests/ 在 include 路径
riscv_mini_catch_test(test_riscv_tests_pipeline tests/test_riscv_tests_pipeline.cpp)
target_include_directories(test_riscv_tests_pipeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

riscv_mini_catch_test(test_pipeline_smoke tests/test_pipeline_smoke.cpp)
riscv_mini_catch_test(test_pipeline_minimal tests/test_pipeline_minimal.cpp)
riscv_mini_catch_test(test_pipeline_debug tests/test_pipeline_debug.cpp)
riscv_mini_catch_test(test_pipeline_debug3 tests/test_pipeline_debug3.cpp)
riscv_mini_catch_test(test_pipeline_debug4 tests/test_pipeline_debug4.cpp)
riscv_mini_catch_test(test_pipeline_debug5 tests/test_pipeline_debug5.cpp)
riscv_mini_catch_test(test_pipeline_debug6 tests/test_pipeline_debug6.cpp)
riscv_mini_catch_test(test_pipeline_wrapper tests/test_pipeline_wrapper.cpp)
riscv_mini_catch_test(test_wrapper_simple tests/test_wrapper_simple.cpp)
riscv_mini_catch_test(test_wrapper_ifstage tests/test_wrapper_ifstage.cpp)
riscv_mini_catch_test(test_wrapper_pipeline tests/test_wrapper_pipeline.cpp)
riscv_mini_catch_test(test_rv32i_pipeline_device tests/test_rv32i_pipeline_device.cpp)
riscv_mini_catch_test(test_ifstage_device tests/test_ifstage_device.cpp)

# === 安装 (Phase 1-3 + 5 个核心 Pipeline 测试) ===
install(TARGETS rv32i_phase1_test rv32i_phase2_test rv32i_phase3_test
              test_pipeline_exec test_soc_hello_world test_riscv_tests
              test_address_decoder test_riscv_tests_pipeline test_pipeline_smoke
        RUNTIME DESTINATION bin/riscv-mini)
