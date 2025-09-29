# Catch2 单头文件路径
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 创建测试可执行文件
add_executable(test_bitvector 
    catch_amalgamated.cpp
    test_bv_cmp.cpp
    test_bitvector.cpp
    test_bv_truncate.cpp
)

# 包含头文件目录
target_include_directories(test_bitvector PRIVATE 
    ${CATCH_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)

# 启用 CTest 支持
add_test(NAME test_bitvector COMMAND test_bitvector)
