# 提案：路线 C 第 2 批 —— RV32I 的「时序侧」组件（`IdStage` / `Rv32iRegFile` / `DataTCM`）进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 批次表 + §4 的「可转性 4 问」
  + `.rddf/checks/handoff-2026-11-02-parity-route-ab.md` §4.7（路线 C 的只读盘点）
- **规格影响**：**无 spec 变更** ⇒ 归档可用 `--skip-specs`
- **规模**：1 个新测试文件（3 个 `DUAL_BACKEND_TEST_PAIR` / 232 断言每后端）+ 1 条 CMake 注册
  + 1 条 include 路径补丁 + 计数锚点 + **1 个新缺陷的独立立项**
- **关联**：objective `objective-test-coverage-100pct` · capability `dual-backend-test-coverage`
  · ADR-046（RV32I 单源化）· ADR-049（CLINT 计时器）· ADR-041（cache Sync/Async）

## Why

### ① 与路线 C 第 1 批**互补**：第 1 批是**纯组合**组件，本批是**含时序状态**的组件

| 组件 | 时序状态 | 为什么两后端最容易在这里分叉 |
|---|---|---|
| `riscv::IdStage` | **16** 个 `ch_reg`（ID/EX 流水线寄存器） | ⇒ 走 `always_ff` codegen 路径 ⇒ 触发 Verilator 的**时钟 0→1→0 翻转**与**复位脉冲**机制（ADR-035 的全部差异点） |
| `riscv::Rv32iRegFile` | **32** 个 `ch_reg`（x0..x31） | ⇒ **仓内最大的 `always_ff` 面** ⇒ ⇒ 32 级 `select` 读 mux 的生成代码规模也最大 |
| `riscv::DataTCM<8,32>` | `ch_mem`（**`aread` 0-cycle**）+ **读-改-写**字节合并 | ⇒ 「读-改-写的**同拍**语义」= 组合读拿到**边沿前**的值 ⇒ ⇒ Verilog 侧 `always_ff` 与 `assign` 的时序关系**最容易**与 interpreter 分叉 |

⇒ ⇒ 三个组件**都走 `always_ff` / mem 写口** ⇒ ⇒ **正是两后端最易分叉的路径**
⇒ ⇒ 这一点与第 1 批（纯组合）**互补**（纯组合路径上两后端几乎必然一致）。

### ② `IdStage` 是**最近 3 个真缺陷的所在层**

| 缺陷 | 修复 change | 当初怎么抓到的 |
|---|---|---|
| U 型立即数用 20 位掩码（把 `rd`/`opcode` 算进立即数） | `rv32i-load-byte-lane-2026-10` | **SoC 级固件** |
| `alu_op` 的 SUB/SRA 判定用 `funct7[5]`（对 I 型是 `imm[10]`） | 同上 change 连带修复 | **SoC 级固件** |
| B 型立即数 bit 11 落在 bit 0 | 🔴 **本批第 1 批挖出**（待修） | **本批的组件级手推期望表** |

⇒ ⇒ 这些组件是 **RV32I 微架构的单一权威**（ADR-046）⇒ ⇒ 组件级覆盖能把它前置。

## 可转性 4 问

| # | 问题 | 回答 |
|---|---|---|
| ① | 语义是否已被别的 dual 文件覆盖？ | **否** —— 三个符号在全部 60 个 macro dual 文件里**各出现 0 次**；唯一既有覆盖是 **SoC 级** hand-crafted dual ⇒ ⇒ **判别形态不同** ⇒ ⇒ 逐案裁决通过 |
| ② | 断言形态有判别力吗？ | **有** —— 变异 **11/12 抓** + 1 条如实登记的**等价变异**（见下） |
| ③ | 与后端无关吗？ | **否** —— 真跑仿真 + 读顶层端口，正是要对比两后端 |
| ④ | 形态值得新写吗？ | **是** —— 三个组件**本身带顶层端口** ⇒ ⇒ **零 DUT 复制**（直接当 DUT）⇒ ⇒ 成本最低 |

