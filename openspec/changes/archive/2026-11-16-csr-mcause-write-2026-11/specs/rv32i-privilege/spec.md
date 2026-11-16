# Capability: rv32i-privilege

## ADDED Requirements

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

- **WHEN** 陷阱入口被触发
- **THEN** 写口 MUST NOT 在**每一拍**都指向同一个 CSR（否则另一个 CSR **永远写不到**）
- **AND** `mcause` MUST 至少被写**一次**（判别力来源：还原「同拍双写 + 优先级 mux」的旧形态
      ⇒ ⇒ 本 Scenario **转红**）

#### Scenario: 电平持续拉高时 MUST NOT 丢数据

- **WHEN** `exception_valid` 持续为真（连续 5 拍）
- **THEN** `mepc` MUST 始终等于 `current_pc`
- **AND** 从第 2 拍起 `mcause` MUST 始终等于 `exception_cause`
- **AND** 两者 MUST NOT 串配（`mcause` MUST NOT 取到别的异常的原因码）
