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
# CPU 模块测试
# ============================================================================

add_catch_test(test_branch_predictor cpu/test_branch_predictor_v2.cpp)
add_catch_test(test_forwarding cpu/test_forwarding.cpp)
add_catch_test(test_hazard cpu/test_hazard_complete.cpp)

# ============================================================================
# Cache 测试
# ============================================================================

add_catch_test(test_cache_full chlib/test_cache_full.cpp)
add_catch_test(test_cache_complete chlib/test_cache_complete.cpp)

# ============================================================================
# RISC-V 集成测试
# ============================================================================

add_catch_test(test_cache_integration riscv-integration/test_cache_pipeline_integration.cpp)
add_catch_test(test_phase4 riscv-integration/test_phase4_integration.cpp)
add_catch_test(test_pipeline riscv-integration/test_pipeline_integration.cpp)

# Add riscv-mini src directory for headers
target_include_directories(test_phase4 PRIVATE ${PROJECT_SOURCE_DIR}/examples/riscv-mini/src)
target_include_directories(test_cache_integration PRIVATE ${PROJECT_SOURCE_DIR}/examples/riscv-mini/src)
