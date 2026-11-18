# AXI4 Examples CMakeLists.txt

# AXI4-Lite Example (由 `.verif` 验证源驱动 · `axi4-verif-driven-cpp-2026-11`)
add_executable(tb_axi4_lite_example tb_axi4_lite_example.cpp)
target_link_libraries(tb_axi4_lite_example PRIVATE cpphdl)
target_include_directories(tb_axi4_lite_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set_target_properties(tb_axi4_lite_example PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
# ⚠️ **验证源的编译期绝对路径**：仓内三个面的 CWD 各不相同（`build/` · `build/tests` ·
#   仓根）⇒ ⇒ 相对路径必失败 ⇒ ⇒ 与 `tests/CMakeLists.txt` 同款注入。
target_compile_definitions(tb_axi4_lite_example PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# AXI4-Lite SoC Integration Example
add_executable(tb_axi_soc_example tb_axi_soc_example.cpp)
target_link_libraries(tb_axi_soc_example PRIVATE cpphdl)
target_include_directories(tb_axi_soc_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set_target_properties(tb_axi_soc_example PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(tb_axi_soc_example PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# ⚠️ `axi4_full_example` 已退役（2026-11-18 · §4.3.3）：行为覆盖 = `tests/test_axi4_full_burst.cpp`（dual-backend）

# Phase 3A Complete Integration (T304)
add_executable(phase3a_complete phase3a_complete.cpp)
target_link_libraries(phase3a_complete PRIVATE cpphdl)
target_include_directories(phase3a_complete PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set_target_properties(phase3a_complete PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

install(TARGETS tb_axi4_lite_example tb_axi_soc_example phase3a_complete RUNTIME DESTINATION bin/axi4)

# ── RTL 发射入口（`emit-cli-into-example` · 2026-11-18）────────────
# ⚠️ **发射入口已并入 `tb_axi4_lite_example`**（`--emit-rtl` 模式）⇒ ⇒
#   原独立 target（Lite 的发射薄壳）已删除（它只是「注册设计 + 调库」的 32 行薄壳，
#   与 tb 重复同一个 factory）⇒ ⇒ 产物**逐字节相同**（基线留档见 change `emit-cli-into-example`）。

# ⚠️ `emit_rtl_axi_soc` 已删除（2026-11-18 · B6）：发射并入 `tb_axi_soc_example --emit-rtl`，
#   产品自检在其 `--check-products` 模式（`soc_product_check.h`）。

# ── C1: `examples/axi4/tests/*.cpp` 的 Catch2 helper（`example-file-boundary` 批次 C1）──
# ⚠️ **CTest 条目名 = 函数第一参数**（与原 `tests/` 的注册名**逐字相同** ⇒ `ctest -N` 不变）
# ⚠️ include 集含 `${CMAKE_CURRENT_SOURCE_DIR}` ⇒ 搬入的测试用**裸文件名** include 设计定义头
function(axi4_catch_test TEST_NAME)
    set(_srcs ${ARGN})
    list(APPEND _srcs ${PROJECT_SOURCE_DIR}/tests/catch_amalgamated.cpp)
    add_executable(${TEST_NAME} ${_srcs})
    target_include_directories(${TEST_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/tests
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(TARGET cpphdl)
        target_link_libraries(${TEST_NAME} cpphdl)
    endif()
    target_compile_definitions(${TEST_NAME} PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    set_target_properties(${TEST_NAME} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "base" TIMEOUT 300)
endfunction()

# 搬入的 example 专属测试（B3）
axi4_catch_test(test_axi_lite_test_master tests/test_axi_lite_test_master.cpp)
axi4_catch_test(test_verif_axi4_lite tests/test_verif_axi4_lite.cpp)
