# tests/riscv/CMakeLists.txt
# RISC-V 测试配置

# 使用主 tests/CMakeLists.txt 中的 add_catch_test 函数
# 注意：CMakeLists.txt 被 tests/CMakeLists.txt include 后，可以使用该函数

# Hazard Unit 编译测试（纯编译验证）
add_executable(test_hazard_compile test_hazard_compile.cpp)
target_include_directories(test_hazard_compile PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)
if(TARGET cpphdl)
    target_link_libraries(test_hazard_compile cpphdl)
endif()
add_test(NAME test_hazard_compile COMMAND test_hazard_compile)
set_tests_properties(test_hazard_compile PROPERTIES LABELS "riscv;base")

# Forwarding 单元功能测试
add_executable(test_forwarding 
    test_forwarding.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
)
target_include_directories(test_forwarding PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)
if(TARGET cpphdl)
    target_link_libraries(test_forwarding cpphdl)
endif()
add_test(NAME test_forwarding COMMAND test_forwarding)
set_tests_properties(test_forwarding PROPERTIES LABELS "riscv;base")

# 5 级流水线性能基准测试
add_executable(benchmark_pipeline 
    benchmark_pipeline.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
)
target_include_directories(benchmark_pipeline PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)
if(TARGET cpphdl)
    target_link_libraries(benchmark_pipeline cpphdl)
endif()
add_test(NAME benchmark_pipeline COMMAND benchmark_pipeline)
set_tests_properties(benchmark_pipeline PROPERTIES LABELS "riscv;base;benchmark")

# Pipeline 集成测试（功能测试）
add_executable(test_pipeline_integration 
    test_pipeline_integration.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
)
target_include_directories(test_pipeline_integration PRIVATE
    ${CATCH_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)
if(TARGET cpphdl)
    target_link_libraries(test_pipeline_integration cpphdl)
endif()
add_test(NAME test_pipeline_integration COMMAND test_pipeline_integration)
set_tests_properties(test_pipeline_integration PROPERTIES LABELS "riscv;base")
