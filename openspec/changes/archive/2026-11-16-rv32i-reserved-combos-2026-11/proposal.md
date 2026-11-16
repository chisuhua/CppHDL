# Proposal: `funct3` / `funct7` **保留组合**检测（G1 的 residual 维度 → 纳入本版）

## Why

`rv32i-isa-gap-closure-2026-11` 的 **G1** 当初**只覆盖 opcode** ⇒ ⇒ `funct3` / `funct7` 的
**ISA 保留组合**（例：`LOAD` 的 `funct3=011` · `OP` 的 `funct7=0x40`）被当作**合法指令执行**
⇒ ⇒ 与「未知 opcode 被当 NOP」**同型**的静默缺陷族。

⇒ ⇒ **ADR-071 v1.2** 已 supersede 其 §2.6 D6（「本版只覆盖 opcode」）⇒ ⇒ 本 change 把
**判据面**同步（capability），实现已在同一批落地。

## What Changes

- Requirement「未知 opcode MUST 可观测…」的**正文**：reason 码由 **3 态扩到 4 态**
  （新增 **`3` = 已知 opcode 但 `funct3`/`funct7` 是保留组合**）+ **判定表指针**（ADR-071 §2.6.1）
  + **单一来源**规则（判定 MUST 在 `Rv32iDecoder` ⇒ ID 级只转发）。
- Scenario「本版只覆盖 opcode」：**标题保留**、**正文改写**为「本版**已覆盖**」+
  **负向对照**（`LUI`/`AUIPC`/`JAL`/`FENCE` 的 bits 14:12 不是 `funct3` ⇒ 任何取值都合法）。

## 关键事实（**手推 · 实测踩过**）

⚠️ **U 型（`LUI`/`AUIPC`）与 J 型（`JAL`）没有 `funct3` 字段** —— bits 14:12 是**立即数位**
⇒ ⇒ 把「`funct3` MUST 为 `000`」套到它们上 ⇒ ⇒ **误判合法指令为非法**
（实测：`lui x10, 0x1` = `0x00001537` 的 bits 14:12 = `001` ⇒ **合法**）。
⇒ ⇒ 本条已写成 Scenario 的**负向对照** ⇒ ⇒ 防止后人在「统一合法性」时**顺手回归**。

## Out of Scope

- **实现**（已在同一批落地：`rv32i_decoder.h` 的保留组合判定 + `id_stage.h` 改为转发）
- **RV32I 之外的扩展**（`Zbb` / `M` / `A` …）⇒ ⇒ 不做
- **`SYSTEM`（`0x73`）的 `funct3`**：归 reason **1**（已知未实现）⇒ ⇒ 不重复判（否则 reason 抖动）

## 验收标准（可勾选）

- [x] reason 码 **4 态**（`0`/`1`/`2`/`3`）且**优先级**写死（`1` > `3` > `2` > `0`）
- [x] 判定表指针 = **ADR-071 §2.6.1**（唯一权威）
- [x] **单一来源**：判定在 `Rv32iDecoder` ⇒ ID 级只转发（`id_stage.h` 的 4 个局部量**已删**）
- [x] **负向对照**：`LUI`/`AUIPC`/`JAL`/`FENCE` 的 bits 14:12 任何取值 ⇒ 合法
- [x] **金标不变**（实测）：`ctest -LE perf` 255/255 · 独立 Verilog TB `checks=24 fails=0`
- [x] **变异自证**：去掉 `LOAD` 的 `funct3` 检查 ⇒ ⇒ 保留组合用例**转红**；还原 + **重建** ⇒ 复绿
- [x] ⚠️ **顺带挖出的夹具缺陷**：差分表的 `sub x1,x2,x3` 编码 `0x803100B3` 的 **`funct7=0x40`**
      = **保留组合** ⇒ ⇒ 已按**手推**修正为 `0x403100B3`（`funct7=0x20`）· 期望字段 64 → 32
- [x] **自证判别力**：我为新用例手推的编码**第一版写错**（`0x00001083` = `funct3=001` = `LH`
      ⇒ 合法）⇒ ⇒ 被**同一条判据当场抓住** ⇒ ⇒ 修正为 `0x00003083`
- [x] spec delta 进主 spec · `openspec validate --specs` 绿

## 用户可见变更

| 面 | 变更 | 迁移动作 |
|---|---|---|
| **`riscv::Rv32iDecoder`** | 新增输出 **`illegal_reason`**（2 bit · 4 态 `0`/`1`/`2`/`3`）⇒ ⇒ 产物顶层端口 **+1**（`decoder_unnamed_output` — ⚠️ 它占了 `_0` ⇒ **既有 8 根 decoder 端口整体 +1 位移**） | 解析产物的 decoder 端口表者 MUST **按新产物重取**（编号位移） |
| **`riscv::IdStage`** | `illegal_reason` 的**值域扩展**（新增 `3`）⇒ ⇒ 端口面**不变**（仍 1 个 2 bit 输出） | 读 `3` 的下游 MUST 处理「保留组合」语义 |
| **行为（可见）** | **`funct3` / `funct7` 的 ISA 保留组合**（例：`LOAD` 的 `funct3=011` · `OP` 的 `funct7=0x40`）此前被**当合法指令执行** ⇒ ⇒ 现在 **reason 3 + 冻结** | ⚠️ 若你的固件里**手写过**保留组合编码 ⇒ MUST 修正（仓内实测**零命中** ⇒ 既有交付**零影响**） |
| **capability** | `rv32i-isa-semantics` 的对应 Requirement：reason 码 **3 态 → 4 态** + 判定表指针（ADR-071 §2.6.1）+ **单一来源**规则 | — |
