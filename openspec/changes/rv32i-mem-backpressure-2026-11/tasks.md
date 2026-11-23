# Tasks: RV32I MEM 级反压

> 每条 = 一个可验证的原子动作。判据编号对应 `proposal.md` 的 AC1–AC8。

## T1 基线

- [x] **T1.1** 记录基线：核侧三份产物（flat / hier / fw-init）的 `top.v` +
  `manifest.json` 的 sha256（AC7 的「逐字节不变」要可证伪）
- [x] **T1.2** 记录基线：`tb_rv32i_soc` 的 `RESULT: checks=24 fails=0` 输出

## T2 `IdStage` 新增 `freeze`

- [x] **T2.1** `include/cpu/pipeline/stages/id_stage.h`：`__io` 加
  `ch_in<ch_bool> freeze;`（注释写明「冻结 ≠ 装气泡」，与 `stall` 的差异）
- [x] **T2.2** 同文件 `describe()`：`idex_update_en &= !freeze` ·
  `idex_bubble &= !freeze` · `idex_valid` 加 `select(freeze, 保持, …)` 外层
- [x] **T2.3** 全部 **5** 个实例化点显式驱动 `freeze`
  （`rv32i_pipeline.h:141` + `test_pipeline_debug.cpp:114` ·
  `test_pipeline_exec.cpp:268` · `test_rv32i_stage_dual.cpp:275/385`）
  ⇒ ⇒ 测试里驱动 `0`（保持既有行为）

## T3 `Rv32iPipeline` 反压通路

- [x] **T3.1** 定义 `auto mem_stall = !io().data_ready;`（位置：`build_control_signals`
  或 `build_datapath` 的开头，需在两个 helper 都可见 ⇒ 用成员或参数传递）
- [x] **T3.2** `EX/MEM`：`exmem_en` 加 `& !mem_stall`；**清零型**字段
  （`exmem_valid` / `exmem_is_store` / `exmem_is_load`）加
  `select(mem_stall, 原值, …)` 外层
- [x] **T3.3** `M2` 的 **8** 个字段加 `select(mem_stall, m2_X, exmem_X)`
- [x] **T3.4** `MEM/WB` 的 **8** 个字段加 `select(mem_stall, memwb_X, m2_X)`
- [x] **T3.5** `if_stage.io().stall` 驱动加 `| mem_stall`
- [x] **T3.6** `id_stage.io().freeze <<= mem_stall;`
- [x] **T3.7** `perf_load_use_stalls` 的驱动**不动**（仍 = `hazard.io().stall`）⇒ 分档

## T4 测试（AC1–AC6）

- [x] **T4.1** 新测试文件 `tests/test_rv32i_mem_backpressure.cpp`
  （`ch_device<Rv32iPipeline>` DUT + 指令流 stub + 可控 `data_ready`）
- [x] **T4.2** AC1：反压期间 `dbg_wb_rd_addr` / `dbg_wb_write_data` 逐拍不变
- [x] **T4.3** AC2：反压期间 `dbg_exmem_valid` / `dbg_exmem_alu_result` 逐拍不变
- [x] **T4.4** AC3：反压期间 `dbg_id_pc` 不变 · `perf_instructions` 不增
- [x] **T4.5** AC4：反压 run 与无反压 run 的**退休数**与**写回值序列**逐项相等
- [x] **T4.6** AC5：`perf_cycles` 增量 == 手推 N（>= 2）+
  **覆盖自证**：去掉 `data_ready=0` 驱动时该用例 MUST 失败
- [x] **T4.7** AC6：反压期间 `perf_load_use_stalls` 不增
- [x] **T4.8** CMake 注册（`add_test`）+ 双后端（interpreter + verilator，若可）

## T5 验收

- [x] **T5.1** AC7：核侧三份产物 `top.v` + `manifest.json` sha256 **逐字节不变**
- [x] **T5.2** AC7：`tb_rv32i_soc` ⇒ `RESULT: checks=24 fails=0`
- [x] **T5.3** `ctest` 全量绿（含新 target）· `run_all_ported_tests.sh` PASS
- [x] **T5.4** 护栏回归：`verify_riscv_soc_core_ports` · `verify_no_duplicate_rv32i_headers`
  · `verify_port_naming` · `verify_examples_rtl_sim` · `verify_hierarchical_rtl`
  · `verify_test_counts`（新增 target ⇒ 计数锚点同步）· `verify_input_port_additions`
- [x] **T5.5** 检查 ADR-069 V4 判据**未**被新实现打红（实测：仓内无自动护栏 ⇒
  人工复核 `design.md` §4 的论证成立）
- [x] **T5.6** 验收报告 `.rddf/reviews/rv32i-mem-backpressure-2026-11-verify.md`
- [x] **T5.7** `docs/CHANGELOG.md` 一条（用户可见变更：`IdStage.freeze`）

## T6 簿记

- [x] **T6.1** 母 improvement 回填 S2 状态
- [x] **T6.2** `.rddf/checks/public-api-surface.md`（`IdStage.freeze` 是**新增**输入
  ⇒ 按该表口径：只有「移除 / 改名」才登记 ⇒ **不登记**，但需在 CHANGELOG 说明）
- [x] **T6.3** `AGENTS.md` 的 `CTEST_TOTAL` 锚点 +1（新增 1 个 CTest target）

## 实测记录（2026-11-23 · 一手）

| 判据 | 实测 | 证据 |
|---|---|---|
| AC1/AC2/AC3 冻结 | ✅ | `test_rv32i_mem_backpressure`：`wb_data` / `exmem_alu` / `id_pc` 逐拍全等 |
| AC4 有效链自洽 | ✅ | 写回**事件**序列 == plain 前缀（去重后逐项相等） |
| AC5 手推拍数 + 覆盖自证 | ✅ | `plain - stall == kStallLen`；去驱动 ⇒「PC 冻结」判据必失败 |
| AC6 口径分档 | ✅ | `load_use == 4`（两侧相同 · 手推 ADR-047 修订后 = 4 拍） |
| AC7 行为零回归 | ✅ | 端口 34 · 名集 IDENTICAL · 24 判据 · dual parity Passed |
| AC7 订正 | ⚠️→✅ | **产物字节会变**（Verilator 不折叠真实端口上的逻辑）⇒ 判据已回写为「端口表 + 行为」 |
| AC8 CTest | ✅ | 274/274 · 锚点同步（CTEST_TOTAL / non-perf / test_*.cpp） |
| 实施期修的 2 个自身缺陷 | ✅ | ① 退休计数重复（`commit_valid &= !mem_stall`）② load-use 程序需 4 条 NOP 前缀 |
