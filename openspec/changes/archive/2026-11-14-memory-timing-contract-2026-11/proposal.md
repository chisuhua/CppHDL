# Proposal: 存储器时序契约 —— D-TCM 改寄存读 + MEM 级数据对齐（`rv32i-memory-timing`）

> **来源**：`.rddf/improvements/memory-timing-contract-2026-11.md`（T1 / T3 / T4 / T5 / T6）
> **架构依据**：`docs/adr/ADR-068-dtcm-memory-timing-contract.md`（**Accepted** · V1–V10）·
> `docs/adr/ADR-069-mem-data-alignment.md`（**Accepted** · V1–V15 · **决策 = 候选 ②（A0）**）
> ⚠️ **🚪 开工门已闭**：T2 裁决（R5 · ADR-069 Accepted）+ 公开面前置登记（R6 ·
> `docs/CHANGELOG.md` + `.rddf/checks/public-api-surface.md`「行为变更」节各 1 行）。
> ⚠️ **顺序硬约束**：本 change MUST 先于 **B4**（`rv32i-dead-component-consolidation` 的 C1–C4）
> —— 时序金标只算一遍 ⇒ ⇒ 反序 ⇒ 「退休数 / 停顿拍数」**两次作废**。

## Why

### 缺陷（实测 · 可复算）

1. **D-TCM 的读是 0 拍组合读** ⇒ ⇒ 产物里是「4096:1 组合 mux」
   （`include/cpu/pipeline/rv32i_tcm.h:113` 的 `old_word <<= memory.aread(io().addr);`）
   ⇒ ⇒ ⇒ **无法推断 BRAM** ⇒ ⇒ 上板时只能实现成分布式寄存器阵列 ⇒ ⇒ **面积与时序双输**。

2. **I-TCM 与 D-TCM 的读时序语义不一致**：取指侧走 **1 拍 `sread`**（`pc_lag` 对齐），
   数据侧走 **0 拍 `aread`** ⇒ ⇒ 同一个 SoC 里两套读语义 ⇒ ⇒ 下游无法用统一模型推理。

3. **`data_ready` 是死端口**（`include/cpu/pipeline/rv32i_pipeline.h:61` 只有声明，
   `include/cpu/**` 内零消费者）⇒ ⇒ 存储器时序契约**没有任何可观测的接口**。

4. **写侧相位没有契约** ⇒ ⇒ `sb` / `sh` 的「写使能 MUST 与被覆盖的那一拍地址对齐」**未定义**
   ⇒ ⇒ 改 D-TCM 读时序后，相位问题会从「看不见」变成「看得见的错」。

### 为什么现在做

- 用户诉求 **②（固件/ELF 装载）** 与 **⑦（RTL 卫生）** 的功能面都卡在这条链上：
  装载通路要往 D-TCM 写数据 ⇒ ⇒ 写侧的时序契约 MUST 先定 ⇒ ⇒ 否则装载实现会被迫
  绕过时序（⇒ ⇒ 又是「声称成立但无人可验证」）。
- **B4 必须等 B3** ⇒ ⇒ 现在做 B3 ⇒ ⇒ 是主线的唯一顺序。

## What Changes

### T1 — D-TCM 读改寄存读（1 拍）

```cpp
// 现状（0 拍组合读）
ch_uint<DATA_WIDTH> old_word;
old_word <<= memory.aread(io().addr);

// 改后（1 拍寄存读）
auto read_data = memory.sread(io().addr, ch_bool(true));
```

⇒ ⇒ **产物形态变化**：组合 mux 读 ⇒ 寄存读 ⇒ ⇒ **可推断 BRAM**。

### T2 — MEM 级数据对齐（**已裁决 = A0**）

**A0 = 访存地址的呈现拍照旧**（仍取自 EX/MEM 级寄存器）
**+ MEM/WB 装载点延后 1 拍**（EX/MEM → **M2** → MEM/WB）⇒ ⇒ **load-use 停顿 3 → 4 拍**。

⇒ ⇒ **三个强制结构变更**（ADR-069 §3.3 · 漏任一 ⇒ ⇒ **静默错值**）：

| # | 强制项 | 判据 |
|---|---|---|
| ① | 前推**档位 2**的数据源 MUST 由 `memwb_*` 改指 **M2** | ADR-069 V13 |
| ② | **D11**（WB→ID 组合旁路）的源 MUST 改指「下一提交者值」 | ADR-069 V14 |
| ③ | **停顿链加一级**（`stall_hold_d3` 或等价）⇒ ⇒ `hazard_unit` 组件内部有改动 | ADR-069 V15 |

⇒ ⇒ ⚠️ **停顿时序契约 MUST 与 ADR-047 §5.5 的 dated addendum 一致**（3 拍 → **4** 拍）。

### T3 — MEM 级反压（`data_ready`）

- `data_ready` 由「死端口」改为 **MEM 级的真实输入**。
- ⚠️ **固定 1 拍延迟 ≠ 握手**：`data_ready` 的语义 MUST 登记为「**固定 1 拍延迟**」
  ⇒ ⇒ **禁**在契约里写成「握手 / 等待就绪」。
- MEM 级等 `data_ready` 时：冻结 MEM/WB 寄存器 + 向前级发 stall ⇒ ⇒ 有效链保持。
- ⚠️ **判别力前提**：必须有一个**能拉低 `data_ready`** 的 DUT（测试专用）
  ⇒ ⇒ 否则该通路**零覆盖** ⇒ ⇒ 判据恒绿。

### T4 — 字节写相位契约

