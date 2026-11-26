## MODIFIED Requirements

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

## ADDED Requirements

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
