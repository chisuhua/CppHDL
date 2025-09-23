# examples/CMakeLists.txt

# 创建 adder_module 可执行文件
add_executable(adder_module adder_module.cpp)
target_link_libraries(adder_module PRIVATE minicash)

# 创建 counter_module 可执行文件
add_executable(counter_module counter_module.cpp)
target_link_libraries(counter_module PRIVATE minicash)

# 设置编译选项
target_compile_options(adder_module PRIVATE -Wall -Wextra)
target_compile_options(counter_module PRIVATE -Wall -Wextra)

add_executable(dual_clock_module dual_clock_module.cpp)
target_link_libraries(dual_clock_module PRIVATE minicash)
