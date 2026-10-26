---
name: rv32i-jump-sra-2026-10
priority: P0
source: 2026-10-26 roadmap 审计（feat-rv32i-isa-completeness ②）
phase: phase-5
category: cpu
type: bugfix
created_at: 2026-10-26
---

# Improvement: RV32I 的 `JAL`/`JALR` MUST 重定向 PC 并写回返回地址；`SRA` MUST 是算术右移

- **Date drafted**: 2026-10-26
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft；change `rv32i-jump-sra-2026-10` 实施中
- **来源**: 2026-10-26 的 roadmap 对齐审计（实测挖出）—— 此前只登记在根
  `AGENTS.md §KNOWN LIMITATIONS` 的「RV32I 的 ISA 覆盖边界」条目里。

## 1. Why (背景与动机)

**两个缺口**（互不耦合，但都在「RV32I 语义不完整」这一类里）：

### 缺口 A：`JAL` / `JALR` 完全不跳，写回值也不是 `pc + 4`

事实链（逐条可核）：

1. `include/cpu/pipeline/rv32i_decoder.h:46-47,135,145` **已**解码 `OP_JAL` /
   `OP_JALR`（`imm_j` / `imm_i`）⇒ **缺口不在 decoder**；
2. `include/cpu/pipeline/stages/id_stage.h:201,265` 也**已**产出 `idex_is_jump`
   （`is_jal | is_jalr`）并一路传到 WB（`rv32i_pipeline.h:318,344,359`）⇒
   **「这是一条跳转指令」这个信息是有的**；
3. ⚠️ 但 `include/cpu/pipeline/stages/ex_stage.h:214`
   `branch_taken_sig = select(io().is_branch, branch_cond, false)` —— `is_branch`
   **只在 `OP_BRANCH` 时为真** ⇒ `ex_stage.io().branch_taken` 对 `JAL`/`JALR`
   **恒为 false**；
4. `rv32i_pipeline.h:180` `if_stage.io().branch_valid <<= ex_stage.io().branch_taken;`
   ⇒ **PC 永不重定向** ⇒ **任何子程序调用都坏**（执行流直接越过 `JAL` 继续往下）；
5. ⚠️ 写回值也错：`wb_stage.h:81`
   `wb_data = select(io().is_alu | io().is_jump, io().alu_result, wb_data)`
   ⇒ `JAL`/`JALR` 写回的是 **ALU 的垃圾结果**（`rs1 + imm`），**不是 `pc + 4`**
   ⇒ `ra`（x1）/ `x5` 拿到的不是返回地址；
6. ⚠️ 即便补上 (3)，`JALR` 的**目标地址算法也不对**：
   `ex_stage.h:178` `branch_target_calc = io().pc + io().imm` ——
   `JALR` 应是 `(rs1 + imm) & ~1`，不是 `pc + imm`。

⇒ 三处**缺一不可**（重定向信号 / 目标地址算法 / 写回值）。

### 缺口 B：`SRA` 退化为 `SRL`

- `include/cpu/pipeline/stages/ex_stage.h:134` `ch_uint<32> sra_result = srl_result;`
  —— **pipeline 实际用的**内联 ALU；
- ⚠️ **还有第二处**：`include/cpu/pipeline/rv32i_alu.h:103`
  `auto sra_result = srl_result;  // TODO: 完善算术右移` —— 独立的 `Rv32iAlu` 组件
  （消费者：`examples/riscv-mini/rv32i_phase1_test.cpp` /
  `rv32i_phase2_test.cpp` / `examples/riscv-mini/tests/test_pipeline_exec.cpp`）
  ⇒ **两处都退化** ⇒ 只修一处会留下「组件层对、流水线层错」的分裂。

⇒ 算术右移对**有符号负数**算错（`-8 >> 1` 得 `-4` 之外的错值）。

### 为什么现在

