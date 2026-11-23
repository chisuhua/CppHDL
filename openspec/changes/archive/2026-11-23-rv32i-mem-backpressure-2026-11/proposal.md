# Change: RV32I MEM 级反压（`data_ready` 真消费）

> **Change 名**：`rv32i-mem-backpressure-2026-11`
> **母 improvement**：`rv32i-soc-platform-2026-11`（批次 **S2** · 状态 `in-progress`）
> **日期**：2026-11-23 · builder
> **性质**：**已签批 spec 的未实现条款补齐**（implementation gap · 零 spec delta）

## Why

1. **已签批 spec 明文要求反压，但实现里是断链**。
   `openspec/specs/rv32i-memory-timing/spec.md` 的
   **「MEM 级的反压接口 MUST 有可观测的语义」**（`:147`）要求：
   上游拉低就绪信号 ⇒ MEM 级 **MUST 冻结其装载寄存器** + **向前级发 stall** + **有效链保持**。
   ⇒ ⇒ 实测现状：
   - `Rv32iPipeline.io().data_ready`（`rv32i_pipeline.h:69`）在 pipeline **内部零消费者**
     （只被 `rv32i_core_top.h:180` 从 `dtcm.io().ready` 接线 ⇒ **接了线但没落地**）
   - `MemStage.__io` **没有任何 ready / stall 输入**
   - `HazardUnit` 的 `stall` 输出**只覆盖 load-use**
   ⇒ ⇒ **该 Requirement 的第二个 Scenario「反压 MUST 有非零覆盖」当场不成立**
   （不存在「能拉低该就绪信号」的 DUT 与用例 ⇒ 而 spec 明文禁止「零覆盖仍判绿」）。

2. **它是 S3（核数据侧总线化）的硬前置**。
   当前 `dtcm.ready ≡ 1`（`rv32i_tcm.h:73/:168/:198` 三处 `ch_bool(true)`）⇒ 反压通路
   从不被触发。接上 AXI4 主口后，总线等待（`rvalid=0` / `bvalid=0`）**必然**要通过这条
   通路反压流水线 ⇒ 现在不打通，S3 只能在工作期临时加，且没有「`ready≡1` 时零回归」的
   独立证据 ⇒ 属**结构性卡点**（母 improvement 已登记）。

3. **不违反已签批 ADR，而是 ADR 预留的例外**。
   `ADR-069`（`Accepted`）：
   - `:134` 明文 **「不做 MEM 级反压（`data_ready` 真反压）⇒ 归 `memory-timing-contract` T3
     ⇒ 本 ADR **零** T3 内容」**
   - `:176` 要求 **「T3 的反压**不得**混进 `perf_load_use_stalls`」**
   ⇒ ⇒ V4「M2 / MEM/WB′ 装载使能恒定（不随停顿冻结）」的「停顿」在其语境里
   = **load-use 停顿**（ADR 自身声明 T3 不在范围内）⇒ ⇒ 本 change 的冻结是
   **T3 的显式例外**，不是推翻 V4 的意图；**判据口径的限定**记入 `design.md` §4。

## What Changes

| 面 | 内容 | 端口 / 产物影响 |
|---|---|---|
| `IdStage` | 新增 `freeze` 输入（**冻结**语义，与 `stall` 的**装气泡**语义分离） | 组件端口 +1（`include/cpu/` 内部契约） |
| `Rv32iPipeline` | ① `mem_stall = !io().data_ready` ② `EX/MEM` / `M2` / `MEM/WB` 在 `mem_stall` 时**冻结** ③ 向 `IF/ID` + `ID/EX` 发 stall / freeze ④ **反压不计入 `perf_load_use_stalls`**（显式分档） | **零**（`__io` 不变） |
| **`Rv32iCoreTop`** | **不改**（`data_ready` 仍接 `dtcm.ready ≡ 1`） | **核侧 34 端口面不变**（⚠️ 产物**字节**会变 —— 见 AC7 的实施期订正） |
| 测试 | 新增「可直接拉低 `data_ready` 的 `ch_device<Rv32iPipeline>` DUT」+ 手推 stall 拍数用例 | 新 CTest target |

⇒ ⇒ **S2 刻意不做的事**：不新增 `perf_mem_stalls` 端口（ADR-069 给了「另立计数器**或**
显式分档」两个选项 ⇒ 本 change 选**显式分档**：反压**不计入**任何性能计数器，
并用**可判定的测试断言**把分档钉住 —— 见 AC6）⇒ ⇒ 因此核端口面 34 **不变**、
soC 侧契约表 / C6 快照 / `verify_riscv_soc_core_ports.sh` **零改动**。

