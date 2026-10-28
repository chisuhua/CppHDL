# Design: dual-backend-full-coverage-inventory-2026-10

> 本 change **只读**：交付物 = 下面的**盘点表**（以 **ctest 条目** 为行）。
> ⚠️ **无代码改动** ⇒ 无 capability 变更 ⇒ 归档用 `--skip-specs`；
> 但 §9 的**判据问题**属 **planner** 职责（`objective-test-coverage-100pct` 的 Go 判据）。

## 0. 一句话结论

⇒ **判据「`ctest -L verilator_e2e` ≥ 100」在现有语料内不可达**：
**口径 A（现状判据 = 标签条目数）诚实上限 ∈ [71, 96]**、
**口径 B（真 dual pair 数，语义正确）诚实上限 ∈ [62, 87]**
⇒ **两种口径的保守上限都 < 100；连乐观上限（口径 A = 96）也 < 100（差 4）**
⇒ ⇒ 「盲做到底」**必然**到不了判据，**连把 demo 改写成 dual 形式去「凑数」也不够**
⇒ ⇒ ⇒ **MUST 由 planner 修订判据或度量单位**（取舍集见 §9.2.2，共 4 个轴）。

⚠️ **「上限」的边界**（否则结论会被误读）：
- 本盘点数的是**现有** ctest 条目 ⇒ **不**覆盖「**新增**有意义的 dual 测试文件」
  这条路线（⇒ §9.2.2 的轴 ③；§9.4 证伪的是该路线的「**必要性**」，
  **不是**「可行性」）；
- **口径 A 的技术上限其实无界**（给任意既有条目加 `verilator_e2e` 标签即可 +1，
  理论上可到 202）⇒ 但那正是「**奖励加标签而不是加 parity**」⇒
  ⇒ 故 71 / 96 是「**语义诚实**的上限」⇒ ⇒ 「保留 ≥ 100」**没有诚实路径**
  （只有**不诚实**路径）。

## 1. 方法（可复现）

1. 条目清单 = `ctest --show-only=json-v1`（**202** 条，含标签）；
2. 注册映射 = 解析 `tests/CMakeLists.txt` 的 `add_catch_test` /
   `add_dual_backend_test` + `examples/riscv-mini/CMakeLists.txt` 的
   `riscv_mini_catch_test` / `riscv_mini_unit_test`
   ⇒ 共 **175** 个注册调用，其中 **172** 个进 CTest、**3** 个
   （`rv32i_phase{1,2,3}_test`）**只构建、不进 CTest** ⇒ **不是 ctest 条目**
   ⇒ 不在判据的分子里；
3. 判定 = `dual-backend-test-coverage` 的 **R6 排除清单（5 类）**
   + 该 capability 明文的候选 grep（`ch_device|ch_module` ∧ ¬`STATIC_REQUIRE`
   ∧ ¬`ch_out<ch_uint<[0-9]{3,}>>`）
   + **该 capability 的「现有 9 个 verilator 测试 MUST 保留原状」Requirement**；
4. ⚠️ **三个口径的坑（本次实测踩到并修正）**：
   - **`DUAL_BACKEND_TEST_PAIR` MUST 判「**调用**」而不是「提到」** ——
     `test_verilator_trace_parity.cpp` 等 **10** 个文件只在**注释**里提到该宏
     ⇒ 子串匹配会把它们**误判**为真 dual pair ⇒ 盘点必须**先剥注释**再判；
   - **>64-bit 端口 MUST 直接判 `N > 64`** ——
     capability 的 grep 用 `[0-9]\{3,\}`（≥100 位）⇒ **65–99 位漏网**；
   - **判据的分子是「带标签的 ctest 条目」** ⇒ 只构建、不进 CTest 的 target
     **不在分子里** ⇒ 盘点表 MUST 以 **ctest 条目**为行，不是「注册调用」。
5. 第四档（本次新增）：**零 runtime 语义** —— 含 `ch_device` 但**从不驱动
   `Simulator`** 的文件 ⇒ dual pair 的 body 里无断言可跑 ⇒ dual 化**不构成覆盖**。

⚠️ **本文件的盘点表与各分类计数由脚本生成**（`inventory.py` → `inventory.md`
→ 本文件）⇒ 人写的只有 §0（结论）/ §1（方法）/ §7 末尾的「推翻前次审查」表 /
§8 的批次数 / §9（交付 planner 的请求）⇒ ⇒ **杜绝手抄漂移**（本次手写版曾把
`test_bundle_ecosystem` 判成 R6 #4，实际是 R6 #1/#5）。

---

## 2. 条目总账（行数守恒）

| 分组 | 条目数 | 说明 |
|---|---|---|
| ctest 条目总计 | **202** | `ctest -N` |
| 盘点范围：Catch2 测试条目 | **172** | `add_catch_test` / `add_dual_backend_test` / `riscv_mini_*_test` |
| 范围外条目 | **30** | perf 基准（4）+ `spinalhdl-ported` 演示程序（25）+ alias 条目（1） |
| **只构建、不进 CTest** 的 target | **3** | `riscv_mini_unit_test`（自带 `main()`，无 Catch2）⇒ **不是 ctest 条目** ⇒ 不在判据的分子里 |

守恒：202 = 172（盘点范围）+ 30（范围外） ✓　　（另有 3 个 target 只构建、不进 CTest ⇒ 不计入 202）

只构建、不进 CTest 的 target 逐条：

- `rv32i_phase1_test` — `riscv_mini_unit_test`（`examples/riscv-mini/CMakeLists.txt:73`，源 `examples/riscv-mini/rv32i_phase1_test.cpp`）
- `rv32i_phase2_test` — `riscv_mini_unit_test`（`examples/riscv-mini/CMakeLists.txt:74`，源 `examples/riscv-mini/rv32i_phase2_test.cpp`）
- `rv32i_phase3_test` — `riscv_mini_unit_test`（`examples/riscv-mini/CMakeLists.txt:75`，源 `examples/riscv-mini/rv32i_phase3_test.cpp`）

范围外条目逐条（附排除理由）：

