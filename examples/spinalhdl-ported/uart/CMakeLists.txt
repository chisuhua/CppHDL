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
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# 安装规则
install(TARGETS uart_tx_example 
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)

# ── RTL 发射入口（`examples-rtl-sim-coverage` E3/E5 · 方案 3）────────────
# ⚠️ **每个 example 一个独立 target** ⇒ ⇒ 护栏 `scripts/verify_examples_rtl_sim.sh`
#   直接调用它产出 RTL + TB 骨架 ⇒ ⇒ 「产物从哪来」有了**唯一**答案。
add_executable(emit_rtl_uart_tx emit_rtl.cpp)
target_link_libraries(emit_rtl_uart_tx PRIVATE cpphdl)
target_include_directories(emit_rtl_uart_tx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(emit_rtl_uart_tx PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
