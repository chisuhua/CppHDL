# 提案：RV32I 的 LB / LH / LBU / LHU MUST 有固件验证（load 字节道）

- **架构依据**：无（本 change 修一个**实现 bug** + **补验证** ⇒ 不触架构决策；
  修复用框架**已有**的 `sext` 原语，不引入新机制 ⇒ 不需要 ADR）
- **5 段草案**：`.rddf/improvements/rv32i-load-byte-lane-2026-10.md`
- **roadmap 归属**：`feat-rv32i-isa-completeness`（ISA 语义边界收口的最后一块）
- **规格影响**：**有** spec 变更（`rv32i-isa-semantics` 的 **1** 条 Requirement **ADDED**）
  ⇒ 归档时 **禁** `--skip-specs`
- **规模 / 风险**：**中**（⚠️ 含 **2 行实现修复** ⇒ 既有固件的 golden 仍应零变化）

## Why

### 实现基本已在，但 load 侧**从未被任何固件跑过**，且 `LB` 的符号扩展**有 bug**

实测 `include/cpu/pipeline/stages/mem_stage.h:99-166`：load 路径**已实现**
LB / LBU（字节道按 `addr[1:0]` 选 + 符号/零扩展）与 LH / LHU（半字道按 `addr[1]` 选
+ 符号/零扩展），`funct3` 分派齐全（0=LB / 1=LH / 2=LW / 4=LBU / 5=LHU）。

⚠️ **但 LB 的符号扩展实现是错的**（`mem_stage.h:119-121`）：

```cpp
ch_uint<32>(4278190080_d) | zext<32>(selected_byte),  // 0xFFFFFF00   ← 注释与代码不符
```

`4278190080 == 0xFF000000`，**不是**注释所写的 `0xFFFFFF00` ⇒ `lb` 返回
`0xFF0000C3`，应为 `0xFFFFFFC3`（同一模式的 LH 那行 `:140` 是**对的** ⇒ 孤例）。
⇒ **本 change 不是「零实现改动的验证 change」**，而是「**修 2 行实现 + 验证**」。

⚠️ 而 capability `rv32i-isa-semantics` 只覆盖字节道「**写**」
（`rv32i-store-byte-enable-2026-10` 落地的是 `DataTCM.strbe` + store 侧 lane 对齐）
⇒ **load 侧从未被任何固件跑过** ⇒ 若 lane 选择或符号扩展写错，**全仓照绿**
（典型的「**绿 ≠ 对**」）。

⚠️ **该缺陷为何从未暴露**：LB/LBU/LH/LHU 在仓内的**唯一**用例是
`test_riscv_tests*.cpp`，依赖**仓外** riscv-tests 二进制（`lb.bin`）
⇒ CI 中**全部 SKIP** ⇒ 有效覆盖为 **0**。

### Why now

- ① 落地后 load/store 的字节道机制**已成对**（同一份 `include/axi4/byte_lane.h` 语义）
  ⇒ 现在补 load 侧固件是**验证**而非**实现** ⇒ 风险低、收益直接；
- 本次顺带把一个**从未被文档化**的实现缺陷收口 ⇒ 它一直在生产路径上；
- 这是 `feat-rv32i-isa-completeness` 收口的**最后一块**（其余 = CSR / ECALL / FENCE
  ⇒ 独立大工程）。

## What Changes

### 实现：**2 行修复**（`mem_stage.h:119-121` + `:139-141`）

把 LB 与 LH 的**手写填充掩码**换成框架已有的 `sext<32>` 原语：

```cpp
ch_uint<32> lb_result = select(byte_sign, sext<32>(selected_byte), zext<32>(selected_byte));
ch_uint<32> lh_result = select(half_sign, sext<32>(selected_half), zext<32>(selected_half));
```

⚠️ `sext` 在 **interpreter / JIT / Verilog codegen 三后端都支持**
⇒ 不会造成双后端语义分叉；Verilog 侧发出 `{{24{<src>[7]}}, <src>}`。
⚠️ **LH 的改动是逐位等价的**（`sext<32>(x)` ≡ `x | (x[15] ? 0xFFFF0000 : 0)`
≡ 原式）⇒ **既有 golden 零变化**（已实测：只改 LB 与同时改 LB+LH 两次跑本固件，
12 项写回序列**逐项相同**）。
⇒ **LB 与 LH 一并改**是为了**消除「手写填充掩码」这一整类错误**
（只修 LB ⇒ LH 的同型掩码仍在原地 ⇒ 与「消除错误类别」的理由不自洽）。

（备选 = **只**把 LB 的常量改成 `0xFFFFFF00`；但那样缺陷形态**无回归锁**
⇒ 变异 M5 MUST 保留，且 MUST 写明「LH 为何例外」。）

### 测试：新增**判别性固件**（独立于既有 108 字固件 ⇒ 既有 golden **零变化**）

宿主 `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`（**既有 target**
⇒ 计数不变）：

1. 先用 **4 次 `SB`** 把 `0xD4`/`0xC3`/`0xB2`/`0xA1` 写进 `0x100..0x103`
   ⇒ `[0x100]` 的字 = `0xA1B2C3D4`
   （⚠️ 每字节都不同 ⇒ 任何 lane 选错都立刻暴露；全 0 / 全 F 的字**无判别力**；
   ⚠️ **4 个字节的 bit7 全为 1** ⇒ 符号扩展与零扩展**结果不同** ⇒ 对扩展方式有判别力；
   ⚠️ **不可用 `SW`** —— `SW` 的字节使能是 `0xF` 且数据不按 `addr[1:0]` 移位
   ⇒ 4 次 `SW` 会互相覆盖 ⇒ 造出的字有 3 个车道相同 ⇒ 判别力前提被破坏）；
