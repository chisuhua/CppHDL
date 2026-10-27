---
name: rv32i-load-byte-lane-2026-10
priority: P0
source: roadmap 审计（2026-10-27，ISA 语义边界收口的最后一块）
phase: phase-5
category: cpu
type: feature
created_at: 2026-10-27
---

# Improvement: RV32I 的 LB / LH / LBU / LHU MUST 有固件验证（load 字节道）

- **Date drafted**: 2026-10-27
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，归属 feature `feat-rv32i-isa-completeness`（ISA 语义边界收口）
- **来源**: ④（取指 skid buffer）收口后的 roadmap 盘点 —— `feat-rv32i-isa-completeness` 的
  验收项「SB/SH/LB/LH/LBU/LHU 的字节/半字语义在双后端一致且符合 ISA（含非零 offset）」
  仍未勾选

## 1. Why (背景与动机)

**实现已在，但零固件验证**（实测，`include/cpu/pipeline/stages/mem_stage.h:82-163`）：
load 路径**已实现** LB / LBU（字节道按 `addr[1:0]` 选 + 符号/零扩展）与
LH / LHU（半字道按 `addr[1]` 选 + 符号/零扩展），`funct3` 分派齐全
（0=LB / 1=LH / 2=LW / 4=LBU / 5=LHU）。

⚠️ 但 **capability `rv32i-isa-semantics` 只覆盖字节道「写」**（① `rv32i-store-byte-enable-2026-10`
落地的是 `DataTCM.strbe` + store 侧 lane 对齐）⇒ **load 侧从未被任何固件跑过**
⇒ 若 lane 选择或符号扩展写错，**全仓照绿**（典型的「绿 ≠ 对」）。

**Why now**：① 落地后 load/store 的字节道机制**已成对**（同一份
`include/axi4/byte_lane.h` 语义）⇒ 现在补 load 侧的固件是**验证**而非**实现**；
且这是 `feat-rv32i-isa-completeness` 收口的**最后一块**（其余两块 = CSR/ECALL/FENCE，
属独立大工程）。

## 2. What Changes (变更范围)

- **实现零改动**（`mem_stage.h` 不动）—— 本 change 是**验证** change；
- 新增**判别性固件**（独立于既有 108 字固件 ⇒ **既有 golden 零变化**）
  ⇒ 宿主 `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`（**既有 target**）：
  - 先 `sw` 一个**每字节都不同**的字（`0xA1B2C3D4`）到 `0x100`
    （⚠️ 每字节不同 ⇒ 任何 lane 选错都立刻暴露；全 0/全 F 的字**无判别力**）；
  - `lb` / `lbu` 读 **4 个非零 offset**（`0x100..0x103`）⇒ 8 个期望值；
  - `lh` / `lhu` 读 **2 个非零 offset**（`0x100` / `0x102`）⇒ 4 个期望值；
  - ⚠️ **必须含非零 offset**：offset 0 会**碰巧**正确（lane 0 = 基准 lane）⇒ 零判别力；
- **spec**：`rv32i-isa-semantics` **ADDED** 一条 Requirement
  「load 字节道 MUST 按地址选车道并按 `funct3` 扩展」+ 4 个 Scenario
  （LB / LBU / LH / LHU），每条带**反向期望**。

## 3. Acceptance (验收标准)

- [ ] 判别性固件的 12 个期望值全部**手工推导**（注释留痕）且带**反向期望**（`X_old != X_new`）
- [ ] 变异验证（每条先确认**变异后仍编译成功**）：M1 lane 选择恒取 lane 0 /
      M2 符号扩展写成零扩展 / M3 半字 lane 选择取反 / M4 `funct3` 分派漏一项
- [ ] 双后端逐拍全等（**无掩码**，27 个顶层输出）
- [ ] 既有固件的 golden **零变化**（既有 golden 的变化 MUST 全部可归因 ⇒ 本次归因 = **零**）
- [ ] `ctest -LE perf` 零回归 + 计数**不变**（`CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` /
      `ctest -L verilator_e2e`）

## 4. Capabilities (涉及的能力维度)

- `rv32i-isa-semantics` —— **ADDED ×1**（load 字节道语义；与既有「字节道写」Requirement
  **成对**，但**不合并**（写与读的判据不同：写有 lane 掩码，读有扩展方式））
- `dual-backend-test-coverage` / `rv32i-commit-path` —— **不涉及**

## 5. Impact (影响面分析)

- **改动文件**：`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`（+ 固件 + 用例）、
  主 spec 的 delta
- **计数**：**三者均不变**（用例进**既有** target）
- **爆炸半径**：只加固件与断言 ⇒ 对既有行为**零**影响（既有固件不动）
- **风险**：**低**（无实现改动）—— 但**判别力风险**高（期望值若从仿真输出抄 ⇒ 零信号）
  ⇒ MUST 手推 + 反向期望 + 变异验证
