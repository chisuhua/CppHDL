# rv32i-privilege Specification

## Purpose

定义 RV32I 的**两段机器级契约**：
① **陷阱入口的异常上下文写入** —— 陷阱被接收时，`mepc`（异常指令 PC）与 `mcause`（异常原因码）MUST 以**同一次异常的一对**被写入 CSR 存储体，并且 MUST 可从 CSR 存储体**读回**；
② **CSR 指令（6 种 `funct3`）的语义与顺序保证** —— `csrrw` / `csrrs` / `csrrc` / `csrrwi` / `csrrsi` / `csrrci` 的
`csr_wdata` 语义、立即数型源操作数、`rd = 0` / 源为 0 的边界形态，以及 **CSR→CSR 的有序性**。

- **为什么存在**：陷阱上下文一旦丢失或**配对错位**（`mcause` 写的是另一次异常的原因码），软件侧就**无法**判定「为何陷入、陷入在哪」⇒ ⇒ 缺陷发生在**硬件层**却**只由软件可见** ⇒ ⇒ 必须在组件级把「写入 + 配对 + 读回」钉死。
- **覆盖**：写入的**两拍配对**语义（陷阱当拍写 `mepc`、其后一拍写 `mcause`）+ 从 CSR 存储体**读回**的闭环验收 + `exception_valid` 为**电平**时的重复写入行为；
  **以及** CSR 指令的 **6 种 `funct3`** 的 `csr_wdata` 语义 · 立即数型用 `zimm`（5 位零扩展）·
  `rd = 0` 不读（`CSRRW` 仍写）· set/clear 型源为 0 不写 · **相邻两条 CSR MUST 有序（读到新值）**。
- **⚠️ 边界**：本 capability **仍不**覆盖 —— `ECALL` / `EBREAK` / `MRET` / `WFI`（`funct3 = 0`，
  这些 **仍按「已知未实现」冻结** ⇒ `illegal_reason = 1`）· `mtval` 的真值 · `mtvec` 取值语义 ·
  `mstatus` 的陷阱侧更新（`MIE`↔`MPIE`）· `MRET` 的 PC 恢复 · 中断（`mtip` / `mie` / `mip` 的
  陷阱侧语义）· 并发异常的排队语义 ⇒ ⇒ 这些**当前不存在**，属独立立项
  （登记在 `.rddf/improvements/rv32i-privileged-csr-2026-11.md`）。
  ⚠️ **CSR 指令语义已由本 capability 覆盖**（2026-11-25 · change
  `rv32i-csr-instructions-2026-11` 归档 ⇒ 此前那句「**不**覆盖 CSR 指令语义」已**作废**）。
- **接缝**：本 capability **不**改变 `rv32i-isa-semantics` 的「未知 / 未实现 opcode ⇒ 流水线级冻结」条款
  ⇒ ⇒ 它只让 `OP_SYSTEM` 的**一部分** `funct3`（CSR 指令）不再属于「未实现」；
  `funct3 = 0` / `4` **仍然冻结** ⇒ ⇒ 未来把陷阱入口接进流水线时，**MUST** 以 MODIFIED delta
  修订那条冻结条款（触发信号已登记）。

## Requirements

### Requirement: 陷阱入口 MUST 把异常上下文**两个** CSR 都写进去，且 MUST 可读回

`riscv::ExceptionUnit` 在陷阱入口时 MUST 写入**异常上下文**：

- **`mepc`** = 异常指令的 PC（`current_pc`）⇒ ⇒ MUST 在**陷阱当拍**写入；
- **`mcause`** = 异常原因码（`exception_cause`）⇒ ⇒ MUST 在**其后的那一拍**写入
  ⇒ ⇒ 两个写入 MUST **同配对**（`mcause` 用**陷阱当拍锁存**的原因码）。

⇒ ⇒ **单写口约束**：`CsrBank` 只有**一条**写口 ⇒ ⇒ 两个 CSR MUST **分两拍**写
⇒ ⇒ **MUST NOT** 在同一拍里用优先级 mux 让其中一个**不可达**。

⇒ ⇒ **可读回**（判别力口径）：写入 MUST 在 `CsrBank` 的**寄存器值**上**可读回**
⇒ ⇒ 验收 MUST 用**「写进去 + 读回来」的闭环**（复合 DUT：`ExceptionUnit` → `CsrBank`）
⇒ ⇒ **MUST NOT** 只断言写口的值（写口是组合输出 ⇒ 沿后读到的是**下一拍**的写
⇒ ⇒ **MUST NOT** 能证明「陷阱当拍写的是哪个 CSR」）。

⇒ ⇒ **`exception_valid` 是电平**：持续拉高 ⇒ ⇒ 每 **2** 拍重复一次「写 `mepc` / 写 `mcause`」
序列 ⇒ ⇒ 两个 CSR 的**值** MUST 始终等于**最近一次异常**的对应值 ⇒ ⇒ **MUST NOT** 丢数据
⇒ ⇒ 使用方 SHOULD 在取走陷阱后撤销该信号。