① `rv32i-store-byte-enable-2026-10`（字节道写）**已归档** ⇒ capability
`rv32i-isa-semantics` **已存在** ⇒ 本 change 的 delta **追加**到同一 capability
（不必新建）；② C3 的**双后端逐拍 parity** + **32 端口观测面**已就位 ⇒
控制流改动可被逐拍对照；③ 现有固件**不含** `JAL`/`JALR`/`SRA`
（实测：`test_rv32i_soc_dual_parity.cpp` 的固件里零出现）⇒
**既有 golden 不会因「修好」而变**（只会因「新增用例」而变）⇒ 回归面被既有断言钉住。

## 2. What Changes (变更范围)

### A. `JAL`/`JALR` 重定向（3 处缺一不可）

1. **`include/cpu/pipeline/stages/ex_stage.h`**
   - `branch_target` 的语义从「条件分支目标」扩展为「**重定向目标**」：
     `JAL` = `pc + imm_j`；`JALR` = `(rs1 + imm_i) & ~1`；条件分支保持 `pc + imm_b`
   - `branch_taken` 对 `JAL`/`JALR` **恒 true**（无条件跳）
   - **新增输出端口** `link_value`（= `pc + 4`，返回地址）
   - ⚠️ `JALR` 的 `rs1` 取 `io().rs1_data` ⇒ **无数据前推**（ADR-046 **D5** 悬置）
     ⇒ `JALR` 的 `rs1` 若由前一条指令写入 ⇒ 读到**旧值** ⇒ **已知限制**（如实登记）
2. **`include/cpu/pipeline/stages/wb_stage.h`**
   - **新增输入端口** `link_value`
   - 写回选择改为**三级**：`load` ⇒ `mem_data`；`jump` ⇒ `link_value`；
     `alu` ⇒ `alu_result`（原先 `is_alu | is_jump` 合并 ⇒ `jump` 拿到 ALU 结果）
   - ⚠️ **新增输入端口 ⇒ 全部实例化点 MUST 显式驱动**（实测：未驱动的输入端口
     读到的是**全 1** ⇒ 写回 `0xFFFFFFFF`）⇒ `examples/riscv-mini/tests/test_pipeline_debug.cpp`
     的 `WbStageTop` MUST 显式驱动
3. **`include/cpu/pipeline/rv32i_pipeline.h`**
   - 新增 2 个流水线寄存器 `exmem_link_value` / `memwb_link_value` + 接线
   - `hazard.io().ex_branch <<= id_stage.io().is_branch | id_stage.io().is_jump;`
     ⇒ `JAL`/`JALR` 也 **flush 错误路径**（否则 PC 跳了、错误路径的指令却继续执行）
   - ⚠️ `if_stage` 的 PC 选择器**不新增第三路** —— **复用**现有
     `branch_valid` / `branch_target` 两路 ⇒ `if_stage.h:88-96` 记录的隐式不变量
     （「flush 的唯一来源是 `ex_branch & ex_branch_taken`，且 `branch_valid` 接的
     正是同一个 `ex_stage.branch_taken`」）**结构不变**；
     但「`ex_branch` 的成分」从「仅条件分支」扩展为「条件分支 + 跳转」
     ⇒ MUST **更新该不变量的文字**（plan 要求的「flush 与 jump 的互斥论证」）

### B. `SRA` 真算术右移（**两处**）

- `ex_stage.h`（内联 ALU，pipeline 实际用）+ `rv32i_alu.h`（`Rv32iAlu` 组件）
- 算法：`sra = (a >> sh) | (sign ? (0xFFFFFFFF << (32 - sh)) : 0)`，`sh == 0` 时**不填充**
- ⚠️ 框架层**没有**算术右移原语：`include/core/operators_shift.h` 里
  `operator>>>`（`sshr_op`）是**被注释掉**的 ⇒ MUST 在 ALU 层用
  「逻辑右移 + 符号位填充」实现
- ⚠️ `ch_uint` 无符号 ⇒ MUST **显式**取符号位（MUST NOT 直接 `>>`）

### C. 测试（`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`）

- 固件**尾部**追加（既有 word 索引一个都不移动）：
  `JAL` 跳到子程序 → 子程序写寄存器 → `JALR`/`JAL` 返回 → 主程序读回；
  `SRA` 用例（`-8 >> 1`）
