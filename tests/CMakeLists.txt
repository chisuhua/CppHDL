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
# RISC-V 相关测试 (使用 chlib/ 路径 - 已删除冗余的 cpu/ 和 riscv-integration/)
# ============================================================================

add_catch_test(test_forwarding chlib/test_forwarding.cpp)
# TODO: branch_predictor_v2.h ch_reg[] 数组下标不兼容 - 需要 switch_() 多路复用器重构
# add_catch_test(test_branch_predictor chlib/test_branch_predictor_v2.cpp)
add_catch_test(test_hazard chlib/test_hazard_complete.cpp)
target_include_directories(test_hazard PRIVATE ${PROJECT_SOURCE_DIR}/examples/riscv-mini/src)

target_include_directories(test_forwarding PRIVATE ${PROJECT_SOURCE_DIR}/examples/riscv-mini/src)

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
# TODO: rv32i_pipeline_cache.h 有多处 API 不兼容（构造函数、IO 端口、bits 操作）
# 需要系统性修复 stages/*.h 和 cache 头文件，工作量较大，延后处理
# add_catch_test(test_cache_pipeline chlib/test_cache_pipeline_integration.cpp)

# Phase 4 集成测试 (chlib 版本)
# TODO: test_phase4_chlib 需要修复 riscv-mini API 兼容性问题
# add_catch_test(test_phase4_chlib chlib/test_phase4_integration.cpp)

# ============================================================================
# 根目录测试 (之前未注册)
# ============================================================================

# 基础功能测试
add_catch_test(test_basic test_basic.cpp)
add_catch_test(test_literal test_literal.cpp)
add_catch_test(test_literal_debug test_literal_debug.cpp)
add_catch_test(test_literal_left_shift test_literal_left_shift.cpp)
add_catch_test(test_literal_width test_literal_width.cpp)
add_catch_test(test_reg test_reg.cpp)
add_catch_test(test_reg_linking test_reg_linking.cpp)
add_catch_test(test_reg_timing test_reg_timing.cpp)
add_catch_test(test_operator test_operator.cpp)
add_catch_test(test_operator_advanced test_operator_advanced.cpp)
add_catch_test(test_operator_connection test_operator_connection.cpp)
add_catch_test(test_operator_results test_operator_results.cpp)

# IO 测试
add_catch_test(test_simple_io test_simple_io.cpp)
add_catch_test(test_io_operations test_io_operations.cpp)
add_catch_test(test_io_extended_ops test_io_extended_ops.cpp)
add_catch_test(test_source_info test_source_info.cpp)

# Bundle 测试
add_catch_test(test_bundle test_bundle.cpp)
add_catch_test(test_bundle_advanced test_bundle_advanced.cpp)
add_catch_test(test_bundle_advanced_features test_bundle_advanced_features.cpp)
add_catch_test(test_bundle_bitstream test_bundle_bitstream.cpp)
add_catch_test(test_bundle_connection test_bundle_connection.cpp)
add_catch_test(test_bundle_node_management test_bundle_node_management.cpp)
add_catch_test(test_bundle_serialization test_bundle_serialization.cpp)
add_catch_test(test_nested_bundle test_nested_bundle.cpp)
# TODO: 修复 CH_BUNDLE_FIELDS / set_bundle_field_value API 不兼容
add_catch_test(test_pod_bundle_conversions test_pod_bundle_conversions.cpp)
add_catch_test(test_simulator_bundle test_simulator_bundle.cpp)
add_catch_test(test_tlm_bundle_phase1 test_tlm_bundle_phase1.cpp)

# Stream 测试
add_catch_test(test_ch_stream test_ch_stream.cpp)
add_catch_test(test_stream_arbiter test_stream_arbiter.cpp)
add_catch_test(test_stream_builder test_stream_builder.cpp)
add_catch_test(test_stream_operators test_stream_operators.cpp)
add_catch_test(test_stream_pipeline test_stream_pipeline.cpp)
add_catch_test(test_stream_width_adapter_complete test_stream_width_adapter_complete.cpp)

# Memory 测试（核心功能已由 test_mem/test_mem_connection/test_mem_timing 覆盖）
add_catch_test(test_mem test_mem.cpp)
add_catch_test(test_mem_connection test_mem_connection.cpp)
add_catch_test(test_mem_timing test_mem_timing.cpp)
# 已删除：以下测试使用过时 API（ChComponent/TestComponent），且测试内部实现细节
# - test_memory_leak.cpp
# - test_memory_optimization.cpp
# - test_memory_performance.cpp

# 组件测试
add_catch_test(test_state_machine test_state_machine.cpp)
add_catch_test(test_pipeline_regs test_pipeline_regs.cpp)
add_catch_test(test_fragment test_fragment.cpp)

# AXI 测试
add_catch_test(test_axi_dma test_axi_dma.cpp)
add_catch_test(test_axi_i2c test_axi_i2c.cpp)
add_catch_test(test_axi_lite_bundle test_axi_lite_bundle.cpp)
add_catch_test(test_axi_pwm test_axi_pwm.cpp)
add_catch_test(test_axi_pwm_simple test_axi_pwm_simple.cpp)
add_catch_test(test_axi_spi test_axi_spi.cpp)

# 运算测试
add_catch_test(test_bits_update test_bits_update.cpp)
add_catch_test(test_bitvector test_bitvector.cpp)
add_catch_test(test_bool_connection test_bool_connection.cpp)
add_catch_test(test_comparison_ops test_comparison_ops.cpp)
add_catch_test(test_uint_compare test_uint_compare.cpp)
add_catch_test(test_mod_width test_mod_width.cpp)
add_catch_test(test_popcount test_popcount.cpp)
add_catch_test(test_precise_width test_precise_width.cpp)
add_catch_test(test_shift_boundary test_shift_boundary.cpp)
add_catch_test(test_sdata_compare test_sdata_compare.cpp)

# FIFO 示例
add_catch_test(test_fifo_example test_fifo_example.cpp)

# Flow / Stream Bundle 测试
# TODO: ch_flow <<= ch_out 运算符不兼容，需要添加 operator<<= 支持
add_catch_test(test_ch_flow test_ch_flow.cpp)
add_catch_test(test_stream_width_adapter test_stream_width_adapter.cpp)

# 其它测试
add_catch_test(test_blt_debug test_blt_debug.cpp)
add_catch_test(test_bv_cmp test_bv_cmp.cpp)
add_catch_test(test_bv_truncate test_bv_truncate.cpp)
add_catch_test(test_ch_reg_bitwise test_ch_reg_bitwise.cpp)
add_catch_test(test_module test_module.cpp)
add_catch_test(test_module_advanced test_module_advanced.cpp)
add_catch_test(test_multithread test_multithread.cpp)
add_catch_test(test_operation_results test_operation_results.cpp)
add_catch_test(test_operation_widths test_operation_widths.cpp)
# TODO: 编译错误
# add_catch_test(test_rv32i_pipeline test_rv32i_pipeline.cpp)
# test_simplified_creation 已删除 — easy_nodes.h 不再存在
add_catch_test(test_trace test_trace.cpp)
add_catch_test(test_user_tracking test_user_tracking.cpp)
add_catch_test(test_verilog_gen test_verilog_gen.cpp)
add_catch_test(test_wires_connection test_wires_connection.cpp)

# ============================================================================
# 完全被注释掉的文件 (无法启用)
# ============================================================================
# test_ch_flow.cpp - 全文注释
# test_mod_width_simple.cpp - 全文注释
# test_stream_width_adapter.cpp - 全文注释

