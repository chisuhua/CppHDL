# 提案：RV32I 的 LB / LH / LBU / LHU MUST 有固件验证（load 字节道）

- **架构依据**：无（本 change **不改实现** ⇒ 不触架构决策；① 的 `rv32i-isa-semantics`
  是**已成**的契约，本 change 只在其上**追加** load 侧的成对条款）
- **5 段草案**：`.rddf/improvements/rv32i-load-byte-lane-2026-10.md`
- **roadmap 归属**：`feat-rv32i-isa-completeness`（ISA 语义边界收口的最后一块）
- **规格影响**：**有** spec 变更（`rv32i-isa-semantics` 的 **1** 条 Requirement **ADDED**）
  ⇒ 归档时 **禁** `--skip-specs`
- **规模 / 风险**：**低**（无实现改动）⇒ 但**判别力风险**高（期望值若从仿真输出抄 ⇒ 零信号）

## Why

### 实现已在，但**零固件验证**

实测 `include/cpu/pipeline/stages/mem_stage.h:82-163`：load 路径**已实现**
LB / LBU（字节道按 `addr[1:0]` 选 + 符号/零扩展）与 LH / LHU（半字道按 `addr[1]` 选
+ 符号/零扩展），`funct3` 分派齐全（0=LB / 1=LH / 2=LW / 4=LBU / 5=LHU）。

⚠️ 而 capability `rv32i-isa-semantics` 只覆盖字节道「**写**」
（① `rv32i-store-byte-enable-2026-10` 落地的是 `DataTCM.strbe` + store 侧 lane 对齐）
⇒ **load 侧从未被任何固件跑过** ⇒ 若 lane 选择或符号扩展写错，**全仓照绿**
（典型的「**绿 ≠ 对**」）。

### Why now

- ① 落地后 load/store 的字节道机制**已成对**（同一份 `include/axi4/byte_lane.h` 语义）
  ⇒ 现在补 load 侧固件是**验证**而非**实现** ⇒ 风险低、收益直接；
- 这是 `feat-rv32i-isa-completeness` 收口的**最后一块**（其余 = CSR / ECALL / FENCE
  ⇒ 独立大工程）。

## What Changes

### 实现：**零改动**（`mem_stage.h` 不动）

### 测试：新增**判别性固件**（独立于既有 108 字固件 ⇒ 既有 golden **零变化**）

宿主 `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`（**既有 target**
⇒ 计数不变）：

1. 先 `sw` 一个**每字节都不同**的字 `0xA1B2C3D4` 到 `0x100`
   （⚠️ 每字节不同 ⇒ 任何 lane 选错都立刻暴露；全 0 / 全 F 的字**无判别力**）；
2. `lb` / `lbu` 读 **4 个非零 offset**（`0x100..0x103`）⇒ 8 个期望值；
3. `lh` / `lhu` 读 **2 个非零 offset**（`0x100` / `0x102`）⇒ 4 个期望值；
4. ⚠️ **必须含非零 offset** —— offset 0 会**碰巧**正确（lane 0 = 基准 lane）⇒ 零判别力。

### 规格

`rv32i-isa-semantics` **ADDED ×1**：「load 字节道 MUST 按地址选车道并按 `funct3` 扩展」
+ 4 个 Scenario（LB / LBU / LH / LHU），每条带**反向期望**。
⚠️ 与既有「字节道**写**」Requirement **成对但不合并**（读的判据是「扩展方式」，
写的判据是「lane 掩码」）。

## Acceptance

### 功能性

- [ ] 判别性固件的 12 个期望值全部**手工推导**（注释留痕）且带**反向期望**
- [ ] 双后端逐拍全等（**无掩码**，27 个顶层输出）
- [ ] 既有固件的 golden **零变化**（既有 golden 的变化 MUST 全部可归因 ⇒ 本次 = **零**）

### 可判别性（零信号断言不合格）

- [ ] **变异验证**（每条先确认**变异后仍编译成功**）：
      M1 lane 选择恒取 lane 0 / M2 符号扩展写成零扩展 / M3 半字 lane 选择取反 /
      M4 `funct3` 分派漏一项
- [ ] 每个期望值带反向期望（`X_old != X_new`）：

  | 期望 | 反向期望（未修/写错时的值） |
  |---|---|
  | `lb  @0x101` = `0xFFFFFFC3` | lane 恒取 0 ⇒ `0xFFFFFFD4`；零扩展 ⇒ `0x000000C3` |
  | `lbu @0x101` = `0x000000C3` | lane 恒取 0 ⇒ `0x000000D4` |
  | `lh  @0x102` = `0xFFFFA1B2` | lane 恒取 0 ⇒ `0xFFFFC3D4` |
  | `lhu @0x102` = `0x0000A1B2` | lane 恒取 0 ⇒ `0x0000C3D4` |

### 回归与工程

- [ ] `ctest -LE perf` 零回归（191/191）+ `ctest -L perf` 全绿
- [ ] `run_all_ported_tests.sh` 28/28 + `run_all_samples.sh` 35/35 + 护栏脚本全 exit 0
- [ ] 计数**不变**：`CTEST_TOTAL` **202** / `EXAMPLES_TEST_SOURCES` **17** /
      `ctest -L verilator_e2e` **37**（用例进**既有** target）

## Capabilities

- **`rv32i-isa-semantics`** —— **ADDED ×1**（load 字节道语义 + 4 个 Scenario）
- `dual-backend-test-coverage` / `rv32i-commit-path` / `mem-read-timing` —— **不涉及**

## Impact

- **计数**：三者均不变
- **文件面**：`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` + 主 spec delta
  ⇒ **不动** `mem_stage.h`、不动既有固件
- **爆炸半径**：只加固件与断言 ⇒ 对既有行为**零**影响
- **可回退**：单 commit（无实现改动 ⇒ 回退代价≈0）

## Non-Goals

- **不改** `mem_stage.h` 的 load 路径（实现已正确 ⇒ 本 change 只**验证**）
- 不覆盖 misaligned load（RV32I 允许但本仓**未实现** ⇒ 独立立项）
- 不覆盖 CSR / ECALL / FENCE / 特权指令
- 不新增顶层观测端口（ADR-046 D10 冻结）
