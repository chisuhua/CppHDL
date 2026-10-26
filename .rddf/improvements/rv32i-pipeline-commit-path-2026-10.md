---
name: rv32i-pipeline-commit-path-2026-10
priority: P0
source: C3 实测发现（objective-rv32i-e2e 的**前置**）
phase: phase-5
category: bugfix
type: feature
created_at: 2026-10-25
---

# Improvement: RV32I 流水线 commit / valid 通路修复（C3 的前置）

- **Date drafted**: 2026-10-25
- **Drafted by**: build-agent（builder 角色，C3 P2 实测时发现）
- **Status**: ✅ **已实施**（change `rv32i-pipeline-commit-path-2026-10`；追因记录见该 change 的 `design.md` §2）
- **Objective**: `objective-rv32i-e2e` 的**前置**（C3 `rv32i-soc-dual-parity-2026-10` 挂起中，恢复条件见本文件 §3.3）

## 1. Why（背景与动机）

C3 的目标是「让 RV32I **SoC 级**双路 parity 成立」。P2 起步时先把
`Rv32iPipelineTop` 从**空壳**（6 个端口全是 `ch_in`、无固件加载、无观测输出）
改造成真 DUT，再写 41 条指令的双后端逐周期比对测试 ⇒

> **测试立刻证明 SoC 根本跑不动程序。**

| 实测（41 条固件 / 200 拍） | 现象 |
|---|---|
| `perf_instructions` | **18**（固件 41 条）——且这 18 次全是「重启后头几条」 |
| `perf_branch_count` / `perf_branch_mispredict` | **18 / 18**（固件只有 2 条分支） |
| `obs_instr_addr` | 每 11 拍从 `0x80000028` 跳回 **0** |
| `obs_data_write_en` | 53/200 拍置位，地址是 ALU 结果（10 / 13 / 0），**不是** store 地址 |

注入探针（固件开头插一个 NOP，整体右移一字）⇒ 重启点从 cyc 10 移到 cyc 12
⇒ **触发点跟着程序走** ⇒ 是「某条指令触发 flush ⇒ PC 归零」而非固定周期噪声。

### 为什么必须先修，而不是「记一笔继续做 C3」

1. **C3 的验收不可达**：没有可运行的程序，「≥20 指令双路 parity」写不出来。
   强行写只能断言「两端一致地错」—— 与 C1 立的「绿 ≠ 对」原则直接冲突。
2. **这些缺陷不是 C3 引入的**，而是**此前不可观测**：SoC 顶层此前**没有任何输出
   端口** ⇒ 谁也看不见。而顶层之所以不可观测，是因为它**从未被运行过**
   （ADR-046 D10 冻结的端口集合里，4 个是死端口，另 2 个从未被引用）。
3. **既有测试是「绿而错」**：`tests/test_verilator_complex_e2e.cpp` 的 RV32I 用例
   断言 `perf_instructions == 12`，而该 fixture 的 imem **从来没有真正工作过**
   （把 `read_port` 落进了宽度不匹配的中间变量 ⇒ 取到的恒为 3）⇒ 12 是垃圾值。

## 2. What Changes（变更范围）

**17 个缺陷**（逐条追因记录见 change 的 `design.md` §2；此处只列清单）：

