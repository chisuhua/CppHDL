# Stream FIFO Example CMakeLists.txt
# 设计定义 `fifo_top.h`（无 `main`）+ 激励驱动 `tb_fifo_stream_example.cpp`
# ⚠️ 默认模式由 `.verif` 驱动（`= 唯一语义来源`）；
#    `--emit-rtl <dir>` = RTL 发射模式（产物端口名 = `top_<C++ 成员名>`）。

add_executable(tb_fifo_stream_example tb_fifo_stream_example.cpp)
target_link_libraries(tb_fifo_stream_example PRIVATE cpphdl)
target_include_directories(tb_fifo_stream_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(tb_fifo_stream_example PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
target_compile_definitions(tb_fifo_stream_example PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

install(TARGETS tb_fifo_stream_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
