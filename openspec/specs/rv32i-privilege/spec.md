# rv32i-privilege Specification

## Purpose

定义 RV32I 的**四段机器级契约**：
① **陷阱入口的**触发与重定向**（2026-11-26 起）** —— **触发集** = `illegal_reason ∈ {2, 3}`
**∪** `sys_trap_kind ≠ 0`（`ECALL` / `EBREAK` —— 二者是**合法指令**）在 ID 级被接收、
且该拍**没有**控制流冲刷时 MUST **触发一次 trap** ⇒ ⇒ `mepc`（**该指令自身的 PC**）与
`mcause`（**按触发源取值**：`ECALL` ⇒ **11** · `EBREAK` ⇒ **3** · 其余 ⇒ **2**）
MUST 以**同一次异常的一对**被写入 CSR 存储体、且 MUST 可从存储体**读回**；
取指级 PC MUST **重定向**到 `mtvec` 的 **BASE 字段**；触发指令 MUST **不退休**（IF/ID 被杀），
而**在途的更老指令 MUST 照常退休**（kill 域**只到 IF/ID**）；
② **陷阱入口的上下文写入机制** —— 见下方 `CsrBank` **单写口仲裁**（trap 上下文优先）与
**两拍配对**写法；
③ **CSR 指令（6 种 `funct3`）的语义与顺序保证** —— `csrrw` / `csrrs` / `csrrc` / `csrrwi` / `csrrsi` / `csrrci` 的
`csr_wdata` 语义、立即数型源操作数、`rd = 0` / 源为 0 的边界形态，以及 **CSR→CSR 的有序性**；
④ **`funct3 = 0` 的细分与 CSR 的 WARL 字段**（2026-11-26 · P6-4）—— `OP_SYSTEM` / `funct3 = 0`
按 `imm[11:0]` 分类（`ECALL` / `EBREAK` ⇒ **合法 + 陷入** · `WFI` ⇒ **合法 NOP** ·
`MRET` ⇒ **已知未实现（冻结）** · 其余含 `SRET` ⇒ **不可编码（陷入）**）+
`mtvec.MODE` **WARL = {0}**（写路径掩零 ⇒ 读回恒 Direct）+ `mepc[1:0]` **恒 0**（IALIGN = 32）
⇒ ⇒ **读回值恒等于硬件实际行为**。

- **为什么存在**：陷阱上下文一旦丢失或**配对错位**（`mcause` 写的是另一次异常的原因码），软件侧就**无法**判定「为何陷入、陷入在哪」⇒ ⇒ 缺陷发生在**硬件层**却**只由软件可见** ⇒ ⇒ 必须在组件级把「写入 + 配对 + 读回」钉死；**触发侧**同理 —— 若非法指令只**冻结**而不陷入，固件就**永久挂死**（既无重定向也无原因码）。
- **覆盖**：**触发与重定向**（`illegal_reason ∈ {2,3}` **∪** `sys_trap_kind ≠ 0` ⇒ 陷入 ·
  `flush` **优先**于 trap · 在途更老指令照常退休 · 触发指令不前进 · PC → `mtvec` 的 BASE 字段 ·
  `mcause` 按触发源取值 **11 / 3 / 2**）· 写入的**两拍配对**语义
  （陷阱当拍写 `mepc`、其后一拍写 `mcause`）+ 从 CSR 存储体**读回**的闭环验收 +
  `exception_valid` 为**电平**时的重复写入行为 + **`CsrBank` 单写口的同拍仲裁**（trap 优先）；
  **以及** CSR 指令的 **6 种 `funct3`** 的 `csr_wdata` 语义 · 立即数型用 `zimm`（5 位零扩展）·
  `rd = 0` 不读（`CSRRW` 仍写）· set/clear 型源为 0 不写 · **相邻两条 CSR MUST 有序（读到新值）**；
  **以及** `funct3 = 0` 的**四类处置**（`ECALL` / `EBREAK` 陷入 · `WFI` NOP · `MRET` 冻结 ·
  `SRET`/未定义 `imm` 陷入）+ `mtvec.MODE` / `mepc[1:0]` 的 **WARL coercion**。
