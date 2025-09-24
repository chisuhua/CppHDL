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

add_executable(dual_port_ram dual_port_ram.cpp)
target_link_libraries(dual_port_ram PRIVATE minicash)

add_executable(vec_and_struct vec_and_struct.cpp)
target_link_libraries(vec_and_struct ${CMAKE_THREAD_LIBS_INIT})

add_executable(sync_fifo sync_fifo.cpp)
target_link_libraries(sync_fifo ${CMAKE_THREAD_LIBS_INIT})

add_executable(async_fifo async_fifo.cpp)
target_link_libraries(async_fifo ${CMAKE_THREAD_LIBS_INIT})

add_executable(stream_example stream_example.cpp)
target_link_libraries(stream_example ${CMAKE_THREAD_LIBS_INIT})