- golden 重算：写回序列 / 停顿拍数 / 退休数 / `taken` 分支冲刷位置
- 变异验证：SRA 改回 SRL ⇒ 转红；`JAL` 目标接成 `pc + 4` ⇒ 转红；
  `link_value` 接成 `pc + imm` ⇒ 转红

## 3. Acceptance (验收标准)

- [ ] `JAL` **真的跳**到子程序且能**返回**（子程序写的寄存器值被主程序读回）
- [ ] `ra`（x1）== 返回地址（`JAL` 指令的 `pc + 4`）
- [ ] `JALR` 跳到 `(rs1 + imm) & ~1`
- [ ] `SRA` 对负数正确（`-8 >> 1 == -4`，即 `0xFFFFFFF8 → 0xFFFFFFFC`）
- [ ] **两处** `SRA` 实现（`ex_stage` 内联 + `Rv32iAlu` 组件）都正确
- [ ] 既有固件的 golden 除「新增用例导致的变化」外**不变**
- [ ] 双后端（interpreter + verilator）**逐拍全等**（无掩码）
- [ ] 变异验证 ≥ 3 条**全被抓**
- [ ] `wb_stage` 的全部实例化点显式驱动 `link_value`
- [ ] `ctest -LE perf` 零回归

## 4. Capabilities (涉及的能力维度)

- **`rv32i-isa-semantics`（已存在，本 change 追加 2 条 Requirement）**
  —— 「跳转 MUST 重定向 PC 并写回返回地址」+「`SRA` MUST 是算术右移」
- `rv32i-commit-path`（SoC 观测面契约**未变**：32 端口不变 ⇒ **无需**改 ADR-046 D10）
- `rv32i-canonical-home`（`include/cpu/` 仍是微架构的唯一权威 ⇒ 本 change 不碰 example 副本）

## 5. Impact (影响面分析)

- **改动文件**：`include/cpu/pipeline/stages/ex_stage.h`、
  `include/cpu/pipeline/stages/wb_stage.h`、`include/cpu/pipeline/rv32i_pipeline.h`、
  `include/cpu/pipeline/rv32i_alu.h`、
  `examples/riscv-mini/tests/test_pipeline_debug.cpp`（新端口驱动）、
  `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`（固件 + golden）
- **API 变更**：`wb_stage` **新增 1 个输入端口** ⇒ 公共 API 面变化 ⇒
  1 个实例化点（`WbStageTop`）MUST 显式驱动；
  `ex_stage` **新增 1 个输出端口** ⇒ 由组件自己驱动 ⇒ 无需外部接线
- **风险**：**中-高** —— 改的是**控制流时序**（PC 重定向 + 冲刷 + 写回）
  - 缓解 1：`JAL`/`JALR` **复用**已有的条件分支通路（BEQ 已验证）⇒ 时序结构不变
  - 缓解 2：既有固件**不含**跳转指令 ⇒ 既有 golden 不会被「修好」而改变
  - 缓解 3：双后端逐拍 parity + 变异验证
- **偏离 plan 的建议（如实记录）**：plan 写「写回通路缺失 ⇒ 体积显著上升 ⇒
  **宁可拆成两个 change**」。本 change **不拆**，理由：写回接口扩展
  （`link_value` 通路）**只服务 `JAL`/`JALR`**，`SRA` 与之**零耦合** ⇒
  拆分会把「两次 golden 重算 + 两次审稿 + 两次归档」的开销翻倍，
  而收益仅是「归因」⇒ **归因改由「分离的断言 + 分离的变异」保证**
  （`SRA` 的断言只看 `SRA` 用例，`JAL` 的断言只看跳转用例 ⇒ 互不掩盖）
- **未做（明确非目标）**：
  - 不接**数据前推**（ADR-046 **D5** 悬置 ⇒ `JALR` 的 `rs1` 无前推 ⇒ 如实登记）
  - 不引入**分支预测**（`rv32i_branch_predict.h` 未接入流水线）
  - 不改 ADR-046 **D10** 的 SoC 32 端口契约（本 change 不新增顶层端口）
  - 不改框架层 `ch_mem` / 算术右移原语（`operator>>>` 仍是注释状态）
