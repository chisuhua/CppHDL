# FIFO Example CMakeLists.txt

add_executable(fifo_stream_example stream_fifo_example.cpp)

target_link_libraries(fifo_stream_example PRIVATE cpphdl)

set_target_properties(fifo_stream_example PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS fifo_stream_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)

# ── RTL 发射入口（`examples-rtl-sim-coverage` E3/E5 · 方案 3）────────────
# ⚠️ **每个 example 一个独立 target** ⇒ ⇒ 护栏 `scripts/verify_examples_rtl_sim.sh`
#   直接调用它产出 RTL + TB 骨架 ⇒ ⇒ 「产物从哪来」有了**唯一**答案。
add_executable(emit_rtl_fifo_stream emit_rtl.cpp)
target_link_libraries(emit_rtl_fifo_stream PRIVATE cpphdl)
target_include_directories(emit_rtl_fifo_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(emit_rtl_fifo_stream PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