## 验收标准

> 期望值 MUST **手推**（禁抄仿真输出）· 计数 MUST 从产物 / `perf_*` 派生（禁裸数字）。
> 全部判据以 `ch_device<Rv32iPipeline>` 为单位（spec 的对象就是 `Rv32iPipeline` 的
> `data_ready` 输入，不是核的顶层端口）。

- [ ] **AC1** 🔴 `data_ready=0` ⇒ **MEM/WB 寄存器冻结**
  ⇒ 断言：反压期间 `dbg_wb_rd_addr` / `dbg_wb_write_data` **逐拍不变**
- [ ] **AC2** 🔴 `data_ready=0` ⇒ **EX/MEM 与 M2 冻结**
  ⇒ 断言：反压期间 `dbg_exmem_valid` / `dbg_exmem_alu_result` **逐拍不变**
- [ ] **AC3** 🔴 `data_ready=0` ⇒ **向前级发 stall**（IF/ID 与 ID/EX 冻结）
  ⇒ 断言：反压期间 `dbg_id_pc` **不变**且 `perf_instructions` **不增**
- [ ] **AC4** 🔴 **有效链自洽**（**固定拍数**双 run：反压 vs 无反压）
  ⇒ 断言：同一程序在「反压 N 拍」与「无反压」两个 run 下，**退休数相同**、
  **写回值序列相同**（`dbg_wb_write_data` 序列逐项相等）
- [ ] **AC5** 🔴 **反压必须有非零覆盖**（spec Scenario 2）
  ⇒ 断言：反压段 `perf_cycles` 的增量 == **手推的 N 拍**（N 由用例指定，>= 2）
  ⇒ ⇒ 且**去掉 `set_input_value(data_ready, 0)` 的驱动**时该用例 **MUST 失败**
  （=== 覆盖自证：判据对「零覆盖」有判别力）
- [ ] **AC6** 🔴 **口径分档**（ADR-069 `:176`）
  ⇒ 断言：反压期间 `perf_load_use_stalls` **不增**（反压不得混进 load-use 计数器）
  ⇒ ⇒ 实现 = `load_use_stalls += hazard.stall & !mem_stall`（纯 load-use 场景不变）
- [ ] **AC7** 🔴 **`ready≡1` 时行为零回归**
  ⇒ 断言：核侧层次化产物的**顶层端口表**（名 + 方向 + 位宽 + 顺序）**逐字不变**（**34** 根）
  ⇒ ⇒ 且 `tb_rv32i_soc` 的 **24 条判据**全过 · `test_rv32i_soc_dual_parity` 全绿
  （**逐拍等价**：interpreter vs verilator）
  ⇒ ⇒ ⚠️ **实施期订正**：产物**字节**会变 —— `mem_stall = !io().data_ready` 是**真实端口**
  上的逻辑，Verilator **不做常量折叠**（`data_ready` 在核里接恒 1，但那是**接线**不是常量）
  ⇒ ⇒ 所以新增的 mux / 反相器会留在产物里（**当前恒 0，S3 接总线时活起来**）。
  ⇒ ⇒ **原判据「sha256 逐字节不变」不成立**，已改为上面两条（端口表 + 行为）。
- [ ] **AC8** 新用例进 CTest（`add_test` 注册）+ 双后端（interpreter + verilator）可跑

- [ ] **AC9** 🔴 **同族计数器不得被污染**（分支 / 控制停顿 / 误预测）
  ⇒ `perf_branch_count` · `perf_control_stalls` · `perf_branch_mispredict` 在反压期间
  **逐拍不变**（与 `perf_instructions` 的重复计数**同族**：EX 级指令多拍驻留 ⇒
  `id_stage.is_branch` / `branch_taken` / `hazard.flush` 稳定为 1）
  ⇒ ⇒ 判据 = 反压起点选在「**EX 级确有分支**」的那一拍（否则气泡驻留 ⇒ 判据平凡成立）
  ⇒ ⇒ **变异自证**：去掉任一 `& !mem_stall` ⇒ 该用例 MUST 红（实测已做）

## Out of Scope

- **核的 `data_ready` 暴露到顶层**（34 端口面）：S3 接总线口时一起做
- **`perf_mem_stalls` 计数器**：本 change 选「显式分档」（见 What Changes 末段）
- **TCM 归属 / 成层**：S3 的决策（撞已签批 `rtl-hierarchical-output`）
- **load-use 停顿语义的任何变更**：`stall` 仍是「装气泡」，本 change 只新增 `freeze`