| # | 缺陷 | 位置 | 症状 |
|---|---|---|---|
| D1 | `flush` ⇒ PC 归零（作者自认的占位） | `stages/if_stage.h` | 每 11 拍程序重启 |
| D2 | 指令流与 PC 错开一拍（imem 的 `sread` 延迟） | `stages/if_stage.h` | ID 译码的指令比 PC 晚一个字 |
| D3 | EX 级没有自己的 pc（接的是 IF/ID 的 pc） | `stages/id_stage.h` | 分支目标偏 1 个字 |
| D4 | 冒险单元的「MEM 级」输入取自 ID 级 | `rv32i_pipeline.h` | load-use 检测退化成「与自身比较」 |
| D5 | MEM/WB 寄存器装载别级的信号 + WB 组合直连 MEM | `rv32i_pipeline.h` | WB 写回**上一条**指令的数据 |
| D6 | `mem_valid_out` 语义错（= load 数据有效） | `stages/mem_stage.h` | ALU 指令**永不写回**；退休数 = load 次数 |
| D7 | MEM 级没有内存读数据端口（局部常量 0） | `stages/mem_stage.h` | **load 路径完全没实现** |
| D8 | SoC 顶层把**字节地址**喂给**字索引**的 TCM | `examples/riscv-mini/src/rv32i_pipeline_top.h` | 取指**快 4 倍**（最初那批症状的总根源） |
| D9 | `OP_STORE` 不在「第二操作数用 imm」白名单 | `stages/ex_stage.h` | store 地址恒 0 |
| D10 | `ex_stage.io().alu_op` **从未被驱动** | `rv32i_pipeline.h` | **所有 ALU 操作都算 ADD** |
| D11 | `funct3` 对访存指令编码的是**访存宽度** | `rv32i_pipeline.h` | `sw` 被当成 `SLT`（地址 = 1） |
| D12 | `ex_stage.io().branch_type` **从未被驱动** | `rv32i_pipeline.h` | **所有分支都按 BEQ 判定** |
| D13 | 寄存器堆读地址取自**寄存后**的 ID/EX 地址 | `rv32i_pipeline.h` | 读到**上一条**指令的寄存器值 |
| D14 | 寄存器堆 `sread` + ID/EX 寄存器 = 2 拍延迟 | `rv32i_pipeline.h` | 同上（另一半原因） |
| D15 | 既有 fixture 把 `read_port` 落进宽度不匹配的中间变量 | `tests/test_verilator_complex_e2e.cpp` | 该 fixture 的 imem **从未工作过** |
| D16 | D-TCM 读端口用 `sread` ⇒ load 数据晚一拍 | `include/cpu/pipeline/rv32i_tcm.h` | load 恒读回 0 |
| **D17** | **load-use 检测晚一级（用 MEM 级 load）+ 停顿是单拍脉冲** | `include/cpu/pipeline/hazard_unit.h` | `ld_stall` **恒 0** ⇒ 冒险从未被检测；紧邻 load→use 时消费者拿到**旧值** |

**附带**：LUI / AUIPC 的 ALU 结果（`imm` / `pc + imm`）。

**观测面**（前置条件）：SoC 顶层 **32 个端口**（5 输入 + 27 输出 = 11 功能 `obs_*`
+ 16 诊断 `dbg_*`）+ `Rv32iPipeline` 的 **17** 个 `dbg_*` 输出 ⇒ ADR-046 **D10-rev**。

**契约**：新 capability `rv32i-commit-path`（ADDED **×6**）+ `verilator-rv32i-e2e`
（MODIFIED ×1：既有「绿而错」断言 ⇒ 语义断言）。

## 3. Acceptance（验收标准）

### 3.1 核心：SoC 能跑完真实程序

> ⚠️ 2026-10-25 第 13 轮 critic 追加了 **D17（load-use）** 与
> 「被冲刷的指令 MUST 从未执行」两条 ⇒ 下列金标已随之变化。
> 固件 = **48 字**（新增**紧邻 load→use** ×2 以覆盖停顿路径；其中第二处是
> 「较老指令写同一 rd」的回归用例）。

- [x] 取指序列与程序一致（含 taken 分支的跳转）；ID 的 pc 与它译码的指令**始终同拍**
- [x] `obs_data_write_en` **只在** store 拍置位（3 条 `sw` ⇒ 恰好 **3** 个 store 拍）
      ⇒ 并补了「store 拍**计数**」断言（存在性断言抓不到第 4 个 spurious 拍）
- [x] 三个 store 的 `(地址, 数据)` == ISA 正确值
      （`0x100←10`（纯 ALU）/ `0x104←**20**`（load-use 停顿后）/ `0x108←2`）
- [x] 固件 **48 字**（含「较老指令写同一 rd」回归用例）
- [x] `perf_branch_count` == 2 / mispredict == 1 / control_stalls == 1
- [x] **被冲刷的指令 MUST 从未执行**（错误路径的寄存器保持 0）
- [x] **`perf_load_use_stalls` == 6**（两处紧邻 load→use × 3 拍；修复前**恒 0**）
- [x] 「紧邻 load→use」的消费者拿到 **load 写回后的新值**（`0x104 ← 20`）
- [x] **「较老指令写同一 rd」** 的消费者同样拿到新值（`x11 == 0`，不是 14）
- [x] 「store 拍的**个数**」== 3（不只是存在性）
- [x] 退休指令数 == 推导值（**188** = 199 个拍间隔 − 11 个不增长间隔
      = 200 拍 − 2 填充 − 3+3 停顿 − 3 冲刷；⚠️ 含 §3.6 填充期的 +1）

### 3.2 双后端