| 条目 | 标签 | 排除理由 |
|---|---|---|
| `perf_regression` | `perf` | perf 基准 harness（`perf` 标签）⇒ 不是单元测试；跑的是基准不是断言 |
| `perf_results_clean` | `perf` | perf 基准 harness（`perf` 标签）⇒ 不是单元测试；跑的是基准不是断言 |
| `perf_tests` | `perf` | perf 基准 harness（`perf` 标签）⇒ 不是单元测试；跑的是基准不是断言 |
| `perf_three_way` | `perf`, `verilator` | perf 基准 harness（`perf` 标签）⇒ 不是单元测试；跑的是基准不是断言 |
| `spinalhdl_assert` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_axi4_full` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_axi4_lite` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_axi4_phase3a` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_axi4_soc` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_branch_predictor_v2` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_counter` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_counter_simple` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_fifo` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_gpio` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_i2c` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_phase1c` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_pwm` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_quadrature_encoder` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_rv32i_phase1` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_rv32i_phase2` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_rv32i_phase3` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_sigma_delta_dac` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_spi` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_timer` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_uart_rx` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_uart_tx` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_vga` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_width_adapter` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `spinalhdl_ws2812` | `spinalhdl-ported` | `main()` 演示程序（**无 Catch2**）⇒ 结构上无法承载 `DUAL_BACKEND_TEST_PAIR` ⇒ 不是「单元测试双后端覆盖率」的分子 |
| `test_verilator_three_way` | `base` | **alias 条目**（`add_test(NAME test_verilator_three_way COMMAND test_codegen_literal_mask)`）⇒ 与 `test_codegen_literal_mask` **同源文件、两个条目** ⇒ 见 §5 的映射 |

## 3. 盘点表（172 行）

| # | 条目 | 源文件 | 注册处 | 判定 | 类别 | 理由 |
|---|---|---|---|---|---|---|
| 1 | `perf_build_us` | `tests/benchmark/test_build_us_present.cpp` | `tests/CMakeLists.txt:285` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 2 | `perf_jit_reg` | `tests/benchmark/test_jit_reg_perf.cpp` | `tests/CMakeLists.txt:279` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 3 | `perf_jit_smallgraph` | `tests/benchmark/test_jit_smallgraph.cpp` | `tests/CMakeLists.txt:282` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 4 | `perf_jit_vs_interp_ratio` | `tests/benchmark/test_jit_vs_interp_ratio.cpp` | `tests/CMakeLists.txt:336` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 5 | `perf_report_consistency` | `tests/benchmark/test_report_consistency.cpp` | `tests/CMakeLists.txt:299` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 6 | `perf_subprocess_isolation` | `tests/benchmark/test_perf_subprocess_isolation.cpp` | `tests/CMakeLists.txt:321` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 7 | `perf_verilator_integration` | `tests/benchmark/test_verilator_integration.cpp` | `tests/CMakeLists.txt:360` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 8 | `riscv_mini_cache_smoke` | `examples/riscv-mini/tests/test_cache_smoke.cpp` | `examples/riscv-mini/CMakeLists.txt:126` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 9 | `test_address_decoder` | `examples/riscv-mini/tests/test_riscv_tests_soc.cpp` | `examples/riscv-mini/CMakeLists.txt:81` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 10 | `test_arithmetic` | `tests/chlib/test_arithmetic.cpp` | `tests/CMakeLists.txt:158` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 11 | `test_arithmetic_advance` | `tests/chlib/test_arithmetic_advance.cpp` | `tests/CMakeLists.txt:159` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 12 | `test_assert` | `tests/chlib/test_assert.cpp` | `tests/CMakeLists.txt:160` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 13 | `test_axi4_full_burst` | `tests/test_axi4_full_burst.cpp` | `tests/CMakeLists.txt:486` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 14 | `test_axi_byte_lane` | `tests/test_axi_byte_lane.cpp` | `tests/CMakeLists.txt:467` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 15 | `test_axi_dma` | `tests/test_axi_dma.cpp` | `tests/CMakeLists.txt:463` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 16 | `test_axi_gpio` | `tests/test_axi_gpio.cpp` | `tests/CMakeLists.txt:507` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 17 | `test_axi_gpio_behavior` | `tests/test_axi_gpio_behavior.cpp` | `tests/CMakeLists.txt:258` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 18 | `test_axi_i2c` | `tests/test_axi_i2c.cpp` | `tests/CMakeLists.txt:508` | **可 dual（无 runtime 语义）** | 可 dual 但无 runtime 语义 | 含 ch_device/ch_module，但**零 `Simulator` 调用** ⇒ dual pair 的 body 里无断言可跑 ⇒ dual 化**不构成覆盖**（凑数） |
| 19 | `test_axi_i2c_behavior` | `tests/test_axi_i2c_behavior.cpp` | `tests/CMakeLists.txt:247` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 20 | `test_axi_interconnect_4x4` | `tests/test_axi_interconnect_4x4.cpp` | `tests/CMakeLists.txt:497` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 21 | `test_axi_lite_bundle` | `tests/test_axi_lite_bundle.cpp` | `tests/CMakeLists.txt:509` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 22 | `test_axi_lite_matrix_2x2` | `tests/test_axi_lite_matrix_2x2.cpp` | `tests/CMakeLists.txt:503` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 23 | `test_axi_lite_slave_conformance` | `tests/test_axi_lite_slave_conformance.cpp` | `tests/CMakeLists.txt:480` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 24 | `test_axi_lite_verilator_e2e` | `tests/test_axi_lite_verilator_e2e.cpp` | `tests/CMakeLists.txt:113` | **不可 dual（契约锁定）** | 契约锁定 | capability Requirement『现有 9 个 verilator 测试 MUST 保留原状』⇒ **MUST NOT** 迁移到 DUAL_BACKEND_TEST_PAIR（spec.md:145-147） |
| 25 | `test_axi_pwm` | `tests/test_axi_pwm.cpp` | `tests/CMakeLists.txt:510` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 26 | `test_axi_pwm_behavior` | `tests/test_axi_pwm_behavior.cpp` | `tests/CMakeLists.txt:243` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 27 | `test_axi_pwm_simple` | `tests/test_axi_pwm_simple.cpp` | `tests/CMakeLists.txt:511` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 28 | `test_axi_rst_n_guard` | `tests/test_axi_rst_n_guard.cpp` | `tests/CMakeLists.txt:491` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 29 | `test_axi_slave_byte_lane` | `tests/test_axi_slave_byte_lane.cpp` | `tests/CMakeLists.txt:474` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 30 | `test_axi_spi` | `tests/test_axi_spi.cpp` | `tests/CMakeLists.txt:512` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 31 | `test_axi_spi_behavior` | `tests/test_axi_spi_behavior.cpp` | `tests/CMakeLists.txt:236` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 32 | `test_axi_timer` | `tests/test_axi_timer.cpp` | `tests/CMakeLists.txt:513` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 33 | `test_axi_timer_behavior` | `tests/test_axi_timer_behavior.cpp` | `tests/CMakeLists.txt:254` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 34 | `test_axi_uart` | `tests/test_axi_uart.cpp` | `tests/CMakeLists.txt:514` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 35 | `test_axi_uart_behavior` | `tests/test_axi_uart_behavior.cpp` | `tests/CMakeLists.txt:262` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 36 | `test_basic` | `tests/test_basic.cpp` | `tests/CMakeLists.txt:384` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 37 | `test_bits_update` | `tests/test_bits_update.cpp` | `tests/CMakeLists.txt:517` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 38 | `test_bitvector` | `tests/test_bitvector.cpp` | `tests/CMakeLists.txt:518` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 39 | `test_bitwise` | `tests/chlib/test_bitwise.cpp` | `tests/CMakeLists.txt:161` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 40 | `test_blt_debug` | `tests/test_blt_debug.cpp` | `tests/CMakeLists.txt:544` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 41 | `test_bool_connection` | `tests/test_bool_connection.cpp` | `tests/CMakeLists.txt:519` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 42 | `test_branch_predictor_v2` | `tests/chlib/test_branch_predictor_v2.cpp` | `tests/CMakeLists.txt:268` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 43 | `test_bundle` | `tests/test_bundle.cpp` | `tests/CMakeLists.txt:413` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 44 | `test_bundle_advanced` | `tests/test_bundle_advanced.cpp` | `tests/CMakeLists.txt:415` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 45 | `test_bundle_advanced_features` | `tests/test_bundle_advanced_features.cpp` | `tests/CMakeLists.txt:416` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 46 | `test_bundle_bitstream` | `tests/test_bundle_bitstream.cpp` | `tests/CMakeLists.txt:417` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 47 | `test_bundle_connection` | `tests/test_bundle_connection.cpp` | `tests/CMakeLists.txt:418` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 48 | `test_bundle_ecosystem` | `tests/test_bundle_ecosystem.cpp` | `tests/CMakeLists.txt:197` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 49 | `test_bundle_large` | `tests/test_bundle_large.cpp` | `tests/CMakeLists.txt:414` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 50 | `test_bundle_literal` | `tests/test_bundle_literal.cpp` | `tests/CMakeLists.txt:77` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 51 | `test_bundle_master_slave` | `tests/test_bundle_master_slave.cpp` | `tests/CMakeLists.txt:196` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 52 | `test_bundle_node_management` | `tests/test_bundle_node_management.cpp` | `tests/CMakeLists.txt:419` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 53 | `test_bundle_serialization` | `tests/test_bundle_serialization.cpp` | `tests/CMakeLists.txt:420` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 54 | `test_bv_cmp` | `tests/test_bv_cmp.cpp` | `tests/CMakeLists.txt:545` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 55 | `test_bv_truncate` | `tests/test_bv_truncate.cpp` | `tests/CMakeLists.txt:546` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 56 | `test_cache_complete` | `tests/chlib/test_cache_complete.cpp` | `tests/CMakeLists.txt:163` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 57 | `test_cache_pipeline` | `tests/chlib/test_cache_pipeline_integration.cpp` | `tests/CMakeLists.txt:182` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 58 | `test_cache_simple` | `tests/chlib/test_cache_simple.cpp` | `tests/CMakeLists.txt:162` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 59 | `test_cache_sync` | `tests/chlib/test_cache_sync.cpp` | `tests/CMakeLists.txt:187` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 60 | `test_cam` | `tests/chlib/test_cam.cpp` | `tests/CMakeLists.txt:164` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 61 | `test_ch_flow` | `tests/test_ch_flow.cpp` | `tests/CMakeLists.txt:540` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 62 | `test_ch_reg_bitwise` | `tests/test_ch_reg_bitwise.cpp` | `tests/CMakeLists.txt:547` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 63 | `test_ch_stream` | `tests/test_ch_stream.cpp` | `tests/CMakeLists.txt:427` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 64 | `test_chipforge_mux_segv_repro` | `tests/test_chipforge_mux_segv_repro.cpp` | `tests/CMakeLists.txt:72` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 65 | `test_codegen_literal_mask` | `tests/test_codegen_literal_mask.cpp` | `tests/CMakeLists.txt:87` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 66 | `test_combinational` | `tests/chlib/test_combinational.cpp` | `tests/CMakeLists.txt:165` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 67 | `test_comparison_ops` | `tests/test_comparison_ops.cpp` | `tests/CMakeLists.txt:520` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 68 | `test_converter` | `tests/chlib/test_converter.cpp` | `tests/CMakeLists.txt:166` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 69 | `test_csr_unit` | `examples/riscv-mini/tests/test_csr_unit.cpp` | `tests/CMakeLists.txt:63` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 70 | `test_dual_backend_64_bit` | `tests/test_dual_backend_64_bit.cpp` | `tests/CMakeLists.txt:136` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 71 | `test_dual_backend_clock` | `tests/test_dual_backend_clock.cpp` | `tests/CMakeLists.txt:144` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 72 | `test_dual_backend_reset` | `tests/test_dual_backend_reset.cpp` | `tests/CMakeLists.txt:152` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 73 | `test_dual_backend_template` | `tests/test_dual_backend_template.cpp` | `tests/CMakeLists.txt:126` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 74 | `test_fifo` | `tests/chlib/test_fifo.cpp` | `tests/CMakeLists.txt:167` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 75 | `test_fifo_example` | `tests/test_fifo_example.cpp` | `tests/CMakeLists.txt:537` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 76 | `test_forwarding` | `tests/chlib/test_forwarding.cpp` | `tests/CMakeLists.txt:52` | **不可 dual** | R6 #4 | 含 SECTION ⇒ 双 run 结构下重入语义不一致 |
| 77 | `test_fragment` | `tests/test_fragment.cpp` | `tests/CMakeLists.txt:460` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 78 | `test_hazard` | `tests/chlib/test_hazard_complete.cpp` | `tests/CMakeLists.txt:53` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 79 | `test_if_stmt` | `tests/chlib/test_if_stmt.cpp` | `tests/CMakeLists.txt:168` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 80 | `test_ifstage_device` | `examples/riscv-mini/tests/test_ifstage_device.cpp` | `examples/riscv-mini/CMakeLists.txt:96` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 81 | `test_interpreter_jit_compat` | `tests/test_interpreter_jit_compat.cpp` | `tests/CMakeLists.txt:710` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 82 | `test_interrupt_flow` | `examples/riscv-mini/tests/test_interrupt_flow.cpp` | `examples/riscv-mini/CMakeLists.txt:98` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 83 | `test_io_extended_ops` | `tests/test_io_extended_ops.cpp` | `tests/CMakeLists.txt:409` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 84 | `test_io_operations` | `tests/test_io_operations.cpp` | `tests/CMakeLists.txt:408` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 85 | `test_jit_compiler` | `tests/test_jit_compiler.cpp` | `tests/CMakeLists.txt:701` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 86 | `test_jit_concat` | `tests/test_jit_concat.cpp` | `tests/CMakeLists.txt:702` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 87 | `test_jit_golden` | `tests/jit/test_jit_golden.cpp` | `tests/CMakeLists.txt:707` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 88 | `test_jit_init_succeeds` | `tests/jit/test_jit_init_succeeds.cpp` | `tests/CMakeLists.txt:718` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 89 | `test_jit_llvm_version` | `tests/jit/test_jit_llvm_version.cpp` | `tests/CMakeLists.txt:717` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 90 | `test_literal` | `tests/test_literal.cpp` | `tests/CMakeLists.txt:385` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 91 | `test_literal_debug` | `tests/test_literal_debug.cpp` | `tests/CMakeLists.txt:386` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 92 | `test_literal_left_shift` | `tests/test_literal_left_shift.cpp` | `tests/CMakeLists.txt:387` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 93 | `test_literal_width` | `tests/test_literal_width.cpp` | `tests/CMakeLists.txt:388` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 94 | `test_logic` | `tests/chlib/test_logic.cpp` | `tests/CMakeLists.txt:169` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 95 | `test_mem` | `tests/test_mem.cpp` | `tests/CMakeLists.txt:443` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 96 | `test_mem_comb_read` | `tests/test_mem_comb_read.cpp` | `tests/CMakeLists.txt:446` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 97 | `test_mem_connection` | `tests/test_mem_connection.cpp` | `tests/CMakeLists.txt:444` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 98 | `test_mem_read_timing_dual` | `tests/test_mem_read_timing_dual.cpp` | `tests/CMakeLists.txt:205` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 99 | `test_mem_timing` | `tests/test_mem_timing.cpp` | `tests/CMakeLists.txt:445` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 100 | `test_memory` | `tests/chlib/test_memory.cpp` | `tests/CMakeLists.txt:170` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 101 | `test_mod_width` | `tests/test_mod_width.cpp` | `tests/CMakeLists.txt:530` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 102 | `test_module` | `tests/test_module.cpp` | `tests/CMakeLists.txt:548` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 103 | `test_module_advanced` | `tests/test_module_advanced.cpp` | `tests/CMakeLists.txt:557` | **不可 dual** | R6 #4 | 含 SECTION ⇒ 双 run 结构下重入语义不一致 |
| 104 | `test_multithread` | `tests/test_multithread.cpp` | `tests/CMakeLists.txt:558` | **可 dual（无 runtime 语义）** | 可 dual 但无 runtime 语义 | 含 ch_device/ch_module，但**零 `Simulator` 调用** ⇒ dual pair 的 body 里无断言可跑 ⇒ dual 化**不构成覆盖**（凑数） |
| 105 | `test_nested_bundle` | `tests/test_nested_bundle.cpp` | `tests/CMakeLists.txt:421` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 106 | `test_onehot` | `tests/chlib/test_onehot.cpp` | `tests/CMakeLists.txt:171` | **不可 dual** | R6 #4 | 含 SECTION ⇒ 双 run 结构下重入语义不一致 |
| 107 | `test_operation_results` | `tests/test_operation_results.cpp` | `tests/CMakeLists.txt:559` | **不可 dual** | R6 #4 | 含 SECTION ⇒ 双 run 结构下重入语义不一致 |
| 108 | `test_operation_widths` | `tests/test_operation_widths.cpp` | `tests/CMakeLists.txt:560` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 109 | `test_operator` | `tests/test_operator.cpp` | `tests/CMakeLists.txt:401` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 110 | `test_operator_advanced` | `tests/test_operator_advanced.cpp` | `tests/CMakeLists.txt:402` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 111 | `test_operator_connection` | `tests/test_operator_connection.cpp` | `tests/CMakeLists.txt:403` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 112 | `test_operator_results` | `tests/test_operator_results.cpp` | `tests/CMakeLists.txt:404` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 113 | `test_phase4_chlib` | `tests/chlib/test_phase4_integration.cpp` | `tests/CMakeLists.txt:377` | **可 dual（无 runtime 语义）** | 可 dual 但无 runtime 语义 | 含 ch_device/ch_module，但**零 `Simulator` 调用** ⇒ dual pair 的 body 里无断言可跑 ⇒ dual 化**不构成覆盖**（凑数） |
| 114 | `test_pipeline_debug` | `examples/riscv-mini/tests/test_pipeline_debug.cpp` | `examples/riscv-mini/CMakeLists.txt:91` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 115 | `test_pipeline_exec` | `examples/riscv-mini/tests/test_pipeline_exec.cpp` | `examples/riscv-mini/CMakeLists.txt:78` | **不可 dual** | R6 #4 | 含 SECTION ⇒ 双 run 结构下重入语义不一致 |
| 116 | `test_pipeline_lib` | `tests/chlib/test_pipeline.cpp` | `tests/CMakeLists.txt:172` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 117 | `test_pipeline_minimal` | `examples/riscv-mini/tests/test_pipeline_minimal.cpp` | `examples/riscv-mini/CMakeLists.txt:90` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 118 | `test_pipeline_wrapper` | `examples/riscv-mini/tests/test_pipeline_wrapper.cpp` | `examples/riscv-mini/CMakeLists.txt:92` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 119 | `test_pod_bundle_conversions` | `tests/test_pod_bundle_conversions.cpp` | `tests/CMakeLists.txt:422` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 120 | `test_popcount` | `tests/test_popcount.cpp` | `tests/CMakeLists.txt:531` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 121 | `test_precise_width` | `tests/test_precise_width.cpp` | `tests/CMakeLists.txt:532` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 122 | `test_reg` | `tests/test_reg.cpp` | `tests/CMakeLists.txt:389` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 123 | `test_reg_linking` | `tests/test_reg_linking.cpp` | `tests/CMakeLists.txt:390` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 124 | `test_reg_timing` | `tests/test_reg_timing.cpp` | `tests/CMakeLists.txt:391` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 125 | `test_riscv_tests` | `examples/riscv-mini/tests/test_riscv_tests.cpp` | `examples/riscv-mini/CMakeLists.txt:80` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 126 | `test_rv32i_pipeline` | `tests/test_rv32i_pipeline.cpp` | `tests/CMakeLists.txt:561` | **不可 dual** | R6 #4 | 含 SECTION ⇒ 双 run 结构下重入语义不一致 |
| 127 | `test_rv32i_pipeline_behavior` | `tests/test_rv32i_pipeline_behavior.cpp` | `tests/CMakeLists.txt:565` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 128 | `test_rv32i_pipeline_example` | `examples/riscv-mini/tests/test_rv32i_pipeline.cpp` | `examples/riscv-mini/CMakeLists.txt:97` | **不可 dual** | R6 #4 | 含 SECTION ⇒ 双 run 结构下重入语义不一致 |
| 129 | `test_rv32i_soc_dual_parity` | `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | `examples/riscv-mini/CMakeLists.txt:106` | **已 dual（hand-crafted）** | 已 dual（hand-crafted） | 同一 TEST_CASE 内 **2×Simulator + InterpreterBackend + VerilatorBackend** ⇒ **语义上**就是双后端 parity（只是没用 macro）⇒ 已计入 38 |
| 130 | `test_sdata_compare` | `tests/test_sdata_compare.cpp` | `tests/CMakeLists.txt:534` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 131 | `test_selector_arbiter` | `tests/chlib/test_selector_arbiter.cpp` | `tests/CMakeLists.txt:173` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 132 | `test_selector_priority` | `tests/chlib/test_selector_priority.cpp` | `tests/CMakeLists.txt:174` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 133 | `test_selector_round_robin` | `tests/chlib/test_selector_round_robin.cpp` | `tests/CMakeLists.txt:175` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 134 | `test_sequential` | `tests/chlib/test_sequential.cpp` | `tests/CMakeLists.txt:176` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 135 | `test_shift_boundary` | `tests/test_shift_boundary.cpp` | `tests/CMakeLists.txt:533` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 136 | `test_simple_io` | `tests/test_simple_io.cpp` | `tests/CMakeLists.txt:407` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 137 | `test_simulator_bundle` | `tests/test_simulator_bundle.cpp` | `tests/CMakeLists.txt:423` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 138 | `test_simulator_trace` | `tests/chlib/test_simulator_trace.cpp` | `tests/CMakeLists.txt:177` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 139 | `test_soc_hello_world` | `examples/riscv-mini/tests/test_soc_hello_world.cpp` | `examples/riscv-mini/CMakeLists.txt:79` | **不可 dual** | R6 #4 | 含 SECTION ⇒ 双 run 结构下重入语义不一致 |
| 140 | `test_source_info` | `tests/test_source_info.cpp` | `tests/CMakeLists.txt:410` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 141 | `test_state_machine` | `tests/test_state_machine.cpp` | `tests/CMakeLists.txt:453` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 142 | `test_stream` | `tests/chlib/test_stream.cpp` | `tests/CMakeLists.txt:178` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 143 | `test_stream_arbiter` | `tests/test_stream_arbiter.cpp` | `tests/CMakeLists.txt:437` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 144 | `test_stream_builder` | `tests/test_stream_builder.cpp` | `tests/CMakeLists.txt:438` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 145 | `test_stream_connect_dual` | `tests/test_stream_connect_dual.cpp` | `tests/CMakeLists.txt:216` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 146 | `test_stream_fifo_dual` | `tests/test_stream_fifo_dual.cpp` | `tests/CMakeLists.txt:228` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 147 | `test_stream_mux_demux` | `tests/chlib/test_stream_mux_demux.cpp` | `tests/CMakeLists.txt:374` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 148 | `test_stream_operators` | `tests/test_stream_operators.cpp` | `tests/CMakeLists.txt:439` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 149 | `test_stream_pipeline` | `tests/test_stream_pipeline.cpp` | `tests/CMakeLists.txt:440` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 150 | `test_stream_width_adapter` | `tests/test_stream_width_adapter.cpp` | `tests/CMakeLists.txt:541` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 151 | `test_switch` | `tests/chlib/test_switch.cpp` | `tests/CMakeLists.txt:179` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 152 | `test_tlm_bundle_phase1` | `tests/test_tlm_bundle_phase1.cpp` | `tests/CMakeLists.txt:424` | **不可 dual** | R6 #1/#5 | 含 STATIC_REQUIRE（编译期元函数，无 runtime 语义） |
| 153 | `test_trace` | `tests/test_trace.cpp` | `tests/CMakeLists.txt:576` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 154 | `test_uint_compare` | `tests/test_uint_compare.cpp` | `tests/CMakeLists.txt:521` | **已 dual** | 已 dual（macro） | 含 DUAL_BACKEND_TEST_PAIR 的**调用** ⇒ 已计入 38 |
| 155 | `test_user_tracking` | `tests/test_user_tracking.cpp` | `tests/CMakeLists.txt:577` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 156 | `test_verilator_backend` | `tests/test_verilator_backend.cpp` | `tests/CMakeLists.txt:606` | **不可 dual（契约锁定）** | 契约锁定 | capability Requirement『现有 9 个 verilator 测试 MUST 保留原状』⇒ **MUST NOT** 迁移到 DUAL_BACKEND_TEST_PAIR（spec.md:145-147）；另有 >64-bit 端口 `ch_out<ch_uint<128>>`（R6 #3，tests/test_verilator_backend.cpp:711） |
| 157 | `test_verilator_bundle_e2e` | `tests/test_verilator_bundle_e2e.cpp` | `tests/CMakeLists.txt:635` | **不可 dual（契约锁定）** | 契约锁定 | capability Requirement『现有 9 个 verilator 测试 MUST 保留原状』⇒ **MUST NOT** 迁移到 DUAL_BACKEND_TEST_PAIR（spec.md:145-147） |
| 158 | `test_verilator_complex_e2e` | `tests/test_verilator_complex_e2e.cpp` | `tests/CMakeLists.txt:672` | **不可 dual（契约锁定）** | 契约锁定 | capability Requirement『现有 9 个 verilator 测试 MUST 保留原状』⇒ **MUST NOT** 迁移到 DUAL_BACKEND_TEST_PAIR（spec.md:145-147） |
| 159 | `test_verilator_e2e_harness` | `tests/test_verilator_e2e_harness.cpp` | `tests/CMakeLists.txt:614` | **不可 dual（契约锁定）** | 契约锁定 | capability Requirement『现有 9 个 verilator 测试 MUST 保留原状』⇒ **MUST NOT** 迁移到 DUAL_BACKEND_TEST_PAIR（spec.md:145-147） |
| 160 | `test_verilator_error_flow` | `tests/test_verilator_error_flow.cpp` | `tests/CMakeLists.txt:693` | **不可 dual（契约锁定）** | 契约锁定 | capability Requirement『现有 9 个 verilator 测试 MUST 保留原状』⇒ **MUST NOT** 迁移到 DUAL_BACKEND_TEST_PAIR（spec.md:145-147）；另含 `STATIC_REQUIRE`（R6 #1/#5） |
| 161 | `test_verilator_memory_e2e` | `tests/test_verilator_memory_e2e.cpp` | `tests/CMakeLists.txt:659` | **不可 dual（契约锁定）** | 契约锁定 | capability Requirement『现有 9 个 verilator 测试 MUST 保留原状』⇒ **MUST NOT** 迁移到 DUAL_BACKEND_TEST_PAIR（spec.md:145-147） |
| 162 | `test_verilator_stream_e2e` | `tests/test_verilator_stream_e2e.cpp` | `tests/CMakeLists.txt:646` | **不可 dual（契约锁定）** | 契约锁定 | capability Requirement『现有 9 个 verilator 测试 MUST 保留原状』⇒ **MUST NOT** 迁移到 DUAL_BACKEND_TEST_PAIR（spec.md:145-147） |
| 163 | `test_verilator_trace_parity` | `tests/test_verilator_trace_parity.cpp` | `tests/CMakeLists.txt:624` | **不可 dual（契约锁定）** | 契约锁定 | capability Requirement『现有 9 个 verilator 测试 MUST 保留原状』⇒ **MUST NOT** 迁移到 DUAL_BACKEND_TEST_PAIR（spec.md:145-147） |
| 164 | `test_verilog_arith` | `tests/test_verilog_arith.cpp` | `tests/CMakeLists.txt:578` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 165 | `test_verilog_compare` | `tests/test_verilog_compare.cpp` | `tests/CMakeLists.txt:579` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 166 | `test_verilog_external` | `tests/test_verilog_external.cpp` | `tests/CMakeLists.txt:599` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 167 | `test_verilog_gen` | `tests/test_verilog_gen.cpp` | `tests/CMakeLists.txt:580` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 168 | `test_verilog_shift` | `tests/test_verilog_shift.cpp` | `tests/CMakeLists.txt:590` | **不可 dual** | R6 #2 | 裸 context（无 ch_device / ch_module 端口） |
| 169 | `test_wires_connection` | `tests/test_wires_connection.cpp` | `tests/CMakeLists.txt:699` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 170 | `test_wrapper_ifstage` | `examples/riscv-mini/tests/test_wrapper_ifstage.cpp` | `examples/riscv-mini/CMakeLists.txt:94` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 171 | `test_wrapper_pipeline` | `examples/riscv-mini/tests/test_wrapper_pipeline.cpp` | `examples/riscv-mini/CMakeLists.txt:95` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |
| 172 | `test_wrapper_simple` | `examples/riscv-mini/tests/test_wrapper_simple.cpp` | `examples/riscv-mini/CMakeLists.txt:93` | **可 dual** | 可 dual | 含 ch_device/ch_module + 有 Simulator 调用 + 无 STATIC_REQUIRE + 无 >64-bit 端口 + 无 SECTION |

