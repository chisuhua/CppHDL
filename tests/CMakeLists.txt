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
add_test(NAME test_bitvector COMMAND test_bitvector)

add_executable(test_reg 
    catch_amalgamated.cpp
    test_reg.cpp
)

# 包含头文件目录
target_include_directories(test_reg PRIVATE 
    ${CATCH_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)


target_link_libraries(test_reg cpphdl)

# 启用 CTest 支持
add_test(NAME test_reg COMMAND test_reg)

#add_executable(test_multithread catch_amalgamated.cpp
#    test_multithread.cpp
#)
#target_include_directories(test_multithread PRIVATE 
#    ${CATCH_INCLUDE_DIR}
#    ${PROJECT_SOURCE_DIR}/include
#)
#target_link_libraries(test_multithread cpphdl)
#add_test(NAME test_multithread COMMAND test_multithread)

add_executable(test_operation catch_amalgamated.cpp
    test_reg.cpp
)
target_include_directories(test_operation PRIVATE 
    ${CATCH_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(test_operation cpphdl)
add_test(NAME test_operation COMMAND test_operation)

