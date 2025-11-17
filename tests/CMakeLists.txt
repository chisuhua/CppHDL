# tests/CMakeLists.txt

# 启用测试支持
enable_testing()

# Catch2 单头文件路径
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 定义创建单个 Catch2 测试的函数
function(add_catch_test TEST_NAME)
    # 参数：测试名称，其余为该测试的源文件（不含 catch_amalgamated.cpp）
    set(SOURCES ${ARGN})

    # 自动包含 catch_amalgamated.cpp
    list(APPEND SOURCES ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp)

    # 创建可执行文件
    add_executable(${TEST_NAME} ${SOURCES})

    # 包含目录
    target_include_directories(${TEST_NAME} PRIVATE
        ${CATCH_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
    )

    # 链接库（如果存在）
    if(TARGET cpphdl)
        target_link_libraries(${TEST_NAME} cpphdl)
    endif()

    # 注册为 CTest 测试
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# ================================
# 方案一：为每个 test_xxx.cpp 创建独立测试（推荐）
# ================================

# 列出所有独立的测试源文件（每个文件一个测试）
set(SINGLE_TEST_SOURCES
    test_reg.cpp
    test_operator.cpp
    test_literal.cpp
    test_simple_io.cpp
    test_bundle.cpp
    test_bundle_advanced.cpp
    test_bundle_advanced_features.cpp
    test_bundle_serialization.cpp
    test_bundle_bitstream.cpp
    # test_multithread.cpp
    test_nested_bundle.cpp
    test_tlm_bundle_phase1.cpp
    test_axi_lite_bundle.cpp
    test_mem.cpp
    test_operator_advanced.cpp
    test_verilog_gen_new.cpp
)

# 为每个源文件创建一个独立的测试
foreach(TEST_SRC ${SINGLE_TEST_SOURCES})
    # 提取测试名，如 test_reg.cpp -> test_reg
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_catch_test(${TEST_NAME} ${TEST_SRC})
endforeach()

# ================================
# 方案二：特殊测试（多文件组合）
# ================================

# test_bitvector 需要多个源文件
add_catch_test(test_bitvector
    test_bv_cmp.cpp
    test_bitvector.cpp
    test_bv_truncate.cpp
)