## 4. 分类分布

| 判定 | 条目数 |
|---|---|
| 不可 dual | 98 |
| 可 dual | 33 |
| 已 dual | 28 |
| 不可 dual（契约锁定） | 9 |
| 可 dual（无 runtime 语义） | 3 |
| 已 dual（hand-crafted） | 1 |

| R6 类别 | 条目数 |
|---|---|
| R6 #2 | 67 |
| 可 dual | 33 |
| 已 dual（macro） | 28 |
| R6 #1/#5 | 23 |
| 契约锁定 | 9 |
| R6 #4 | 8 |
| 可 dual 但无 runtime 语义 | 3 |
| 已 dual（hand-crafted） | 1 |

## 5. 文件 → 条目 映射

- 注册调用共 **175** 个（172 个进 CTest + 3 个只构建），涉及源文件 **172** 个
  ⇒ **平均每文件 1 个条目**。
- **多文件一个条目**：（无）
- **一个文件多个条目**：（Catch2 注册路径上无）
- **alias 条目**（不额外占源文件）：`test_verilator_three_way` ⇒ `test_codegen_literal_mask`（`tests/CMakeLists.txt:88`）⇒ **1 文件 2 条目**。

## 6. spec 候选 grep 的盲区（实测）

`dual-backend-test-coverage` 的候选 grep 用 `ch_out<ch_uint<[0-9]\{3,\}>>` 匹配 **>64-bit 端口**
⇒ 它只能命中 **≥100 位** ⇒ **65–99 位**的端口**漏网**。
实测（2026-10-28）：

