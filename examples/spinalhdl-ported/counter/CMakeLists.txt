# Counter Examples CMakeLists.txt
# 设计定义 `counter_simple_top.h` / `counter_top.h`（无 `main`）
# + 激励驱动 `tb_counter_simple_example.cpp` / `tb_counter_example.cpp`
# ⚠️ 默认模式由 `.verif` 驱动（`= 唯一语义来源`）；
#    `--emit-rtl <dir>` = RTL 发射模式（产物端口名 = `top_<C++ 成员名>`）。

add_executable(tb_counter_simple_example tb_counter_simple_example.cpp)
target_link_libraries(tb_counter_simple_example PRIVATE cpphdl)
target_include_directories(tb_counter_simple_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(tb_counter_simple_example PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

add_executable(tb_counter_example tb_counter_example.cpp)
target_link_libraries(tb_counter_example PRIVATE cpphdl)
target_include_directories(tb_counter_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(tb_counter_example PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# ⚠️ 验证源的编译期绝对路径：仓内三个面（run_all_ported_tests → build/ ·
#    CTest → build/tests · 护栏 → 仓根）的 CWD 各不相同 ⇒ 相对路径必失败。
target_compile_definitions(tb_counter_simple_example PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(tb_counter_example PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

install(TARGETS tb_counter_simple_example tb_counter_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