## ⚠️ 为什么**不做** `InstrTCM`（`sread` 那一侧）—— 记此以免后人重复

`InstrTCM` 用 `memory.sread(addr, true)` ⇒ 「地址在 T 拍给出、数据在 T+1 拍输出」
⇒ ⇒ 但**陷阱 T2**：地址是**顶层输入端口**时，TB 在 `tick()` **之前**就把它写进了
`data_map_` ⇒ ⇒ 沿上采样到的**已经是新地址** ⇒ ⇒ **0-cycle 与 1-cycle 的观测值完全相同**
⇒ ⇒ ⇒ **组件级的 `sread` 时序不可判别**
⇒ ⇒ 唯一可判别的形态是「地址由**同一条沿提交的 `ch_reg`** 驱动」
⇒ ⇒ 那需要一层 wrapper ⇒ ⇒ **不是零 DUT 复制** ⇒ ⇒ **本批不做**。

## 读模型（期望值的承重前提）

`Simulator::tick()`（`src/simulator.cpp:308-328`）= 边沿前组合求值 → 寄存器提交 → 边沿后组合求值
⇒ ⇒ **tick 之后读到的端口 = f(边沿后的寄存器, 本拍输入)**：

- `IdStage` / `Rv32iRegFile` ⇒ 读到的是**刚提交**的寄存器值；
- `DataTCM` ⇒ 它的「寄存器」是 `ch_mem` 的字：`aread` 是**组合**的、写在**沿上**提交
  ⇒ ⇒ **tick 后 `rdata` 是「写后」的字**（不是写前的旧字）⇒ ⇒ 这正是
  「读-改-写」的**可观测结果**（旧字 + 新字节）⇒ ⇒ 也是两后端最容易分叉的一处。

## 期望值（**手推**，禁抄仿真输出）

模型 = **独立 Python 实现**（`/tmp/opencode/gen_route_c2_table.py`）逐条按实现推导；
指令字由**规范编码器**按 RISC-V 规范生成（期望值**不是**从实现反推）⇒ ⇒ **一次通过**。

## ✅ 钉住的「气泡契约」（2026-10-25 的修复 · `rv32i-pipeline-commit-path-2026-10`）

`IdStage` 的 ID/EX 寄存器在 `stall`/`flush`（**气泡**）时：

| 字段 | 气泡时 | 依据 |
|---|---|---|
| `is_branch`/`is_load`/`is_store`/`is_jump`/`is_alu` | **清零** | 气泡时残留上一条指令的属性 ⇒ ⇒ 误判 load-use / 控制冒险 |
| `rd_addr` | **清零** | 残留 ⇒ ⇒ 与 ID 级消费者的地址**误匹配** |
| `valid_out` | **清零** | 错误路径 MUST NOT 前进到 EX 并写回 |
| 数据字段（`imm`/`opcode`/地址/数据…） | **仍刷新**（当 `valid=1`） | `valid=0`（`en=0`）⇒ ⇒ **全部保持** |

⇒ ⇒ 三个形态（**气泡** / **valid=0 保持** / **正常**）**互为判别** ⇒ ⇒ 三条变异各自可抓。

## 判别力（变异证据；每条先确认仍可编译；还原用 `cp` 后重建）

| 变异 | 内容 | 结果 |
|---|---|---|
| M1 | `IdStage`：气泡时 `valid` 不再清零 | 🔴 转红 |
| M2 | `IdStage`：气泡时 `is_load` 不再清零 | 🔴 转红 |
| M3 | `IdStage`：`valid=0` 时数据字段也刷新 | 🔴 转红 |
| M4 | `IdStage`：气泡时 `rd_addr` 不再清零 | 🔴 转红 |
| M5 | `IdStage`：**把 B 型立即数的 bit 11 修好** | 🔴 转红 ⇒ **证明生产路径的钉点有判别力** |
| M6 | `IdStage`：JALR 不再进 `rs1` 地址 mux | 🔴 转红 |
| M7 | `Rv32iRegFile`：写 x0 不再被忽略 | 🟢 **恒绿** ⇒ 见「等价变异」 |
| M8 | `Rv32iRegFile`：x1 的写地址分派错位 | 🔴 转红 |
| M9 | `Rv32iRegFile`：`rs1` 读地址分派错位（x5 ⇒ x6） | 🔴 转红 |
| M10 | `DataTCM`：写使能忽略 `valid` | 🔴 转红 |
| M11 | `DataTCM`：不做读-改-写（直接写 `wdata`） | 🔴 转红 |
| M12 | `DataTCM`：合并的「旧字」恒为 0 | 🔴 转红 |