- 全仓**真实代码**里的 >64-bit `ch_out<ch_uint<N>>` 只有 **1** 处 ——
  `tests/test_verilator_backend.cpp:711` 的 `ch_out<ch_uint<128>>`
  ⇒ 而该文件是 capability「9 个 verilator 测试 MUST 保留原状」的**契约锁定**对象
  ⇒ ⇒ **不在「可转 dual」的分子里** ⇒ R6 #3 分类数 = **0**；
- 唯一出现的 **65–99 位**（96 位）在 `tests/test_dual_backend_64_bit.cpp:12` 的
  **注释**里（描述「EXCEEDS the cap」）⇒ 剥注释后不匹配 ⇒ 该盲区**当前无实际影响**。

⇒ ⇒ 但它是**潜在**漏洞：将来出现 65–99 位的**真实**端口时，spec 的 grep 会把它
当候选 ⇒ 建议 planner 裁决是否收紧为 `[0-9]\{2,3\}` 或直接判 `N > 64`。

## 7. 上限估算（双侧）

| 口径 | 构成 | 条目数 |
|---|---|---|
| **已计入 38**（判据数的是**带标签的条目**，不是 dual pair 数） | 当前带 `verilator_e2e` 标签的全部条目 | 38 |
| ├ 其中真 dual pair（`DUAL_BACKEND_TEST_PAIR` 的**调用**） | — | 28 |
| ├ 其中 hand-crafted 真 dual（2×Simulator + 两个 backend 类） | — | 1 |
| └ 其中**契约锁定**（9 个 verilator 参考实现，MUST 保留原状） | **不得**转 dual | 9 |
| 新增（转 dual 即 +1） | 纯 `base` 标签且通过 R6 | 33 |
| **保守上限** | 已计入 38 + 新增 33（只算 Catch2 单元测试条目） | **71** |
| 乐观上限 | 保守上限 + 把 `spinalhdl-ported` 演示程序（25 个）也改造成 dual 形式 | **96** |

