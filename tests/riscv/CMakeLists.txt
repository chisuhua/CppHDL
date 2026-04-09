# tests/riscv/CMakeLists.txt
# RISC-V 测试配置

# 使用 ${CATCH_INCLUDE_DIR} 变量（由上层定义）
set(RISCV_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})

macro(add_test_riscv _test_name _sources)
    add_executable(${_test_name}
        ${RISCV_TEST_DIR}/${_sources}
        ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    )
    target_include_directories(${_test_name} PRIVATE
        ${CATCH_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${RISCV_TEST_DIR}
    )
    if(TARGET cpphdl)
        target_link_libraries(${_test_name} cpphdl)
    endif()
    add_test(NAME ${_test_name} COMMAND ${_test_name})
    set_tests_properties(${_test_name} PROPERTIES LABELS "riscv;base")
endmacro()

# 编译验证测试
add_test_riscv(test_hazard_compile test_hazard_compile.cpp)

# 功能测试
add_test_riscv(test_forwarding test_forwarding.cpp)
add_test_riscv(test_pipeline_integration test_pipeline_integration.cpp)
add_test_riscv(test_forwarding_hazard_integration test_forwarding_hazard_integration.cpp)

# 性能基准测试
add_test_riscv(benchmark_pipeline benchmark_pipeline.cpp)
