# 验收报告 —— `rv32i-mem-backpressure-2026-11`（S2 · MEM 级反压）

- **验收者**：verifier（角色内建 · builder 会话内执行）
- **日期**：2026-11-23 · **范围**：AC1–AC8 + 遗留清扫
- **change**：`openspec/changes/rv32i-mem-backpressure-2026-11/`
- **判据来源**：已签批 `openspec/specs/rv32i-memory-timing/spec.md` 的
  「MEM 级的反压接口 MUST 有可观测的语义」（2 个 Scenario）+ `ADR-069:176`（口径分档）

## 结论

**✅ 8 条 AC 全部成立** ⇒ 本 change **可归档**。

**一句话**：`data_ready` 从「接了线但没落地」变为**真消费** —— 拉低它时
`EX/MEM` / `M2` / `MEM/WB` 冻结、`IF/ID` 与 `ID/EX` 同时停、退休不增、
解除后写回序列与无反压完全一致；而**核的 34 端口面与行为零回归**。

## 逐条判定

| AC | 判定 | 证据（本轮实跑） |
|---|---|---|
| **AC1** MEM/WB 冻结 | ✅ | `test_rv32i_mem_backpressure` 的 `wb_data` / `wb_rd` 在 `[from+1, from+len]` 逐拍全等（`select(mem_stall, memwb_X, m2_X)`） |
| **AC2** EX/MEM + M2 冻结 | ✅ | 同测试的 `dbg_exmem_alu_result` 逐拍全等（M2 同源冻结） |
| **AC3** 向前级 stall | ✅ | `dbg_id_pc` 不变 · `perf_instructions` 不增（冻结期间） |
| **AC4** 有效链自洽 | ✅ | 反压 run 的**写回事件序列** == plain 的前缀（去重后逐项相等）⇒ 无多写 / 错写 / 丢写 |
| **AC5** 非零覆盖 + 手推拍数 | ✅ | `plain.final_instrs - stall.final_instrs == kStallLen`（**手推：冻结型 ⇒ 少退休 N 条**）· 且「去掉 `data_ready=0` 驱动 ⇒ PC 冻结判据 MUST 失败」的**覆盖自证**用例通过 |
| **AC6** 口径分档 | ✅ | `plain.final_load_use == 4`（手推：ADR-047 修订后 load-use = 4 拍）· `stall.final_load_use == 4`（反压 6 拍**未混入**） |
| **AC7** `ready≡1` 行为零回归 | ✅ | 核侧 `flat` / `hier` 顶层端口 **34** 根 · 端口名集 **IDENTICAL** · `RESULT: checks=24 fails=0` · `test_rv32i_soc_dual_parity` **Passed**（逐拍等价） |
| **AC8** CTest 注册 | ✅ | `add_catch_test(test_rv32i_mem_backpressure ...)` · CTest 总数 **273 → 274**（锚点已同步） |

### AC7 的实施期订正（已回写 proposal）

**原判据「核侧三份产物 sha256 逐字节不变」不成立** ⇒ 实测 `flat/hier/fw` 全部 DIFF（400 行）。
根因：`mem_stall = !io().data_ready` 是**真实端口**上的逻辑 ⇒ **Verilator 不做常量折叠**
（`data_ready` 在核里接恒 1，但那是**接线**不是常量）⇒ 新增的 mux / 反相器留在产物里。

⇒ ⇒ 判据改为**可判定的两条**：① 顶层端口表逐字不变（34 根 · 名集会比）② 行为零回归
（24 判据 + 双后端逐拍等价）。⇒ ⇒ 新逻辑**不是死代码**：`data_ready` 是真实输入，
S3 接 AXI4 主口后它会动。

## 实施期发现的另外两个问题（都已修）

| 问题 | 根因 | 处置 |
|---|---|---|
| **退休计数重复** | `commit_valid = mem_stage.io().mem_valid_out`；反压时它**冻结为 1** ⇒ 每拍 +1 ⇒ 同一条指令重复计数 | `commit_valid &= !mem_stall`（反压期间不退休）+ 注释 |
| **load-use 程序测不出停顿** | `lw` 紧贴第 1 条时，消费者进 ID 时 load 已过 EX 级 ⇒ 检测点错过（`perf_load_use_stalls == 0`） | 测试程序加 **4 条 NOP 前缀**（流水线启动稳定）⇒ `load_use == 4` |

## 回归（本轮实跑）

- `ctest` 全量：**274 / 274 全绿**（实测 `100% tests passed, 0 tests failed out of 274`）
- `run_all_ported_tests.sh`：**PASS 29 / FAIL 0**
- 关键 target：`test_rv32i_soc_dual_parity` · `test_rv32i_hier_rtl` · `test_rv32i_stage_dual`
  · `test_rv32i_mem_backpressure` ⇒ 4/4 Passed
- 护栏：`verify_riscv_soc_core_ports` · `verify_no_duplicate_rv32i_headers`(+self-test)
  · `verify_port_naming` · `verify_examples_rtl_sim` · `verify_hierarchical_rtl`
  · `verify_input_port_additions` · `verify_examples_rtl_sim_coverage` · `verify_docs_links`
  · `verify_agents_size` · `verify_roadmap_fragments` · `verify_arch_ledger_residuals`
  · `verify_guard_reachability` · `verify_rv32i_fw_init_rtl` · `verify_dtcm_read_timing`
  · `verify_test_counts` ⇒ 全 PASS

## 遗留清扫（本次范围内）

| 项 | 处置 |
|---|---|
| TODO / 半截实现 / 临时调试日志 | **无**（临时探针 `TEST_CASE("DBG 临时探针")` 已删；`printf` 一并移除） |
| 新增输入端口未驱动 | ✅ 全部 **5** 个 `IdStage` 实例化点显式驱动（`rv32i_pipeline.h` + 4 个测试） |
| 新增端口命名债 | ✅ `freeze` 用**具名构造**（`ch_in<ch_bool> freeze{"freeze"}`）⇒ **不增债务**（不登记豁免） |
| 登记面同步 | ✅ `include/cpu/AGENTS.md` 行数 6745 → **6804**（两处：正文 + STRUCTURE 树）· `CTEST_TOTAL` 273 → **274** · `non-perf` 262 → **263** · `tests/test_*.cpp` 163 → **164** |
| **范围外遗留（只记录 · 未动）** | ① **`perf_mem_stalls` 不可观测**：显式分档的代价（ADR-069 允许二选一）⇒ 归 **S3**（那时核端口面本来就要大改）<br>② **ADR-069 V4 的判据表述**：「不随**停顿**冻结」在新实现下有歧义 ⇒ 建议 architect 加 dated addendum 限定为「load-use 停顿」（本 change 不单方面改 ADR）<br>③ **verilator 后端的反压等价**：本测试单后端（拍级语义）；双后端等价由 `test_rv32i_soc_dual_parity` 承担（它跑核 = pipeline 实装配，但 `data_ready` 恒 1 ⇒ **不覆盖反压路径**）⇒ 归 S3 一并验 |

## 已用轮次

**1 / 3**（一次「实现 → 测试 → 修 2 个自身缺陷 → 验证」，无整轮返工）。

## 下一步（母 improvement 批次序列）

```
✅ S2（本次 · 已签批 spec 补齐 · 零端口变化 · 行为零回归）
  ↓
🔜 G-7 总线类裁决（Lite vs full AXI4）—— 纯决策，无代码
  ↓
S3  核数据侧 AXI4 主口 + D-L1 + burst 升级 ← `data_ready` 在这里第一次真正被拉低
     （TCM 归属 / 成层决策也在这一批）
```
