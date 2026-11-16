# Capability: rv32i-privilege

## MODIFIED Requirements

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
