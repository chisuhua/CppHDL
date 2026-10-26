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

### Requirement: I 型大立即数 MUST NOT 被误判为 SUB

- **WHEN** 一条 I 型指令（`ADDI` / `ANDI` / `ORI` / …）的 12 位立即数 **`>= 0x400`** 或为负数
- **THEN** ALU 操作 MUST 仍是该 `funct3` 对应的操作（`ADDI` ⇒ `ADD`）
- **AND** MUST NOT 因为 `instr[30]` 为 1 就被判成 `SUB` / `SRA`
  （对 I 型而言 `instr[30]` 是 **`imm[10]`**，不是 `funct7[5]`）
- **AND** `SUB` / `SRA` 的判定 MUST 只对 **R 型** 与 `SRAI` 生效

#### Scenario: 负数立即数

- **WHEN** 执行 `addi x21, x0, -8`（`0xFF800A93`）
- **THEN** 结果 MUST 是 `0xFFFFFFF8`
- **AND** `0 - (-8)` 得到的 `8` MUST NOT 出现

#### Scenario: 大正立即数

- **WHEN** 执行 `addi x, x0, 0x7FF`（`0x7FF00A93`）
- **THEN** 结果 MUST 是 `2047`
- **AND** `0 - 0x7FF` 得到的 `0xFFFFF801` MUST NOT 出现

### Requirement: U 型立即数 MUST 是 `instr[31:12] << 12`

- **WHEN** 一条 `LUI` 或 `AUIPC` 执行
- **THEN** 立即数 MUST 是 `instr[31:12] << 12`（**低 12 位 MUST 清零**）
- **AND** MUST NOT 包含 `rd` / `opcode` 位

#### Scenario: `auipc` 的立即数为 0

- **WHEN** 执行 `auipc x18, 0`（`0x00000917`）
- **THEN** 立即数 MUST 是 `0`（结果 = 该指令自身的 `pc`）
- **AND** `0x917000`（把 `rd<<7 | opcode` 当立即数）MUST NOT 出现
