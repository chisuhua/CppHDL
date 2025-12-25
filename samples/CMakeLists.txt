# samples/CMakeLists.txt
set(CH_SAMPLE_TARGETS
    counter
    test_dag
    simple_io
    handshake
    fifo
    fifo_bundle
    bundle_demo
    bundle_advanced_features
    bundle_serialization_demo
    bundle_top_example
    axi_lite_demo
    nested_bundle_demo
    tlm_bundle_demo
    bundle_simulator_demo
    pod_bundle_conversion
    minimal_fifo_bundle
    feedback_counter
    pipeline
    shift_register
    destruction
    precise_width_demo
    precise_width_example
    onehot_module_example
)

# 为每个示例创建可执行文件
foreach(target ${CH_SAMPLE_TARGETS})
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} cpphdl)
    
    # 设置C++20标准
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
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
