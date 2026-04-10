# tests/CMakeLists.txt
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

function(add_catch_test TEST_NAME)
    set(_srcs ${ARGN})
    list(APPEND _srcs ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp)
    add_executable(${TEST_NAME} ${_srcs})
    target_include_directories(${TEST_NAME} PRIVATE
        ${CATCH_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
    )
    if(TARGET cpphdl)
        target_link_libraries(${TEST_NAME} cpphdl)
    endif()
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "base")
endfunction()

enable_testing()

# ============================================================================
# CPU 模块测试 (大部分被暂时禁用 - API 兼容性问题)
# ============================================================================

add_catch_test(test_branch_predictor cpu/test_branch_predictor_v2.cpp)
add_catch_test(test_forwarding cpu/test_forwarding.cpp)
add_catch_test(test_hazard cpu/test_hazard_complete.cpp)

# ============================================================================
# Cache 测试 (被暂时禁用 - API 兼容性问题)
# ============================================================================

add_catch_test(test_cache_full chlib/test_cache_full.cpp)
add_catch_test(test_cache_complete chlib/test_cache_complete.cpp)

# ============================================================================
# RISC-V 集成测试 (被暂时禁用 - API 兼容性问题)
# ============================================================================

add_catch_test(test_cache_integration riscv-integration/test_cache_pipeline_integration.cpp)
add_catch_test(test_phase4 riscv-integration/test_phase4_integration.cpp)
add_catch_test(test_riscv_pipeline riscv-integration/test_pipeline_integration.cpp)

# Add riscv-mini src directory for headers
target_include_directories(test_phase4 PRIVATE ${PROJECT_SOURCE_DIR}/examples/riscv-mini/src)
target_include_directories(test_cache_integration PRIVATE ${PROJECT_SOURCE_DIR}/examples/riscv-mini/src)

# ============================================================================
# chlib 库测试 (26 个测试文件)
# ============================================================================

# 基础功能测试
add_catch_test(test_arithmetic chlib/test_arithmetic.cpp)
add_catch_test(test_arithmetic_advance chlib/test_arithmetic_advance.cpp)
add_catch_test(test_axi4lite chlib/test_axi4lite.cpp)
add_catch_test(test_bitwise chlib/test_bitwise.cpp)
add_catch_test(test_cache_simple chlib/test_cache_simple.cpp)
add_catch_test(test_cam chlib/test_cam.cpp)
add_catch_test(test_combinational chlib/test_combinational.cpp)
add_catch_test(test_converter chlib/test_converter.cpp)
add_catch_test(test_fifo chlib/test_fifo.cpp)
add_catch_test(test_logic chlib/test_logic.cpp)
add_catch_test(test_memory chlib/test_memory.cpp)
add_catch_test(test_onehot chlib/test_onehot.cpp)
add_catch_test(test_pipeline_lib chlib/test_pipeline.cpp)
add_catch_test(test_selector_arbiter chlib/test_selector_arbiter.cpp)
add_catch_test(test_selector_priority chlib/test_selector_priority.cpp)
add_catch_test(test_selector_round_robin chlib/test_selector_round_robin.cpp)
add_catch_test(test_sequential chlib/test_sequential.cpp)
add_catch_test(test_stream chlib/test_stream.cpp)
add_catch_test(test_switch chlib/test_switch.cpp)

# Cache 相关测试
add_catch_test(test_cache_pipeline chlib/test_cache_pipeline_integration.cpp)

# Phase 4 集成测试 (chlib 版本)
add_catch_test(test_phase4_chlib chlib/test_phase4_integration.cpp)