- **⚠️ 边界**：本 capability **仍不**覆盖 —— `MRET`（`imm = 0x302`）的**完整语义**
  （`pc ← mepc` + `MIE`/`MPIE`/`MPP` 弹栈 ⇒ 归 **P6-5**；P6-4 之前它**只**按「已知未实现」
  **冻结** ⇒ `illegal_reason = 1`）· `mtval` 的真值（**P6-7**）·
  `mtvec` 的 **BASE 字段语义**（本 capability 只保证**读回值 = 硬件行为**（WARL）·
  BASE 的取值由软件决定）· `mstatus` 的陷阱侧更新（`MIE`↔`MPIE`）· `MRET` 的 PC 恢复 ·
  中断（`mtip` / `mie` / `mip` 的陷阱侧语义 ⇒ `mcause[31] = 1` ⇒ **P6-6**）·
  并发异常的排队语义 ⇒ ⇒ 这些**当前不存在**，属独立立项
  （登记在 `.rddf/improvements/rv32i-privileged-csr-2026-11.md`）。
  ⚠️ **`ECALL` / `EBREAK` / `WFI` 已由 P6-4 兑现**（2026-11-26 · change
  `rv32i-ecall-ebreak-wfi-2026-11` 归档）⇒ ⇒ 此前那句「**不**覆盖 `funct3 = 0` 的原因码与处置」
  **已作废**。
  ⚠️ **CSR 指令语义已由本 capability 覆盖**（2026-11-25 · change
  `rv32i-csr-instructions-2026-11` 归档 ⇒ 此前那句「**不**覆盖 CSR 指令语义」已**作废**）。
- **接缝（2026-11-26 起已兑现）**：`rv32i-isa-semantics` 的「未知 opcode ⇒ **流水线级冻结**」
  条款已被本 capability 的 **MODIFIED delta**（change `rv32i-trap-entry-2026-11`）**修订**为
  「`reason ∈ {2,3}` ⇒ **陷入** · `reason = 1` ⇒ **仍冻结**」⇒ ⇒ 两处**不再**是「冻结」一套口径；
  **P6-4 又修订了同一 capability 的「`reason = 1` 具体指哪些指令」**（`reason = 1` **只剩 `MRET`**）
  ⇒ ⇒ 两份 spec 的 `reason = 1` 口径**一致**。
  ⇒ ⇒ 未来若把**中断**接进来，MUST 继续以 **MODIFIED delta** 扩展触发集。

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

⇒ ⇒ **与冻结语义的接缝（2026-11-26 · P6-3 起已兑现）**：`rv32i-isa-semantics` 的
「未知 opcode ⇒ **流水线级冻结**」Requirement **已由 MODIFIED delta 修订**为
「`reason ∈ {2,3}` ⇒ **陷入** · `reason = 1` ⇒ **仍冻结**」（change `rv32i-trap-entry-2026-11`）
⇒ ⇒ 两处**不再**是「冻结」一套口径。⇒ ⇒ ⚠️ **未来**扩展触发集（`ECALL`/`EBREAK`/中断）
MUST 继续以 **MODIFIED delta** 扩展那条 Requirement（**触发信号已登记**）。

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
- **AND** `OP_SYSTEM` 的 `funct3 = 0` MUST 按 `imm[11:0]` **细分**（见下方 Scenario）

#### Scenario: `rd = 0` 与「源操作数为 0」是两个独立维度

- **WHEN** `CSRRW` 的 `rd = 0`
- **THEN** MUST **不读**旧值⇒ ⇒ 但该 CSR **仍 MUST 被写**
- **AND** **WHEN** `CSRRS` / `CSRRC` 的 `rs1 = 0`（或立即数型的 `zimm = 0`）
- **THEN** MUST **不写**该 CSR⇒ ⇒ 但读 + 写 `rd` **仍 MUST 发生**

#### Scenario: 保留组合 ⇒ 陷入；`funct3 = 0` ⇒ 仍冻结

- ⚠️ **标题保留为可追溯性**（openspec 要求 MODIFIED 的 Scenario **同名**）⇒ ⇒
  **判据以本 Scenario 正文为准**：`funct3 = 0` 已不再整体冻结，改为按 `imm[11:0]` 细分
