# examples/riscv-soc/CMakeLists.txt
#
# SoC 级产品单元（ADR-077 D1 · change `riscv-soc-skeleton-2026-11`）。
#
# ⚠️ **只一个 target**：`tb_riscv_soc`（激励驱动 + 发射入口）。
#    设计定义在 `src/rv32i_soc_top.h`（无 main）。
# ⚠️ **不** `add_test` —— 与 `tb_rv32i_soc` 同型（产品级 TB 由
#    `scripts/verify_examples_rtl_sim.sh` 跑，不进 CTest）⇒ `CTEST_TOTAL` 不变。

add_executable(tb_riscv_soc tb_riscv_soc.cpp)

target_link_libraries(tb_riscv_soc PRIVATE cpphdl)

# ⚠️ 核复合体住 `examples/riscv-mini/src/`（ADR-077 D1 的两级顶层分工）
#    ⇒ 本目录以**局部** include 注入该路径（不是全局 `-I` ⇒ 不影响根 tests 的 C2 判据）。
target_include_directories(tb_riscv_soc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../riscv-mini/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${PROJECT_SOURCE_DIR}/include
)

# 验证源 / 仓根定位（`verify_examples_rtl_sim.sh` 与 TB 的 `EXPECT-SRC` 需要）
target_compile_definitions(tb_riscv_soc PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
