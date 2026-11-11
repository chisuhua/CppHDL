# AXI4 Examples CMakeLists.txt

# AXI4-Lite Example (时序验证)
add_executable(axi4_lite_example axi4_lite_example.cpp)
target_link_libraries(axi4_lite_example PRIVATE cpphdl)
target_include_directories(axi4_lite_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set_target_properties(axi4_lite_example PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# AXI4-Lite SoC Integration Example
add_executable(axi_soc_example axi_soc_example.cpp)
target_link_libraries(axi_soc_example PRIVATE cpphdl)
target_include_directories(axi_soc_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set_target_properties(axi_soc_example PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# AXI4 Full Protocol Example (T301)
add_executable(axi4_full_example axi4_full_example.cpp)
target_link_libraries(axi4_full_example PRIVATE cpphdl)
target_include_directories(axi4_full_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set_target_properties(axi4_full_example PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# Phase 3A Complete Integration (T304)
add_executable(phase3a_complete phase3a_complete.cpp)
target_link_libraries(phase3a_complete PRIVATE cpphdl)
target_include_directories(phase3a_complete PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set_target_properties(phase3a_complete PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

install(TARGETS axi4_lite_example axi_soc_example axi4_full_example phase3a_complete RUNTIME DESTINATION bin/axi4)

# ── RTL 发射入口（`examples-rtl-sim-coverage` E3/E5 · 方案 3）────────────
# ⚠️ **每个 example 一个独立 target** ⇒ ⇒ 护栏 `scripts/verify_examples_rtl_sim.sh`
#   直接调用它产出 RTL + TB 骨架 ⇒ ⇒ 「产物从哪来」有了**唯一**答案。
add_executable(emit_rtl_axi4_lite emit_rtl.cpp)
target_link_libraries(emit_rtl_axi4_lite PRIVATE cpphdl)
target_include_directories(emit_rtl_axi4_lite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(emit_rtl_axi4_lite PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