⇒ **11/12 抓**。

### ⚠️ M7 恒绿 = 如实登记的「**等价变异**」（不是测试缺口）

写 x0 被**三重**冗余防护挡住：

1. `we = select(io().rd_addr != 0, io().rd_we, false)`（写使能排除 x0）
2. `x0->next = x0;`（x0 的寄存器**永不**被写）
3. 读端 `data_a = select(sel_a0, ch_uint<32>(0_d), data_a);`（读时硬连线 0）

⇒ ⇒ 去掉 ① 仍被 ②③ 挡住 ⇒ ⇒ **不可判别** ⇒ ⇒ **不是**测试缺口
⇒ ⇒ 顺带得到一条事实：**x0 的「不可写」在实现里是冗余的**（契约仍被守住，只是**无冗余地**）。

## 🔴 本批挖出 1 个新缺陷（已独立立项；**本批刻意不修**）

### `include/cpu/pipeline/stages/id_stage.h` **非自包含**

`#include "rv32i_decoder.h"` ⇒ 但该头在**上一级**目录（`include/cpu/pipeline/`）
⇒ ⇒ **单独 include `id_stage.h` 会编译失败**：

```
include/cpu/pipeline/stages/id_stage.h:19:10: fatal error: rv32i_decoder.h: No such file or directory
```

⇒ ⇒ 本仓**已有**一个目标在用同样的补丁（`tests/CMakeLists.txt` 的
`target_include_directories(test_rv32i_pipeline_behavior PRIVATE .../include/cpu/pipeline)`）
⇒ ⇒ **同一个补丁要打两处** ⇒ ⇒ **根因在头文件，补丁在构建脚本** ⇒ ⇒ 「症状被压住」的形态
⇒ ⇒ 违反根 `AGENTS.md` 的「**Headers are self-contained**」
⇒ ⇒ 独立立项：`.rddf/improvements/id-stage-header-not-self-contained-2026-11.md`
⇒ ⇒ **零行为影响**（只改 include 路径）⇒ ⇒ 不需 ADR。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/test_rv32i_timing_dual.cpp`（**新**） | 3 个 pair（`IdStage` 10 组 · `Rv32iRegFile` 16 组 · `DataTCM` 10 组）= **232 断言/后端** |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test` + 1 条 `target_include_directories`（`id_stage.h` 非自包含的绕过，沿用既有先例） |
| `AGENTS.md` | 4 个计数锚点 + 3 处 NOTES 计数 + 本批 NOTES 条目 |
| `.rddf/improvements/id-stage-header-not-self-contained-2026-11.md`（**新**） | 缺陷的独立立项（5 段） |
| 台账 | plan §5 墙钟 · objective §2.1bis（现状 60 → **61**）· handoff §1/§5 |

## Verification（前瞻 · 可勾选）

- [x] **N2′**：**464 == 2 × 232**
- [x] 变异 **11/12 抓** + 1 条如实登记的**等价变异**（x0 的三重冗余防护）
- [x] 期望值**手推**（独立 Python 模型 + 规范编码器 ⇒ **一次通过**）
- [x] **零 DUT 复制**（三个库组件直接当 DUT）
- [x] 每个输入端口（含**死端口** `clk`/`rst`）都显式驱动
- [x] 全量重建后 `ctest -LE perf` **227/227** 零回归
- [x] 计数锚点同步且两个计数护栏双绿
- [x] 全部 `scripts/verify_*.sh` exit 0
- [x] 归档用 `--skip-specs`（本 change 显式声明无 spec 变更）
