# examples/riscv-full-soc/CMakeLists.txt
#
# full 产品线的 SoC 级产品单元（ADR-078 · change `rv32i-full-product-line-2026-11` · S4c-0）
#
# ⚠️ **S4c-0 是「空壳」**（零功能）⇒ ⇒ 本目录只有两个 target：
#    · `tb_riscv_full_soc`     —— 产品级可执行（有 main · **进运行护栏的跟踪集合**）
#    · `test_rv32i_full_soc`   —— Catch2 判据（**进 CTest** · `LABELS "base"`）
#    ⚠️ **不发 RTL** ⇒ ⇒ `tb_riscv_full_soc` 在
#    `.rddf/checks/examples-rtl-sim-exclusions.tsv` 的**落选登记**里（理由 = 无 RTL 产物）
#    ⇒ ⇒ RTL 发射（`--emit-rtl` / `--check-products`）从 **S4c-1** 起。
#
# ⚠️ **为什么空壳也要有 target**：新线 MUST **立即进入**全部清单与护栏的扫描面
#    ⇒ ⇒ 「漏登记」当场红（而不是静默不覆盖）⇒ ⇒ 空壳就是那把探针。

# ── 产品级可执行（有 main）──
add_executable(tb_riscv_full_soc tb_riscv_full_soc.cpp)
target_link_libraries(tb_riscv_full_soc PRIVATE cpphdl)
target_include_directories(tb_riscv_full_soc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(tb_riscv_full_soc PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# ── Catch2 判据（进 CTest）──
# ⚠️ **target 名与测试源 MUST 同行**（护栏 `verify_example_test_wiring.sh` 的 MAP 解析
#    逐行取「`fn(` 的首个实参 = target」+「**最后一个** `.cpp` 实参 = 源」）⇒ ⇒
#    若把 `catch_amalgamated.cpp` 写在同一行 ⇒ ⇒ 关联会落到它身上 ⇒ ⇒
#    测试源变成「未被任何已覆盖的 target 编译」⇒ ⇒ 护栏报红。
#    ⇒ ⇒ 故拆成两行：**测试源独占第一行**（唯一的 `.cpp`）· amalgamated 走 `target_sources`。
add_executable(test_rv32i_full_soc tests/test_rv32i_full_soc.cpp)
target_sources(test_rv32i_full_soc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp)
target_link_libraries(test_rv32i_full_soc PRIVATE cpphdl)
target_include_directories(test_rv32i_full_soc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
)
set_target_properties(test_rv32i_full_soc PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
add_test(NAME test_rv32i_full_soc COMMAND test_rv32i_full_soc)
set_tests_properties(test_rv32i_full_soc PROPERTIES LABELS "base" TIMEOUT 300)