⇒ **判据 `ctest -L verilator_e2e` ≥ 100 的可达性**：

- **保守口径：不可达**（上限 71 < 100，差 29）
- **乐观口径：同样不可达**（上限 96 < 100，差 4）—— 即便把 **25 个 `main()` 演示程序**
  全部改写成 dual 形式也**仍然到不了**判据 ⇒ ⇒ 「凑数」这条退路**也不够**

⇒ ⇒ **口径 A（现状判据 = 标签条目数）**：上限 ∈ **[71, 96]**

⚠️ **「上限」的边界（完整表述见 §9.2.1）**：上述是「**语义诚实**的上限」——
口径 A 的**技术上限其实无界**（给任意既有条目加 `verilator_e2e` 标签即可 +1，
理论上可到 202）⇒ 但那正是「奖励加标签而不是加 parity」⇒ ⇒ 「保留 ≥ 100」
**没有诚实路径**（只有不诚实路径）⇒ ⇒ 且本盘点**不**覆盖「新增有意义的
dual 测试文件」这条路线（⇒ §9.2.2 的轴 ③）⇒ ⇒ 结论是
「**判据在现有语料内不可达**」，**不是**「判据不可达」。

### ⚠️ 口径 A 度量的**不是**「双后端覆盖率」

当前 **38** 个带标签条目的**真实构成**：

