# Stream Mux —— 设计定义 `stream_mux_top.h`（无 `main`）+ 激励驱动 `tb_stream_mux_demo.cpp`
# ⚠️ 默认模式由 `.verif` 驱动（`tb_stream_mux_demo.verif` = 唯一语义来源）；
#    `--emit-rtl <dir>` = RTL 发射模式（产物端口名 = `top_<C++ 成员名>`）。
add_executable(tb_stream_mux_demo tb_stream_mux_demo.cpp)
target_link_libraries(tb_stream_mux_demo PRIVATE cpphdl)
target_include_directories(tb_stream_mux_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(tb_stream_mux_demo PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
# ⚠️ 验证源的编译期绝对路径：仓内三个面（run_all_ported_tests → build/ · CTest → build/tests ·
#    护栏 → 仓根）的 CWD 各不相同 ⇒ 相对路径必失败 ⇒ 与 axi4 同款注入。
target_compile_definitions(tb_stream_mux_demo PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
install(TARGETS tb_stream_mux_demo RUNTIME DESTINATION bin/examples/stream)

add_executable(stream_demux_demo stream_demux_demo.cpp)
target_link_libraries(stream_demux_demo PRIVATE cpphdl)
set_target_properties(stream_demux_demo PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
install(TARGETS stream_demux_demo RUNTIME DESTINATION bin/examples/stream)

add_executable(stream_arbiter_demo stream_arbiter_demo.cpp)
target_link_libraries(stream_arbiter_demo PRIVATE cpphdl)
set_target_properties(stream_arbiter_demo PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
install(TARGETS stream_arbiter_demo RUNTIME DESTINATION bin/examples/stream)

add_executable(stream_fork_demo stream_fork_demo.cpp)
target_link_libraries(stream_fork_demo PRIVATE cpphdl)
set_target_properties(stream_fork_demo PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
install(TARGETS stream_fork_demo RUNTIME DESTINATION bin/examples/stream)
