# samples/CMakeLists.txt
# 注册的 sample target 列表 = 回归护栏 scripts/run_all_samples.sh 的覆盖集合。
#
# ⚠️ **每个 samples/*.cpp MUST 要么在此注册，要么登记进 samples/disabled_samples.txt**
#    （带理由 + 归位条件）⇒ 否则护栏的 C1「注册一致性」会失败。
#    未注册的 sample 见 samples/disabled_samples.txt（5 个，2026-10-22 实测）。
set(CH_SAMPLE_TARGETS
    counter
    test_dag
    dag_compare
    simple_io
    handshake
    fifo_example
    fifo_bundle
    bundle_demo
    bundle_advanced_features
    bundle_serialization_demo
    bundle_top_example
    axi_lite_demo
    nested_bundle_demo
    nested_fragment_demo
    tlm_bundle_demo
    pod_bundle_conversion
    minimal_fifo_bundle
    feedback_counter
    pipeline
    shift_register
    destruction
    precise_width_demo
    precise_width_example
    onehot_example
    configured_trace_example
    switch_demo
    stream_arbiter_example
    stream_demux_example
    stream_flow_basic_example
    stream_fork_example
    stream_join_example
    stream_fifo_example
    advanced_pod_bundle_serialization
    large_pod_bundle_example
    spinalhdl_stream_example
    conditional_demo
    if_demo
    stream_component_example
    stream_handshake_example
)

# 为每个示例创建可执行文件
foreach(target ${CH_SAMPLE_TARGETS})
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} cpphdl)
    
    # On macOS, cpphdl transitively links against LLVM-18 (which uses the
    # libc++ ABI tagged [abi:ue170006]) but does NOT propagate the libc++
    # link itself. Samples that use std::length_error / std::runtime_error
    # then fail at link with:
    #   "std::length_error::length_error(char const*)", referenced from ...
    # Append -lc++ only on Apple platforms to fix the missing libc++ link
    # without disturbing Linux (where the system libstdc++ is found
    # automatically by the linker).
    if(APPLE)
        target_link_libraries(${target} "-lc++")
    endif()
    
    # 设置C++23标准
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
    # 添加自定义目标以便于构建单个示例
    add_custom_target(${target}-run
        COMMAND ${target}
        DEPENDS ${target}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

# 创建一个目标来构建所有示例
add_custom_target(all-samples)
add_dependencies(all-samples ${CH_SAMPLE_TARGETS})