- **28** 个用 `DUAL_BACKEND_TEST_PAIR` 的**调用**（⚠️ 不是「提到」——
  `test_verilator_trace_parity.cpp` 等参考实现只在**注释**里提到该宏 ⇒ 子串匹配会误判）；
- **1** 个是 **hand-crafted 真 dual**（`test_rv32i_soc_dual_parity`：同一 TEST_CASE 内
  **2×Simulator + InterpreterBackend + VerilatorBackend** ⇒ 语义上就是双后端 parity，
  只是没用 macro）；
- **9** 个是 capability 明文「MUST 保留原状」的**单后端** verilator 参考实现
  ⇒ **它们不是双后端 parity，且契约禁止把它们迁到 macro**。

⇒ ⇒ **按标签计数会把 9 个「单后端 verilator e2e」算成「双后端覆盖」**
⇒ ⇒ 判据若不改度量单位，**它奖励的是「加标签」而不是「加 parity」**。

⇒ ⇒ **口径 B（真 dual pair 数，语义正确）**：上限 ∈ **[62, 87]**

| 口径 | 保守上限 | 乐观上限 | ≥100 可达？ |
|---|---|---|---|
| A：标签条目数（**现状判据**） | 71 | 96 | 保守 ❌ / 乐观 ❌（仍差 4） |
| B：真 dual pair 数（语义正确） | 62 | 87 | 保守 ❌ / 乐观 ❌ |

