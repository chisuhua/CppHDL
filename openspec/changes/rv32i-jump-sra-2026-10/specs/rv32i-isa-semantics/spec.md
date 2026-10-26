# Capability: rv32i-isa-semantics

## ADDED Requirements

### Requirement: `JAL` / `JALR` MUST 重定向 PC 并写回返回地址

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
- **AND** `rs1` MUST 取 EX 级的寄存器值（MUST NOT 依赖数据前推 —— 前推未裁决）

### Requirement: `SRA` MUST 是算术右移

- **WHEN** 一条 `SRA` 在 RV32I SoC 上执行
- **THEN** 结果 MUST 是**算术右移**（高位 MUST 用源操作数的符号位填充）
- **AND** MUST NOT 等同于逻辑右移（`SRL`）
- **AND** 移位量为 0 时结果 MUST 等于源操作数

#### Scenario: 负数的算术右移

- **WHEN** 源操作数是 `0xFFFFFFF8`（`-8`）且移位量是 1
- **THEN** 结果 MUST 是 `0xFFFFFFFC`（`-4`）
- **AND** 逻辑右移得到的 `0x7FFFFFFC` MUST NOT 出现

#### Scenario: 两处 ALU 实现 MUST 一致

- **WHEN** 流水线内联的 ALU 与独立的 ALU 组件都实现 `SRA`
- **THEN** 两者 MUST 给出**逐位相同**的结果
- **AND** MUST NOT 只实现其中一处（否则留下「组件层对、流水线层错」的分裂）