- 写使能 MUST 与**被覆盖的那一拍地址**对齐。
- 判据 MUST 覆盖 `sb` / `sh` 的**四个车道 × 两个半字车道**组合（现有仅 2 个用例）。

### T5 — 时序金标重算（**全部手推**）

受影响的期望值：`perf_instructions`（当前 178）· `load_use_stalls`（当前 6）·
`control_stalls`（当前 4）· store 拍数（当前 5）· `dbg_id_pc` 逐拍序列 · 24 条写回序列。

⇒ ⇒ ⚠️ **禁抄仿真输出**（仓内纪律：两后端同源 ⇒ ⇒ parity 对该族缺陷**结构性失明**
⇒ ⇒ 期望值 MUST 手推 ⇒ ⇒ 且手推值本身要能判别）。

### T6 — 判别性判据（**紧邻 SW/LW**）

紧邻 `SW x1, 0(x2)` / `LW x3, 0(x2)`（**同址 · 零间隔**）⇒ ⇒ 断言 `x3 == 写入值`
⇒ ⇒ 覆盖 `read-during-write` 语义。

⇒ ⇒ ⚠️ **现固件的「隔 4 字」形态 MUST NOT 冒充该判据**（`SW`@word16 / `LW`@word20）⇒ ⇒ 新用例 MUST **紧邻**。

## Impact

### Affected specs

| capability | 动作 | 说明 |
|---|---|---|
| **`rv32i-memory-timing`**（**新**） | ADDED | 存储器时序契约：读 = 固定 1 拍寄存读 · MEM 数据对齐（A0）· 写相位 · 反压语义 |
| `rv32i-commit-path` | MODIFIED | 时序金标基线订正（退休数 / 停顿拍数 / 逐拍序列）⇒ ⇒ **标题逐字保留** |
| `rv32i-isa-semantics` | MODIFIED | 追加「字节/半字写的**相位**契约」+「MEM 反压期间写使能保持」⇒ ⇒ **标题逐字保留** |
| `mem-read-timing` | **不动** | 该 capability 讲 `ch_mem` 端口自身的时序 ⇒ ⇒ 本项是其**使用侧**契约 ⇒ ⇒ MUST NOT 复述 |
| `cpu-cache-subsystem` | **不动**（引用） | 本项只满足接 cache 的**时序前置** ⇒ ⇒ cache 接入仍属后续立项 |

### Affected code

| 路径 | 改动 |
|---|---|
| `include/cpu/pipeline/rv32i_tcm.h` | D-TCM 读口 `aread` → `sread`（1 拍） |
| `include/cpu/pipeline/rv32i_pipeline.h` | `data_ready` 接真信号 + MEM 级 stall 通路 |
| `include/cpu/pipeline/hazard_unit.h` | 停顿链加一级（ADR-069 §3.3 ③） |
| `include/cpu/pipeline/stages/*.h` | MEM/WB 装载点后移一拍 + 前推档位 2 源改指 M2（① ②） |
| `include/cpu/AGENTS.md` | 时序契约登记（`data_ready` 语义 = 固定 1 拍延迟，非握手） |
| `tests/**` | 时序 golden 手推重算 + T6 判别性用例 + 反压 DUT |

### 不做（**Out of Scope**）

- ❌ **cache 接入**（`cpu-cache-subsystem`）—— 仍属独立立项。
- ❌ **固件装载链路**（`prog_addr` 语义 / D-TCM 加载通路）—— 属 `firmware-load-chain`。
- ❌ **CSR / ECALL / FENCE** —— 属 `rv32i-isa-gap-closure`（P6）。
- ❌ **分支预测器接入** —— 独立立项。

## 用户可见变更

- **`riscv::DataTCM` 的读口语义变更**（`include/cpu/pipeline/rv32i_tcm.h`）
  ⇒ ⇒ 由**全组合读**（`aread` · 0 拍）改为**寄存读**（`sread` · 1 拍）
  ⇒ ⇒ ⚠️ **迁移指引**：下游若按「**同拍**读回数据」写的 TB ⇒ ⇒ **MUST 改成「下一拍」**
  ⇒ ⇒ 内存时序 golden **全部**重算（`load_use_stalls` 6 → **8** · `non_retired` 21 → **23**
  · 退休数 178 → **176**）。
- **`Rv32iPipeline::data_ready` 的语义** ⇒ ⇒ 登记为「**固定 1 拍延迟**（**不是握手**）」
  ⇒ ⇒ **driver 仍恒 1**（⚠️ **禁**在本批实现真反压 ⇒ ⇒ 那是「接 cache」独立立项）。
- **新增组件 `riscv::LoadLaneSelector`**（`include/cpu/pipeline/load_lane_selector.h`）
  ⇒ ⇒ load 的**车道选择 + 符号 / 零扩展**从 `MemStage` **迁出**到 **M2 的下游**（语义**逐字**不变）。
- **`riscv::MemStage` 的 `mem_read_data`（输入）/ `mem_data`（输出）移除**
  ⇒ ⇒ 载体内接线已迁到 `m2_load_data` ⇒ ⇒ 下游改用 `riscv::LoadLaneSelector`。
- **契约面**：新增 capability **`rv32i-memory-timing`**（5 requirements）·
  `rv32i-commit-path` / `rv32i-data-forwarding` / `rv32i-isa-semantics` 各有 Requirement 订正
  ⇒ ⇒ 依据 = `docs/adr/ADR-069-mem-data-alignment.md`（**Accepted** · 决策 **A0** · §7.6 追认）。
