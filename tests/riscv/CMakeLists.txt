# tests/riscv/CMakeLists.txt
# RISC-V 相关测试的子 CMake 配置

# 由于当前代码还在开发中，这些测试作为编译检查使用
# 不添加到主测试列表中，除非手动启用

# IF 阶段编译测试
# add_executable(test_if_stage_compile test_if_stage.cpp)
# target_include_directories(test_if_stage_compile PRIVATE
#     ${CMAKE_SOURCE_DIR}/tests
#     ${CMAKE_SOURCE_DIR}/include
# )
# if(TARGET cpphdl)
#     target_link_libraries(test_if_stage_compile cpphdl)
# endif()

# ID 阶段编译测试
# add_executable(test_id_stage_compile test_id_stage.cpp)
# target_include_directories(test_id_stage_compile PRIVATE
#     ${CMAKE_SOURCE_DIR}/tests
#     ${CMAKE_SOURCE_DIR}/include
# )
# if(TARGET cpphdl)
#     target_link_libraries(test_id_stage_compile cpphdl)
# endif()