⇒ ⇒ **两种口径下，保守上限都 < 100** ⇒ 「盲做到底」**必然**到不了判据
⇒ ⇒ **结论以区间表述**：口径 A ∈ **[71, 96]**，口径 B ∈ **[62, 87]** ⇒
**这就是需要 planner 裁决的结论**（判据的**度量单位** + 是否允许把 demo 计入）
⇒ ⚠️ **build 不得自行改 objective 的 Go 判据**（`objective-test-coverage-100pct` 属 planner 职责）。

### ⚠️ **推翻**前次审查的若干数字（实测订正）

| 前次审查的说法 | 实测 | 判定 |
|---|---|---|
| 「含 `DUAL_BACKEND_TEST_PAIR` 的 `.cpp` 文件 = **38** 个 ⇒ 与标签条目数**数值巧合**」 | 真**调用**只有 **29** 个（macro 28 + hand-crafted 1）；另 **10** 个文件只在**注释**里提到该宏 | ❌ **数字错**（38 是**标签**数，不是 dual pair 数） |
| 「带标签但**不含** dual pair 的条目 = **18** 个」 | 实测 **9** 个（且**全部**受「MUST 保留原状」保护 ⇒ **不得**转 dual） | ❌ **数字错**（把 28 个真 dual pair 也算进去了） |
| 「盘点表 175 行」 | 实测 **172** 行（另有 3 个 `riscv_mini_unit_test` target **只构建、不进 CTest** ⇒ 不是 ctest 条目） | ❌ **口径错**（把非 ctest 条目算进了分子） |
| 「可 dual 上限 = 74（乐观）/ 64（保守）」 | 实测口径 A ∈ **[71, 96]**、口径 B ∈ **[62, 87]** | ⚠️ **口径不同 ⇒ 不可比**（标签数 vs 真 dual pair 数） |

## 8. 工作量折算

- **可直接转 dual** 的条目 **33** 个（纯 `base` 标签 + 通过 R6 + 有 runtime 语义）
  ⇒ 按「每批 ≤ 10 个文件」⇒ **至少 4 批**
- 另 **3** 个「可 dual 但**无 runtime 语义**」（含 `ch_device` 但从不驱动 `Simulator`）
  ⇒ dual 化前**还得先补 runtime 断言** ⇒ 工作量更大 ⇒ **不计入**上面的批次数
- 每批的**真实**墙钟**必须实测**（`verilator_backend` 的 `RESOURCE_LOCK` 会把 verilator 测试**串行化** ⇒ 墙钟不可拍脑袋）⇒ 见 capability 里「该锁串行化导致墙钟估算需实测」的备注。
- 另：每个 dual 化都要**变异验证**（证明断言对两个后端都有判别力）⇒ 批次工作量 ≈ 「改 CMake + 补 dual pair + 写断言 + 变异验证」，**不是**「加一个标签」。

## 9. 交付给 planner 的**判据修订请求**

⚠️ **build 不得自行改 objective 的 Go 判据**（`objective-test-coverage-100pct` 属 planner 职责）
⇒ ⇒ 本 change 只**提交实测证据 + 候选修订值 + 取舍集**：

### 9.1 判据的**度量单位**问题（更根本）

现状判据「`ctest -L verilator_e2e` ≥ 100」数的是**带标签的条目**，而这 38 个里：

- **28** 个是 `DUAL_BACKEND_TEST_PAIR` 的**调用**；
- **1** 个是 hand-crafted 真 dual（`test_rv32i_soc_dual_parity`）；
- **9** 个是 capability 明文「MUST 保留原状」的**单后端** verilator 参考实现
  ⇒ **它们不是双后端 parity**，且契约**禁止**把它们迁到 macro。

⇒ ⇒ **按标签计数会把「单后端 verilator e2e」算成「双后端覆盖」**
⇒ ⇒ 判据若不改度量单位，**它奖励的是「加标签」而不是「加 parity」**。

### 9.2 候选修订（供 planner 裁决）

#### 9.2.1 先厘清「上限」的**边界**（否则结论会被误读）

⚠️ 本盘点数的是**现有** ctest 条目 ⇒ 下述上限是「**现有语料内**」的上限，
**不**覆盖「**新增**有意义的 dual 测试文件」这条路线
⇒ 那正是 objective §1 原本的 Phase 5 主路线 ⇒ 本次盘点**证伪的是它的
「必要性」**，**不是**它的「可行性」（见 §9.4）。

⚠️ 另：**口径 A 的技术上限其实无界** —— 判据数的是「带 `verilator_e2e` 标签的
ctest 条目」⇒ 给**任意**既有条目（包括那 98 个判为「不可 dual」的）加上该标签
即可让指标 +1 ⇒ 理论上可到 **202** ⇒ ⇒ 但那正是 §9.1 指出的
「**奖励加标签而不是加 parity**」⇒ ⇒ 故本盘点给出的 **71 / 96** 是
「**语义诚实**的上限」（新增的标签背后**真的有**双后端 parity）
⇒ ⇒ 「保留 ≥ 100」在当前仓内**没有诚实路径**（只有**不诚实**路径）。

#### 9.2.2 planner 的取舍集（**4 个轴**，请一并裁决）

