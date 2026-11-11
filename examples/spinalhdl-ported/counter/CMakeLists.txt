# Counter Example CMakeLists.txt

# 创建可执行文件 - 完整版
add_executable(counter_example counter.cpp)

# 链接 CppHDL 库
target_link_libraries(counter_example 
    PRIVATE 
        cpphdl
)

# 设置 C++ 标准
set_target_properties(counter_example PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# 创建可执行文件 - 简化版
add_executable(counter_simple_example counter_simple.cpp)

# 链接 CppHDL 库
target_link_libraries(counter_simple_example 
    PRIVATE 
        cpphdl
)

# 设置 C++ 标准
set_target_properties(counter_simple_example PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# ── RTL 发射入口（`examples-rtl-sim-coverage` E3/E5 · 方案 3）────────────
# ⚠️ **每个 example 一个独立 target** ⇒ ⇒ 护栏 `scripts/verify_examples_rtl_sim.sh`
#   直接调用它产出 RTL + TB 骨架 ⇒ ⇒ 「产物从哪来」有了**唯一**答案。
# ⚠️ 本 target **在** examples 里 ⇒ ⇒ 不违反 C1 的「工具不链 examples」约束
#   （`tools/cpphdl-emit-rtl` 仍然只链 `cpphdl`）。
add_executable(emit_rtl_counter emit_rtl_counter.cpp)
target_link_libraries(emit_rtl_counter PRIVATE cpphdl)
target_include_directories(emit_rtl_counter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(emit_rtl_counter PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

add_executable(emit_rtl_counter_simple emit_rtl_counter_simple.cpp)
target_link_libraries(emit_rtl_counter_simple PRIVATE cpphdl)
target_include_directories(emit_rtl_counter_simple PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(emit_rtl_counter_simple PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# 安装规则
install(TARGETS counter_example counter_simple_example
    RUNTIME DESTINATION bin/examples
)
