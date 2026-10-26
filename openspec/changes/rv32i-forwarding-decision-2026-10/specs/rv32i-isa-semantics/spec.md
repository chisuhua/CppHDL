# Capability: rv32i-isa-semantics

## MODIFIED Requirements

### Requirement: `JAL` / `JALR` MUST 重定向 PC 并写回返回地址

- **MODIFIED**: `JALR` 的 `rs1` 取值约束从「MUST NOT 依赖数据前推（**前推未裁决**）」
  改为「MUST 取**最新值**」—— 数据前推的接入与形态已裁决，前推**不再是**禁区。
  ⚠️ 前推的**微架构契约**（档位 / 数据源 / 时间对齐 / 距离矩阵）MUST NOT 写在本
  capability ⇒ 那是 `rv32i-data-forwarding` 的职责（**单一权威**）。
  其它条款（重定向 / 返回地址 / 目标地址算法 / 错误路径冲刷 / 计数器口径）**不变**。

- **WHEN** 一条 `JAL` 或 `JALR` 在 RV32I SoC 上执行
- **THEN** PC MUST 被重定向到该指令的目标地址
- **AND** `rd` MUST 写回**返回地址**（该指令的 `pc + 4`）
- **AND** 目标地址 MUST 是：`JAL` ⇒ `pc + imm_j`；`JALR` ⇒ `(rs1 + imm_i) & ~1`
- **AND** 错误路径的指令 MUST 被冲刷（MUST NOT 产生任何效果）

#### Scenario: 子程序调用与返回

- **WHEN** 固件用 `JAL` 跳到一个子程序，子程序写一个寄存器后用 `JALR` 返回
- **THEN** 子程序写的寄存器值 MUST 被主程序读回
- **AND** `ra`（x1）MUST 等于 `JAL` 指令的 `pc + 4`

#### Scenario: `JALR` 的目标地址

- **WHEN** 一条 `JALR` 执行且 `rs1 + imm_i` 的最低位为 1
- **THEN** 目标地址 MUST 是 `(rs1 + imm_i) & ~1`（最低位 MUST 被清零）
- **AND** `rs1` MUST 取**该指令在 EX 级的最新值**
  （数据前推已接入 ⇒ 允许来自前推通路；MUST NOT 要求固件手工插 NOP 来避开旧值；
  前推的微架构契约见 `rv32i-data-forwarding` capability）

#### Scenario: 跳转 MUST NOT 劫持 PC（错误路径上的跳转）

- **WHEN** 一条 `JAL`/`JALR` 的**错误路径**上紧跟着另一条 `JAL`/`JALR`
  （`call; call`）
- **THEN** 第二条跳转 MUST NOT 改变 PC（它 MUST 被冲刷）
- **AND** 「是否跳转」的判定 MUST 用**气泡清零后**的跳转标志
  （MUST NOT 只看 `opcode` —— 错误路径的 `opcode` 会在气泡拍残留）

#### Scenario: 性能计数器口径

- **WHEN** 统计 `branch_count` / `branch_mispredict` / `control_stalls`
- **THEN** `branch_count` 与 `branch_mispredict` MUST 只统计**条件分支**
  （`JAL`/`JALR` 是**无条件**跳，不是「分支」也不是「误预测」）
- **AND** `control_stalls` MUST 统计**全部控制重定向次数**（含 `JAL`/`JALR`）
- **AND** `control_stalls` 的口径 MUST 是**次数**（每次 +1），MUST NOT 被描述为「停顿拍数」
  （每次重定向的实际代价是 3 拍气泡）
