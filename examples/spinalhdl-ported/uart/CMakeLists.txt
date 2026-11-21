# UART TX Example CMakeLists.txt
# 设计定义 `uart_top.h`（无 `main`）+ 激励驱动 `tb_uart_tx_example.cpp`
# ⚠️ 默认模式由 `.verif` 驱动（`= 唯一语义来源`）；
#    `--emit-rtl <dir>` = RTL 发射模式（产物端口名 = `top_<C++ 成员名>`）。

add_executable(tb_uart_tx_example tb_uart_tx_example.cpp)
target_link_libraries(tb_uart_tx_example PRIVATE cpphdl)
target_include_directories(tb_uart_tx_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(tb_uart_tx_example PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
# ⚠️ 验证源的编译期绝对路径：仓内三个面（run_all_ported_tests → build/ ·
#    CTest → build/tests · 护栏 → 仓根）的 CWD 各不相同 ⇒ 相对路径必失败。
target_compile_definitions(tb_uart_tx_example PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

install(TARGETS tb_uart_tx_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
