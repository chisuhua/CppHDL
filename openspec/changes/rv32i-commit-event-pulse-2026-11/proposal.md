# Change: 提交事件端口脉冲化（`obs_data_write_en` / `dbg_wb_write_en`）

> **Change 名**：`rv32i-commit-event-pulse-2026-11`
> **母 improvement**：`rv32i-soc-platform-2026-11`（**S4a 的硬前置** · 状态 `in-progress`）
> **决策依据**：
> - **G-9 已裁决**（2026-11-24 · 用户）⇒ ⇒ S4a 的核内 pre-cache 分流会把
>   **MMIO 段**接进数据通路 ⇒ ⇒ MMIO 事务**必然多拍** ⇒ ⇒ `mem_stall` 必然出现
>   ⇒ ⇒ `.rddf/checks/decision-request-g9-core-port-face-v2-2026-11.md`
> - **已签批 spec `rv32i-commit-path`** ⇒ 「该指令的**写回** MUST 只发生**一次**」
>   + 「观测面 MUST 让『指令退休』可被外部验证」
>   ⇒ ⇒ 本 change 是**实现既有 Requirement**（不是新增能力）
> **日期**：2026-11-24 · builder
> **性质**：**观测面语义订正**（端口名 / 方向 / 位宽**一律不变** ⇒ **核端口面零变更**）
> ⇒ ⇒ **零公开面新增**（语义变更 ⇒ 登记 `public-api-surface.md` 的「语义订正」节）
> ⇒ ⇒ **零 spec delta 的 ADDED**（只 MODIFIED 既有 Requirement 的**观测面**条款）

## Why

1. 🔴 **两根事件端口是「电平」⇒ 停顿期间持续 ⇒ 任何按「计数」消费它们的观测面被灌水**：
   - `data_write_en` = `exmem_is_store`（`include/cpu/pipeline/rv32i_pipeline.h:693`），
     而 `exmem_is_store` 在 `mem_stall` 时**冻结保持**（`:568`）
     ⇒ ⇒ **停顿 N 拍 ⇒ 该端口高 N 拍**；
   - `dbg_wb_write_en` = `wb_stage.io().write_en`（`rv32i_pipeline.h:891`），
     而 `write_en = needs_writeback & rd_not_zero & mem_valid`
     （`include/cpu/pipeline/stages/wb_stage.h:110`）⇒ ⇒ `mem_valid` 来自 MEM/WB 寄存器，
     该寄存器在 `mem_stall` 时**冻结**（`rv32i_pipeline.h:637-652`）
     ⇒ ⇒ **停顿 N 拍 ⇒ 该端口高 N 拍**。
   ⇒ ⇒ **「事件次数」被污染成「事件时长之和」** ⇒ ⇒ **手推期望值不可得**
   ⇒ ⇒ 直接违反本仓纪律「**期望值不得依赖实现时序**」。

2. 🔴 **这在 S4a 之前是「潜伏」的**：核级/SoC 级现有程序**只有 `sw`（无 `lw`）**
   ⇒ ⇒ `sw` 走 D-L1 的 **write-through 单拍**路径 ⇒ ⇒ 无 fill ⇒ ⇒ **无 `mem_stall`**
   ⇒ ⇒ 今日实测 `store_count == 1` / `writeback_count == 3` 仍绿。
   ⇒ ⇒ 但 **S4a 的 SoC 级程序必然含 MMIO 访问** ⇒ ⇒ MMIO 事务**必然多拍**
   ⇒ ⇒ **`mem_stall` 必然出现** ⇒ ⇒ 灌水必然发生 ⇒ ⇒ **AC 的手推终值当场失效**。

3. 🔴 **仓内已有「已知违规」登记，且现有 workaround 掩盖了它**：
   - `openspec/specs/rv32i-commit-path/spec.md:96-99` 的 NOTE 明写
     「固件的第一条指令被写回两次（连续两拍 `wb_write_en=1`）」⇒ ⇒ **同一形态**；
   - `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:958-974` 的注释明写
     「`wb_write_en` 是**电平**，同一条指令会在 WB **停留多拍** ⇒ ⇒ 逐拍收集会重复计数
     ⇒ ⇒ 折掉**连续相同** `(rd, data)`」⇒ ⇒ 且**自陈**
     「该折算会**掩盖**『同一条指令连续两拍写回**同一个** (rd,data)』这一形态」
     ⇒ ⇒ **已登记的判别力缺口**。

