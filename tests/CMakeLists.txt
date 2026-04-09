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

include(chlib/CMakeLists.txt)
include(riscv/CMakeLists.txt)
enable_testing()

# Cache 完整功能测试
add_catch_test(test_cache_full chlib/test_cache_full.cpp)

# Cache-流水线集成测试
add_catch_test(test_cache_pipeline_integration riscv/test_cache_pipeline_integration.cpp)

# Hazard 完整功能测试
add_catch_test(test_hazard_complete riscv/test_hazard_complete.cpp)

# Phase 4 完整集成验证
add_catch_test(test_phase4_integration riscv/test_phase4_integration.cpp)