- [x] 200 拍 × 27 个输出观测值**逐拍全等**（**无掩码**）

### 3.3 恢复条件（供 C3 续跑）

- [x] 前置 change 归档 ⇒ C3 可从 T2 续跑（草稿在
      `openspec/changes/rv32i-soc-dual-parity-2026-10/drafts/`）
- [ ] C3 剩余项：JIT 回落三件套断言（`is_jit_enabled` / `is_jit_compiled` /
      日志归因）+ `verilator-rv32i-e2e` 的 ADDED ×5 delta + 归档

### 3.4 门禁

- [x] `ctest -LE perf` 189/189；examples 28/28；samples 35/35
- [x] 7 个护栏 exit 0（含单源化护栏的零消费者豁免**已清空**）
- [x] `openspec validate rv32i-pipeline-commit-path-2026-10 --strict` valid

## 4. Capabilities（涉及的能力维度）

| Capability | 影响 | 备注 |
|------------|------|------|
| 新 `rv32i-commit-path` | ADDED **×6** | 退休语义 / 有效链 / 级间信号不串级 / 观测面与双后端比对 / **load-use 保持型停顿** / **停顿·冲刷期间有效链自洽** |
| `verilator-rv32i-e2e` | MODIFIED ×1 | 既有 RV32I 用例的「绿而错」断言 ⇒ 语义断言 |
| `rv32i-canonical-home` | 无 delta | 其既有条款已要求「stale 豁免 MUST 失败」⇒ 本 change 移除豁免条目即可 |

## 5. Impact（影响面）

### 5.1 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| 16 处流水线改动连环影响行为 | 🔴 高 | 每改一处都跑 SoC 级逐拍 diff（观测面已就位 ⇒ 可即时判别）+ 既有 RV32I 测试 |
| `Rv32iPipeline` / `MemStage` / `IfStage` / `IdStage` / `ExStage` 的**端口变更** | 🟡 中 | 只**新增**端口（`mem_read_data` / `pc_out` / `rs*_addr_id` / `funct7_out`），不改既有端口 ⇒ 既有消费者不受影响 |
| 「两后端分歧」曾疑似 codegen 缺陷 | 🟢 已排除 | 修完流水线后**分歧归零** ⇒ 那 52 处是坏流水线的症状 ⇒ **不得**去改 `src/codegen_verilog.cpp` |
| ADR-046 D10「冻结端口」被修订 | 🟡 中 | D10 约束对象是 C2；D10-rev 记录修订理由 + 护栏 C6 快照钉住新集合 |

### 5.2 明确不做（Non-Goals）

- **不接数据前推**（ADR-046 **D5** 仍悬置）—— 但**落地多拍停顿自保**
  （2026-10-25 用户裁决 A1-a）⇒ 固件**不再需要** ≥3 NOP 间隔
  （停顿会自保，但仍是「无前推」⇒ 停顿若干拍）
- **不实现 JIT 的 mem-read lowering**（独立 objective）
- **不重写流水线**：目标是让现有装配正确，不是换架构
- **不做 cache 的 SoC 级集成**（follow-up）
- **不做 AXI 互连**（独立 change）
- **不改 `src/codegen_verilog.cpp`**（无证据表明它有问题）

### 5.2b A1-a 裁决记录（2026-10-25）

第 13 轮 critic 的 BLOCKER 2 = 「load-use 冒险从未被检测」。两条候选路径：

| 方案 | 内容 | 裁决 |
|------|------|------|
| A1-a 多拍停顿 | 修检测时序（EX 级 load vs ID 级消费者）+ 停顿改**保持型**（持续到那条 load 写回） | ✅ **已裁决采用** |
| A1-b 显式降级 | 承认「无停顿自保」，固件 MUST 保持 ≥3 NOP 间隔 | ❌ 未采用 |

**A1-a 的边界**：= 「无前推 + 多拍停顿自保」⇒ **不触碰** ADR-046 D5
（D5 的对象是「要不要接数据前推」）。
设计细节（时序表 + 逐文件改动清单 + 波及面）见
`openspec/changes/rv32i-pipeline-commit-path-2026-10/design.md` §3。

### 5.3 复现材料

```bash
# SoC 级逐拍 trace（每个观测值的 interp/verilator 对）
RV32I_SOC_DEBUG=200 ./build/examples/riscv-mini/test_rv32i_soc_dual_parity \
    "Rv32iSoc - DualBackendTraceParity"
```