| # | 方向 | 说明 | 对上限的影响 |
|---|---|---|---|
| ① | **改阈值** | 保持口径 A，阈值降到 ≤ 71 | 立即可达（**但**仍奖励加标签） |
| ② | **改度量单位** | 改数「真 dual pair 数」⇒ 阈值 ≤ 62 | 立即可达（**推荐**：奖励 parity 而非标签） |
| ③ | **新增有意义的 dual 测试文件** | Phase 5 原本的主路线（objective §1）⇒ 本盘点**未覆盖**这条路径 | 理论上**可**到 ≥ 100 ⇒ 但那是「**新增覆盖**」不是「转换」⇒ 工作量另计 |
| ④ | **放松 R6 排除清单** | R6 #2 的 **67** 个裸 context 条目若改写为 component 包装形式 ⇒ 上限可显著抬高 | 可抬高 ⇒ 但那是**改契约** ⇒ 需 planner 显式裁决 |

⚠️ ⇒ **结论不是「判据不可达」，而是「判据在现有语料内不可达」**
⇒ ⇒ 请 planner 在 ①②③④ 中**显式选择**并**记录该取舍**。

#### 9.2.3 候选阈值（**仅当**选 ① 或 ② 时适用）

| 候选 | 口径 | 诚实上限（保守 / 乐观） | 建议阈值 | **margin 的理由** |
|---|---|---|---|---|
| **B（语义正确，推荐）** | 真 dual pair 数 | **62 / 87** | **≥ 60** | 留 **2** 个条目余量 ⇒ 吸收「条目退役 / hand-crafted 条目被合并 / 某条目因 R6 新增类别被判死」等波动 ⇒ 又不至于把阈值定到「必须全量转换才达标」的脆弱值 |
| A（现状口径） | 标签条目数 | **71 / 96** | **≥ 70** | 留 **1** 个条目余量（同上）⇒ 余量比 B 更紧，是因为**口径 A 本身会把「非 parity」的标签计入** ⇒ 它的可达标空间已被**高估** ⇒ 余量须更保守 |

⚠️ 两个候选的阈值都**必须** ≤ 对应口径的**诚实保守上限** ⇒ 否则就是「硬凑」的判据。
⚠️ 阈值最终由 **planner** 裁决 ⇒ build 只给候选值与 margin 理由。

### 9.3 objective 文件里的 `37`：实测是 **6 处**，其中 **4 处需处理**、**2 处是历史记录**

`objective-test-coverage-100pct` 里 `37` 共出现 **6** 次（实测 `grep -n "37"`）：

| 行 | 内容 | 性质 | 本盘点的判定 |
|---|---|---|---|
| 25 / 26 | 「34 @ 2026-10-23 → 35 → 37」的状态快照链 | **历史记录** | ⚠️ **不改**（历史事实照录） |
| 37 | 「实测 **37** = 含 `DUAL_BACKEND_TEST_PAIR` 的 `.cpp` 文件数」 | **单位陈述** | ❌ **被证伪**：实测含 macro **调用**的文件只有 **28** 个（另 1 个 hand-crafted 真 dual、9 个契约锁定的单后端参考实现）⇒ 37 是**标签条目数** ⇒ **必须改** |
| 45 | done-when：「**2026-10-26 实测 37** ⇒ 未达成」 | **当前值** | ⚠️ 现值 **38** ⇒ **要改** |
| 72 | §9.2 Go 判据：「**2026-10-27 复核仍 37**；⚠️ 单位 = **dual 文件数**」 | **当前值 + 单位陈述** | ⚠️ 数值陈旧 + 单位错 ⇒ **要改** |
| 93 | 历史表 `revise` 行（引用同一处单位陈述） | **历史记录** | ⇒ 建议**加注**（指向本 change 的订正），不改写历史行 |

⇒ ⇒ **请求 planner** 处理上述 **4** 处（L37 / L45 / L72 / L93 加注），
且 L37 / L72 / L93 的「单位 = dual 文件数」陈述**必须**改成
「单位 = **带 `verilator_e2e` 标签的 ctest 条目数**」。

### 9.4 ⚠️ 第三个被证伪的前提：「在既有文件里加 dual pair **不会**推进本指标」

objective §1 写：

> ⇒ 「≥ 100」= **100 个 dual 文件**。⚠️ 因此「在既有文件里加 dual pair」
> **不会**推进本指标（它只增加覆盖广度，不增加文件数）⇒ Phase 5 的全量推进
> MUST 以**新文件**为主

⚠️ **本次盘点证伪其中的「不会推进」**：

- 该指标数的是**带 `verilator_e2e` 标签的 ctest 条目**（不是「含 macro 的文件数」）；
- 一个**既有**的 `base` 标签条目，只要把它的注册从 `add_catch_test` 改成
  `add_dual_backend_test`（它本来就有 `ch_device` + `Simulator`）⇒
  **该条目就带上了 `verilator_e2e` 标签 ⇒ 指标 +1**；
- 本次盘点的 **33** 个「可 dual」条目**全部**是**既有文件** ⇒ ⇒ 它们**全部**
  能通过「改注册方式」推进指标（**不需要**新建文件）。

⇒ ⇒ objective 给 Phase 5 的指导「MUST 以**新文件**为主」作为**必要性**是
**误导性**的（它基于「加 pair 不推进指标」这个已被证伪的前提）
⇒ ⇒ 若 planner 保留口径 A，Phase 5 的**转换路线**应是「**改既有条目的注册**」
⇒ ⇒ 而这会显著改变工作量估算（**不**产生新测试文件 ⇒
`EXAMPLES_TEST_SOURCES` / `CTEST_TOTAL` 锚点**均不变**）

⚠️ **但请注意区分「必要性」与「可行性」**（本盘点**只**证伪前者）：
「**新增**有意义的 dual 测试文件」**仍然是可行路线**（⇒ §9.2.2 的轴 ③）
⇒ ⇒ 它**不在**本盘点的覆盖范围内（盘点的是现有条目）⇒ ⇒ 若 planner 选轴 ③，
工作量需**另做**盘点（新增 dual 文件的**内容**规划 = Phase 5 的正题）。

### 9.5 顺带发现的**契约缺口**（同样是 planner 职责）

- **R6 候选 grep 的盲区**：`ch_out<ch_uint<[0-9]\{3,\}>>` 只匹配 **≥100 位**
  ⇒ **65–99 位**的 >64-bit 端口**漏网** ⇒ 将来出现这类端口时，grep 会把它当候选
  ⇒ 建议收紧为 `[0-9]\{2,3\}` 或直接判 `N > 64`。
  （实测：全仓**真实代码**里的 >64-bit `ch_out<ch_uint<N>>` 只有 **1** 处 ——
  `tests/test_verilator_backend.cpp:711` 的 `ch_out<ch_uint<128>>` ⇒ 而该文件
  是**契约锁定**对象 ⇒ 不在「可转 dual」的分子里 ⇒ 当前无实际影响；
  唯一出现的 96 位在 `tests/test_dual_backend_64_bit.cpp:12` 的**注释**里。）