- **WHEN** 一条 `OP_SYSTEM` 指令的 `funct3` 为 **4**（保留组合）
- **THEN** `illegal_reason` MUST 为 **3** ⇒ ⇒ MUST **陷入**（`mcause = 2`）
- **AND** **WHEN** `funct3` 为 **0**
- **THEN** MUST 按 `imm[11:0]` 分类：
  - `0x000`（`ECALL`）· `0x001`（`EBREAK`）⇒ `illegal_reason` = **0**（合法）·
    MUST 触发 trap（`mcause` = 11 / 3）
  - `0x105`（`WFI`）⇒ `illegal_reason` = **0**（合法）· MUST **正常退休**（实现为 NOP）
  - `0x302`（`MRET`）⇒ `illegal_reason` = **1**（已知未实现）· MUST **冻结**
  - 其余（含 `0x102` `SRET`）⇒ `illegal_reason` = **3** ⇒ ⇒ MUST **陷入**（`mcause = 2`）
- **AND** ⚠️ `ECALL` / `EBREAK` MUST **NOT** 被当作「合法但无功能」而**当 NOP 退休**
  （⇒ ⇒ 它们 MUST 陷入）

### Requirement: trap 入口 MUST 保存异常上下文并重定向 PC

RV32I 的**陷阱触发集** MUST 按机器级 trap 处置 —— 触发集 = `illegal_reason ∈ {2, 3}`
**∪** `sys_trap_kind ≠ 0`（`ECALL` / `EBREAK`）：

- **WHEN** 一条指令进入 ID 级，且该拍**没有**控制流冲刷（`flush`），且满足**任一**：
  - `illegal_reason ∈ {2, 3}`（未知 opcode / ISA 保留组合 / 本实现不可编码）
  - `sys_trap_kind ≠ 0`（**1 = `ECALL`** · **2 = `EBREAK`** ⇒ 二者是**合法指令**）
- **THEN** 它 MUST **触发一次 trap**
- **AND** `mepc` MUST = **该指令自身的 PC**
- **AND** `mcause` MUST 按触发源取值：`ECALL` ⇒ **11**（environment call from **M-mode**）·
  `EBREAK` ⇒ **3**（breakpoint）· 其余 ⇒ **2**（illegal instruction）
  ⇒ ⇒ 陷阱是**异常**（不是中断）⇒ `mcause[31]` MUST 为 **0**
- **AND** 取指级 PC MUST **重定向**到 `mtvec` 的 **BASE 字段**（`MODE` 不改变落点 ——
  本实现只支持 Direct ⇒ 见「`mtvec` 与 `mepc` 的 WARL 字段」）
- **AND** 该指令 MUST **不退休** ⇒ IF/ID MUST 被杀 ⇒ 在途的**更老**指令 MUST 照常退休
- **AND** `ECALL` / `EBREAK` 的 `illegal_reason` MUST 为 **0**（**合法指令**）
  ⇒ ⇒ 它们的「不前进」MUST 由**与非法指令同一条气泡路径**保证
  ⇒ ⇒ **MUST NOT** 依赖 `illegal_reason != 0` 的 stall（那会把它们误判为非法并冻结）

#### Scenario: 控制流冲刷 MUST 优先于 trap（错误路径不得写坏上下文）

- **WHEN** 同一拍既有 taken 分支（`flush`）又有触发源（**非法指令**或 `ECALL`/`EBREAK`
      位于 ID 级的 fall-through 槽）
- **THEN** MUST **不**触发 trap
- **AND** `mepc` / `mcause` MUST **保持不变**
  ⇒ ⇒ 理由：flush 那一拍 ID 级持有的是**错误路径**上的指令 ⇒ ⇒ 用它的 PC 写 `mepc`
  ⇒ ⇒ 会把**错误路径**的 PC 写进异常上下文

#### Scenario: trap MUST 多杀一拍（取指在途指令）

- **WHEN** 一条 trap 发生
- **THEN** ID 级 MUST **连续两拍**无效（第二拍对应**上一拍已提交给取指口**的地址）
- **AND** 取指侧的 skid 缓冲 MUST 在该窗口被**清空**
  ⇒ ⇒ 理由：取指是 1 拍 `sread` ⇒ ⇒ 只杀一拍会让在途指令**正常进 ID**

