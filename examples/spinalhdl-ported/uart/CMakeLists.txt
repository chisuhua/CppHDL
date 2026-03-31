# UART TX Example CMakeLists.txt

# 创建可执行文件
add_executable(uart_tx_example uart_tx.cpp)

# 链接 CppHDL 库
target_link_libraries(uart_tx_example 
    PRIVATE 
        cpphdl
)

# 设置 C++ 标准
set_target_properties(uart_tx_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# 安装规则
install(TARGETS uart_tx_example 
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
