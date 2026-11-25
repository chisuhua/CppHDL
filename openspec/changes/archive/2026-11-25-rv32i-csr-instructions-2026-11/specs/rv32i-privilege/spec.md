## Purpose

> ⚠️ **本 delta 的 `## Purpose` 不会被合并进主 spec**（`docs/developer_guide/openspec-specs-guide.md`
> §「`## Purpose` 段 MUST 在 archive 之后手工写入」）⇒ ⇒ **归档后** MUST 用本段
> **替换** `openspec/specs/rv32i-privilege/spec.md` 的 Purpose ⇒ ⇒
> ⇒ ⇒ **唯一要改的一句** = 删掉「**不**覆盖 CSR 指令语义（`csrrw`/`csrrs`/`csrrc`）」⇒ ⇒
> ⇒ ⇒ 并把「这些**当前不存在**」改为「CSR 指令语义**已由本 capability 覆盖**；其余仍属独立立项」。

定义 RV32I **CSR 指令**（`csrrw` / `csrrs` / `csrrc` / `csrrwi` / `csrrsi` / `csrrci`）的
**语义**与**顺序保证**契约 —— 覆盖 6 种 `funct3` 的读写语义、立即数型源操作数、
「不读 / 不写」的边界形态，以及 **CSR→CSR 依赖的有序性**。

- **为什么存在**：CSR 指令此前**完全不可用**（整个 `OP_SYSTEM` 归「已知未实现」⇒ 流水线冻结）
  ⇒ 而 `CsrBank` 的硬件**早已实现**（9 个 CSR + 组合读口 + 沿写口）⇒ ⇒ **有硬件、无指令**的倒挂
  ⇒ ⇒ 软件侧看不到任何错误信号 ⇒ ⇒ 必须在**流水线级**把「指令语义」与「顺序保证」钉死。
- **覆盖**：6 种 `funct3` 的 `csr_wdata` 语义 · 立即数型用 `zimm` · `rd = 0` 不读 /
  `rs1 = 0`（或 `zimm = 0`）不写 · **CSR→CSR 依赖 MUST 有序**（相邻两条 MUST 读到**新值**）。
- **⚠️ 边界（本 Requirement 只覆盖「指令语义 + 顺序保证」）**：
  ① **不**覆盖 `funct3 = 0`（`ECALL` / `EBREAK` / `MRET` / `WFI`）⇒ ⇒ 那些**仍是「已知未实现」**
  ⇒ ⇒ **冻结**；② **不**覆盖 trap 入口接线（`mepc` / `mcause` 写）⇒ ⇒ 那是**独立立项**；
  ③ **不**覆盖中断（`mtip` / `mie` / `mip` / `mstatus` 陷阱侧更新）；
  ④ **不**覆盖 `mtval` 的真值（cause 相关）；⑤ **不**覆盖 `misa` / `mip` 的**可写性语义**
  （本版写入仍被忽略 · 读返回 0）。
- **接缝**：本 Requirement **不改变** `rv32i-isa-semantics` 的「未知 / 未实现 opcode ⇒
  流水线级冻结」条款 ⇒ ⇒ 只是 `OP_SYSTEM` 的**一部分** `funct3` 不再属于「未实现」
  ⇒ ⇒ ⇒ 保留组合（`funct3 = 4`）与 `funct3 = 0` **仍然冻结**。

---

## ADDED Requirements

### Requirement: CSR 指令 MUST 按 `funct3` 表计算 `csr_wdata`

- **WHEN** 一条 CSR 指令进入流水线
- **THEN** 它 MUST **正常退休**（MUST NOT 冻结）⇒ ⇒ `illegal_reason` MUST 为 **0**
- **AND** 它的 `csr_wdata` MUST 按**下表**计算
- **AND** 源操作数为 `rs1` 的形态与源操作数为 `zimm` 的形态 MUST **可区分**
  ⇒ ⇒ 判据 MUST 用一条 `rs1_value ≠ zimm` 的指令

#### Scenario: 6 种 `funct3` 的 `csr_wdata` 语义

`csr_old` = 该 CSR 的**当前值** · `rs1_value` = 源寄存器的值 · `zimm` = 指令中
5 位立即数的**零扩展**值。

| 指令 | `funct3` | `csr_wdata` | 源操作数 |
|---|---|---|---|
| `CSRRW` | `001` | `rs1_value` | 寄存器 |
| `CSRRS` | `010` | `csr_old \| rs1_value` | 寄存器 |
| `CSRRC` | `011` | `csr_old & ~rs1_value` | 寄存器 |
| `CSRRWI` | `101` | `zimm` | **立即数** |
| `CSRRSI` | `110` | `csr_old \| zimm` | **立即数** |
| `CSRRCI` | `111` | `csr_old & ~zimm` | **立即数** |

- ⚠️ `CSRRC` / `CSRRCI` 的掩码 MUST 是「**按位取反后 AND**」⇒ ⇒ **MUST NOT** 用 XOR
- ⚠️ 写路径与读路径 MUST 用**同一个** `csr_old`（**MUST NOT** 用写后的新值）

### Requirement: CSR 指令的边界形态与 CSR→CSR 的顺序保证 MUST 正确

- **WHEN** 一条读操作（`csrrs rd, csr, x0` 形态）执行
- **THEN** 它 MUST 把 `csr_old` 写入 `rd`
- **AND** `rd = 0` ⇒ ⇒ MUST **不读**旧值（`CSRRW` 仍 MUST 写）
- **AND** `rs1 = 0`（或立即数型的 `zimm = 0`）且指令是 set/clear 形态
  ⇒ ⇒ MUST **不写**该 CSR（读 + 写 `rd` 仍 MUST 发生）
- **AND** 相邻两条 CSR 指令（前者写 / 后者读**同一个** CSR）⇒ ⇒ 后者 MUST 读到
  **写入后的新值**⇒ ⇒ **MUST NOT** 读到写入前的旧值
  ⇒ ⇒ ⇒ 依据 = RISC-V 规范要求 **CSR 访问之间保持顺序**
  ⇒ ⇒ ⇒ 实现 MUST 为此引入**停顿或等价机制**⇒ ⇒ **MUST NOT** 以「接受读旧值」实现

#### Scenario: `rd = 0` 与「源操作数为 0」是两个独立维度

- **WHEN** `CSRRW` 的 `rd = 0`
- **THEN** MUST **不读**旧值⇒ ⇒ 但该 CSR **仍 MUST 被写**
- **AND** **WHEN** `CSRRS` / `CSRRC` 的 `rs1 = 0`（或立即数型的 `zimm = 0`）
- **THEN** MUST **不写**该 CSR⇒ ⇒ 但读 + 写 `rd` **仍 MUST 发生**

#### Scenario: 保留组合与 `funct3 = 0` MUST 仍然冻结

- **WHEN** 一条 `OP_SYSTEM` 指令的 `funct3` 为 **4**（保留组合）
- **THEN** `illegal_reason` MUST 为 **3**（保留组合）⇒ ⇒ 且流水线 MUST 冻结
- **AND** **WHEN** `funct3` 为 **0**（`ECALL` / `EBREAK` / `MRET` / `WFI`）
- **THEN** `illegal_reason` MUST 为 **1**（已知未实现）⇒ ⇒ 且流水线 MUST 冻结
  ⇒ ⇒ ⇒ **MUST NOT** 因本 Requirement 而变成「合法但无功能」
  （⇒ ⇒ 那会比「冻结」更糟 ⇒ ⇒ 会当 NOP 退休）