4. ✅ **核内已有正确口径可对齐**：`commit_count` 的推进条件是
   `commit_valid = mem_stage.io().mem_valid_out & !mem_stall`
   （`rv32i_pipeline.h:833-834`）⇒ ⇒ **退休计数已经正确排除了停顿**
   ⇒ ⇒ 两根事件端口与 `commit_count` **口径不一致** ⇒ ⇒ 这是**一致性缺陷**，不是新需求。

5. ⇒ ⇒ **结论**：S4a 之前 MUST 先把这两根**事件**端口改成与 `commit_count` 同口径的**事件脉冲**。

## What Changes

| 面 | 内容 |
|---|---|
| **库层** `include/cpu/pipeline/rv32i_pipeline.h` | ① `io().data_write_en <<= exmem_is_store & !mem_stall;`<br>② `io().dbg_wb_write_en <<= wb_stage.io().write_en & !mem_stall;`<br>⇒ ⇒ **两根端口都只作输出**（无内部消费者）⇒ ⇒ **零行为影响**（除观测面） |
| **核 IP 顶层** `examples/riscv-mini/src/rv32i_core_top.h` | **零改动**（只是转接 pipeline 的同名输出 ⇒ 端口名 / 方向 / 位宽全部不变 ⇒ **核端口面仍是 34**） |
| **判据**（新增）`examples/riscv-mini/tests/test_commit_event_pulse.cpp` | 新建 Catch2 target：构造一个**含 `lw`（冷 D-TCM ⇒ D-L1 miss ⇒ MEM 级多拍冻结）**的程序 ⇒ ⇒ 断言 ① 停顿期间两根事件端口**逐拍为 0** ② **事件次数 == 指令条数**（手推）③ 停顿期间 `commit_count` / `perf_instructions` **同样不增长**（一致性锚点） |
| **判据**（加强）`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | 保留既有的「折连续相同」workaround（脉冲化后它**变成幂等** ⇒ golden **逐项不变**）⇒ ⇒ **追加**一条断言：**不折**的原始逐拍序列里**不存在相邻重复项** ⇒ ⇒ 补上原 workaround 掩盖的形态 |
| **登记面** | `.rddf/checks/public-api-surface.md`（**语义订正**节）· `docs/CHANGELOG.md`（一行）· `include/cpu/AGENTS.md`（流水线观测面约定补一节）· `examples/riscv-mini/AGENTS.md`（核端口契约表的**语义**脚注 —— 表本身不动） |

## spec delta（**MODIFIED** · 不新增 capability）

- **MODIFIED** `rv32i-commit-path` 的「**RV32I SoC 顶层 MUST 有可观测的端口契约**」Requirement
  ⇒ ⇒ **既有 3 个 Scenario 逐字保留** ⇒ ⇒ 只**追加** 2 个 Scenario：
  1. 「**事件端口** MUST 是**单拍脉冲**（与事件时长无关）」；
  2. 「**事件端口** MUST 与 `commit_count` **同口径**（停顿期间两者 MUST 同时不增长）」。
- ⚠️ **不改** `rv32i-memory-timing`（它约束 MEM/WB 的**冻结**语义 —— 本 change **正是**依赖它）
- ⚠️ **不改** `cpu-cache-subsystem`（不改 cache 的任何行为）
- ⚠️ **不改** `rv32i-data-forwarding`（不碰前推）
- ⚠️ **不改** `example-component-boundary` / `rtl-soc-output` / `rtl-hierarchical-output`
  （产物端口面**零变更** ⇒ 那三个 capability 的条款**自动成立**）

## 验收标准

> 期望值 MUST **手推**（禁抄仿真输出）· 事件次数 MUST 由**程序结构**推出。
> ⚠️ **核级现有 24 条判据 + `.sv` TB + golden 写回序列** MUST **逐项不变**（本 change 是**观测面**改，不是**行为**改）。

- [ ] **AC1** 🔴 **停顿期间事件端口逐拍为 0**：构造程序 `addi x1,x0,42` · `lw x2,0(x0)`
  （冷 D-TCM ⇒ D-L1 miss ⇒ MEM 级被冻结多拍）· `addi x3,x0,1` · `jal x0,0`
  ⇒ ⇒ 在 `lw` 处于 MEM 级且 `data_ready=0` 的那些拍 ⇒ ⇒ `dbg_wb_write_en` MUST **逐拍为 0**
  ⇒ ⇒ ⚠️ **旧实现（电平）在这些拍恒为 1** ⇒ ⇒ **必红**
  ⇒ ⇒ **判别力自证**：变异 M1 = 去掉 `& !mem_stall` ⇒ ⇒ 该断言转红（实测记录进期望台账）
- [ ] **AC2** 🔴 **事件次数 == 指令条数（手推）**：AC1 的程序 ⇒ ⇒ **写回事件 = 3**
  （x1 / x2 / x3；`jal x0,0` 的 `rd=x0` **不写回**）· **store 事件 = 0**
  ⇒ ⇒ ⚠️ **旧实现（电平）的写回事件 = 3 + 停顿拍数** ⇒ ⇒ **必红**
  ⇒ ⇒ ⇒ 这是「事件次数」与「事件时长之和」的**分离判据**
- [ ] **AC3** 🔴 **与 `commit_count` 同口径**：AC1 程序的停顿期间 ⇒ ⇒
  `obs_perf_instructions` MUST **逐拍不增长** ⇒ ⇒ 与两根事件端口**同时**为 0
  ⇒ ⇒ ⇒ 证明三者是**同一个口径**（不是两个各自为政的判定）
- [ ] **AC4** 🔴 **核级既有判据零变化**：`examples/riscv-mini/tb_rv32i_soc` 的
  `RESULT: checks=24 fails=0` **不变** · `tb_rv32i_soc.sv` 的 24 条**不变**
  ⇒ ⇒ `test_rv32i_soc_dual_parity.cpp` 的**写回序列 golden 逐项不变**
  ⇒ ⇒ 「折连续相同」workaround 保留（脉冲化后**幂等**）⇒ ⇒ golden **零改动**
- [ ] **AC5** 🔴 **判别力缺口被补上**：`test_rv32i_soc_dual_parity.cpp` **追加**一条断言 ——
  **不折**的原始逐拍写回序列里**不存在相邻重复项**
  ⇒ ⇒ ⚠️ **旧实现下必然存在**（同一条指令在 WB 停留多拍）⇒ ⇒ **必红**
  ⇒ ⇒ ⇒ 该断言**正面覆盖**了 `test_rv32i_soc_dual_parity.cpp:962-964` 自陈的「掩盖」形态
- [ ] **AC6** 🔴 **零回归**：`ctest` 全绿（**275**）· `run_all_ported_tests` PASS
  ⇒ ⇒ **全部 `scripts/verify_*.sh` 全绿**（含 C6 快照 —— 端口面**零变更** ⇒ C6 **不动**）

## Out of Scope（本 change 明确不做）

- **`reg_file.write` 的「重复写入」** ⇒ ⇒ 停顿期间 WB 级仍会**每拍**写同一 `(rd, data)`
  （**幂等** ⇒ 寄存器堆内容无可观测差异）⇒ ⇒ **本 change 不动它**
  ⇒ ⇒ 如实登记为**残留**（见 design §4）
- **`dbg_exmem_is_store` / `dbg_id_is_store`** ⇒ ⇒ 它们是**级属性**（EX/MEM / ID 寄存器的内容）
  ⇒ ⇒ **电平是正确语义** ⇒ ⇒ **不改**（见 design §2 的「事件端口 vs 级属性端口」判据）
- **MMIO 通道 / pre-cache 分流 / SoC 装配** ⇒ ⇒ **S4a**
- **`irq` 口** ⇒ ⇒ **S5**
- **D-TCM 窗口收紧**（今日越界访问静默别名）⇒ ⇒ **S4a 的 Out of Scope**（同批登记）