#### Scenario: CSR 存储体只有一条写口 ⇒ 陷阱上下文 MUST 优先

- **WHEN** 同拍既有 CSR 指令的 CSR 写、又有陷阱上下文的写（`mepc` / `mcause`）
- **THEN** **陷阱上下文**的写 MUST 生效
- **AND** CSR 指令的那次写 MUST 被抑制
  ⇒ ⇒ 理由：陷阱上下文是「**一次异常**」的**成对**写入 ⇒ ⇒ 丢它 ⇒ ⇒ 软件永远看不到原因
  ⇒ ⇒ ⚠️ 被抑制的那次写**不得**被「补偿重试」（单写口无队列）

#### Scenario: 本 Requirement 只覆盖「非法指令」这一个触发源（**触发集 = `{2, 3}`**）

- ⚠️ **标题保留为可追溯性**（openspec 要求 MODIFIED 的 Scenario **同名**）⇒ ⇒
  **判据以本 Scenario 正文为准**：触发源 = `{2, 3}` **∪** `ECALL`/`EBREAK`
- **WHEN** 触发源是 `MRET`（`imm = 0x302`）或**中断**
- **THEN** `MRET` MUST 仍按「已知未实现」处置（`illegal_reason = 1` ⇒ **冻结** ⇒ **不**触发 trap）
  ⇒ ⇒ 其 `pc ← mepc` 与 `mstatus` 弹栈语义属**独立立项**
- **AND** 中断 MUST 仍**不**实现（`mcause[31] = 1` 的路径不产生）
- **AND** `mtval` MUST **保持 0**（本 Requirement **不**覆盖其真值）
- **AND** ⚠️ 触发集 MUST 显式写成 `illegal_reason ∈ {2, 3} | sys_trap_kind ≠ 0`
  ⇒ ⇒ **MUST NOT** 写成「`illegal_reason != 0`」（那会让 `MRET` 也 trap）

### Requirement: `mtvec` 与 `mepc` 的 WARL 字段 MUST 读回硬件实际行为

`mtvec` 与 `mepc` 中的 WARL 字段 MUST 在**写路径**做 coercion，使**读回值恒等于硬件实际行为**；
`mcause` 的 Exception Code 是 **WLRL** ⇒ ⇒ 本实现 MUST **不** coercion。
`mtvec` 的复位值 MUST 落在 I-TCM 映射区内且与旧值的**字索引**相同。

#### Scenario: 写 `mtvec` 的未实现 / 保留 MODE ⇒ 读回 Direct

- **WHEN** 软件写 `mtvec = 0x8000_0101`（MODE = Vectored）或 `0x8000_0103`（MODE = 保留）
- **THEN** 读回 MUST 为 **`0x8000_0100`**（MODE = Direct）
  ⇒ ⇒ 理由：本实现只支持 Direct ⇒ ⇒ 读回 MUST 反映**行为**（不是写入值）

#### Scenario: 写奇数 `mepc` ⇒ 读回 4 字节对齐值

- **WHEN** 软件写 `mepc = 0x0000_1237`
- **THEN** 读回 MUST 为 **`0x0000_1234`**
- **AND** **WHEN** 软件写 `mepc = 0x0000_123F` ⇒ 读回 MUST 为 **`0x0000_123C`**
  ⇒ ⇒ 理由：IALIGN = 32 ⇒ ⇒ `mepc[1:0]` MUST 恒 0
  ⇒ ⇒ 否则 `MRET` 可跳到非对齐 PC（本实现**没有**对应的 misaligned 原因码）

#### Scenario: `mtvec` 复位值 MUST 是映射区内的同一落点

- **WHEN** 复位后读 `mtvec`（未做任何写入）
- **THEN** 读回 MUST 为 **`0x8000_0100`**
- **AND** 该值 MUST 位于 I-TCM 映射区内
- **AND** 其**字索引** MUST 与 `0x2000_0100` 的**字索引**相同（**`0x040`**）
  ⇒ ⇒ 理由：旧值 `0x2000_0100` **不在任何映射区**，它能工作只是因为取指丢弃高位
  ⇒ ⇒ 属**偶然别名**；换成映射区内同一 word 的值 ⇒ ⇒ **行为零变化**、读回是合法代码地址