⇒ ⇒ **范围声明（本 Requirement 只覆盖「写入 + 配对 + 读回」）**：
- ⚠️ **不在本 Requirement 覆盖内**（⇒ ⇒ **MUST NOT** 被读成「已覆盖」）：
  ① **`mtval`** —— 本版**恒 0**（`ExceptionUnit` **没有数据源** ⇒ 只有 `exception_cause`）
  ⇒ ⇒ 写入属**特权级立项**；
  ② **`mtvec` 取值语义**（`trap_entry_pc` 的来源与模式位）⇒ ⇒ 属特权级立项；
  ③ **`mstatus` 的陷阱侧更新**（MIE↔MPIE）⇒ ⇒ 属特权级立项
  （⚠️ `CsrBank` 的 `mstatus` **只有写口** ⇒ ⇒ **没有**任何「MRET 自动恢复」路径
  ⇒ ⇒ 该行为**当前不存在**，组件头注释已同步订正）；
  ④ **`MRET` 的 PC 恢复语义** ⇒ ⇒ 属特权级立项；
  ⑤ **并发异常的排队语义**（多异常在途）⇒ ⇒ 属特权级立项
  （本版 `exception_valid` 为电平 ⇒ ⇒ 持续拉高 ⇒ ⇒ 每 2 拍重复一次序列 ⇒ ⇒ **无数据丢失**）。

⇒ ⇒ **与冻结语义的接缝（显式声明 · 避免未来双头裁决）**：本 capability **不改变**
`rv32i-isa-semantics` 的「未知 / 未实现 opcode ⇒ **流水线级冻结**」Requirement
⇒ ⇒ 本契约仅在**组件级**被激励 ⇒ ⇒ 未来把 trap 入口**接进流水线**时
⇒ ⇒ MUST 以 **MODIFIED delta** 修订那条冻结 Requirement（**触发信号已登记**）。

#### Scenario: `mcause` MUST 在陷阱后的下一拍被写入并可读回

- **WHEN** 陷阱入口被触发（`exception_valid` 为真）且 `exception_cause` = 11
- **THEN** `mcause` MUST 在**下一拍**被写入
- **AND** 从 `CsrBank` 读回的 `mcause` MUST 等于 **11**
- **AND** 从 `CsrBank` 读回的 `mepc` MUST 等于触发时的 `current_pc`

#### Scenario: `mepc` MUST 在陷阱当拍被写入

- **WHEN** 陷阱入口被触发
- **THEN** `mepc` MUST 在**陷阱当拍**被写入
- **AND** 从 `CsrBank` 读回的 `mepc` MUST 等于 `current_pc`

#### Scenario: 写口 MUST NOT 让 `mcause` 不可达

- ⚠️ **标题保留为可追溯性**（openspec MODIFIED 语义要求 Scenario **同名**）⇒ ⇒ **判据以本
  Scenario 正文为准**（本条已由「负向且窗口无界」改写为「**正向两拍**」⇒ ⇒ 2026-11-16 · oracle
  SHOULD_FIX-4）
- **WHEN** 陷阱入口被触发
- **THEN** **陷阱当拍**的写 MUST 指向 `CSR_MEPC`
- **AND** **其后的那一拍**的写 MUST 指向 `CSR_MCAUSE`
- **AND** `mcause` MUST 至少被写**一次**（判别力来源：还原「同拍双写 + 优先级 mux」的旧形态
      ⇒ ⇒ 第二个 CSR **永不可达** ⇒ ⇒ 本 Scenario **转红**）
- **AND** ⚠️ 该两拍窗口对**任何**观察者（含未来的流水线集成方）**可判**
  ⇒ ⇒ 旧写法（「MUST NOT 在**每一拍**都指向同一个 CSR」）的窗口**无界**
  ⇒ ⇒ 且它判别的是**实现形状**而非**行为契约** ⇒ ⇒ 本条取代它

#### Scenario: 电平持续拉高时 MUST NOT 丢数据

- ⚠️ **标题保留为可追溯性**（同上）⇒ ⇒ **判据以本 Scenario 正文为准**（本条已补「**配对不串**」
  的**可判**形态 ⇒ ⇒ 2026-11-16 · oracle SHOULD_FIX-3）
- **WHEN** `exception_valid` 持续为真（连续 5 拍），且**第 2 拍起** `exception_cause` 与
      `current_pc` **被改成另一组值**
- **THEN** `mepc` MUST 始终等于**当次异常**的 `current_pc`
- **AND** 从第 2 拍起 `mcause` MUST 始终等于**当次异常**的 `exception_cause`
- **AND** ⚠️ **配对 MUST 不串**：第 2 拍写入的 `mcause` MUST 是**上一拍锁存**的原因码
      ⇒ ⇒ **MUST NOT** 取到**改动后**的当前输入
      （判别力来源：把写入源从「锁存值」改成「当前输入」⇒ ⇒ 本条 **转红**）
- **AND** 从**下一次**陷阱当拍起，两个 CSR MUST 同步更新为**新**的一组值

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