2. `lb` / `lbu` 读 **4 个非零 offset**（`0x100..0x103`）⇒ 8 个期望值；
3. `lh` / `lhu` 读 **2 个非零 offset**（`0x100` / `0x102`）⇒ 4 个期望值；
4. ⚠️ **必须含非零 offset** —— offset 0 会**碰巧**正确（lane 0 = 基准 lane）⇒ 零判别力。

⇒ 合计 **12 项写回序列**（4 条 `ADDI` + 8 条 load；store 不写回）。

### 规格

`rv32i-isa-semantics` **ADDED ×1**：「load 字节道 MUST 按地址选车道并按 `funct3` 扩展」
+ **5** 个 Scenario（LB / LBU / LH / LHU / **可观测性**），每条带**反向期望**。
⚠️ 与既有「字节道**写**」Requirement **成对但不合并**（读的判据是「扩展方式」，
写的判据是「lane 掩码」）。
⚠️ 「每字节都不同 / 符号位为 1 / 造字用 `SB`」属**实施细节**（取值选取）
⇒ 写在该 change 的 `design.md` §2.1/§2.2，**不**写进 capability spec
（否则读起来像「产品要求每个道的值都不同」这种实现无法满足的条款）。

## Acceptance

### 功能性

- [ ] **实现修复**落地（`LB` 的符号扩展 = `sext<32>` 或等价的正确填充掩码）
- [ ] 判别性固件的 **12 项写回序列**全部**手工推导**（注释留痕）且带**反向期望**
- [ ] 双后端逐拍全等（**无掩码**，27 个顶层输出）
- [ ] 既有固件的 golden **零变化**（既有 golden 的变化 MUST 全部可归因 ⇒ 本次 = **零**）

### 可判别性（零信号断言不合格）

- [ ] **变异验证**（每条先确认**变异后仍编译成功** ⇒ 否则跑旧二进制 ⇒ 假绿）：
      M1 byte lane 恒取 lane0 / M2 符号扩展写成零扩展 / M3 half lane 取反 /
      M4 `funct3` 分派漏一项 / **M5 LB 填充掩码写成 `0xFF000000`** /
      **M6 LH 填充掩码写成 `0xFF000000`** / **M7 造字用 `SW` 代替 `SB`**
- [ ] 每个期望值带反向期望（`X_old != X_new`）：

  | 期望 | 反向期望（未修/写错时的值） |
  |---|---|
  | `lb  @0x101` = `0xFFFFFFC3` | lane 恒取 0 ⇒ `0xFFFFFFD4`；零扩展 ⇒ `0x000000C3`；填充掩码错 ⇒ `0xFF0000C3` |
  | `lbu @0x102` = `0x000000B2` | lane 恒取 0 ⇒ `0x000000D4` |
  | `lh  @0x102` = `0xFFFFA1B2` | lane 恒取 0 ⇒ `0xFFFFC3D4`；填充掩码错 ⇒ `0xFF00A1B2` |
  | `lhu @0x102` = `0x0000A1B2` | lane 恒取 0 ⇒ `0x0000C3D4` |

⚠️ **M2 与 M5 是不同缺陷形态**（扩展**方式**错 vs 扩展**正确但填错位置**）
⇒ **M2 抓不到 M5** ⇒ 两者 MUST 同时存在。

### 回归与工程

- [ ] `ctest -LE perf` 零回归（191/191）+ `ctest -L perf` 全绿
- [ ] `run_all_ported_tests.sh` 28/28 + `run_all_samples.sh` 35/35 + 护栏脚本全 exit 0
- [ ] 计数**不变**：`CTEST_TOTAL` **202** / `EXAMPLES_TEST_SOURCES` **17** /
      `ctest -L verilator_e2e` **37**（用例进**既有** target）

## Capabilities

- **`rv32i-isa-semantics`** —— **ADDED ×1**（load 字节道语义 + 5 个 Scenario）
- `dual-backend-test-coverage` / `rv32i-commit-path` / `mem-read-timing` —— **不涉及**

## Impact

- **计数**：三者均不变
- **文件面**：`include/cpu/pipeline/stages/mem_stage.h`（**2 行**）+
  `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` + 主 spec delta
  ⇒ **不动**既有固件
- **爆炸半径**：实现改动只触及 **LB / LH 的符号扩展分支**
  （LH 的改动**逐位等价** ⇒ 实际只有 LB 的行为变化）
  ⇒ 既有固件**零** LB/LH ⇒ 既有 golden 零变化（**已实测**：带修复跑既有 dual 用例
  ⇒ `10859 assertions in 5 test cases` 全绿）
- **可回退**：单 commit（实现改动 2 行 ⇒ 回退代价≈0）

## Non-Goals

- **不改** `mem_stage.h` 的**车道选择**逻辑（已正确 ⇒ 只验证）
- **不改** LB/LH 之外的 load 语义（LW / LBU / LHU 已正确）
- 不覆盖 misaligned load（RV32I 允许但本仓**未实现** ⇒ 独立立项）
- 不覆盖 CSR / ECALL / FENCE / 特权指令
- 不新增顶层观测端口（ADR-046 D10 冻结）
