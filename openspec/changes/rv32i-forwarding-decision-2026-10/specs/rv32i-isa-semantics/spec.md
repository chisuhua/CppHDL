# Capability: rv32i-isa-semantics

## MODIFIED Requirements

### Requirement: `JAL` / `JALR` MUST 重定向 PC 并写回返回地址

- **MODIFIED**: `JALR` 的 `rs1` 取值约束从「MUST NOT 依赖数据前推（**前推未裁决**）」
  改为「MUST 取**最新值**」—— 数据前推的接入与形态已裁决，前推**不再是**禁区。
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
  （数据前推已接入 ⇒ 允许来自前推通路；MUST NOT 要求固件手工插 NOP 来避开旧值）

#### Scenario: `JALR` 的 `rs1` 紧邻被写

- **WHEN** 一条 `JALR` 的 `rs1` 刚被**前一条指令**写入
- **THEN** `JALR` MUST 用**新值**计算目标地址
- **AND** MUST NOT 用该寄存器在写入**之前**的旧值

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

## ADDED Requirements

### Requirement: 数据前推 MUST 为消费者提供最新值

- **WHEN** 一条指令的源操作数依赖**前一条**（或更早）指令写入的寄存器
- **THEN** 消费者 MUST 拿到该寄存器的**最新值**
- **AND** MUST NOT 要求固件手工插 NOP 来避免读到旧值
- **AND** 消费者 MUST NOT 为了拿到新值而**额外停顿**
  （load-use 停顿是**唯一**例外 —— 见 `rv32i-commit-path` capability 的
  「load-use 冒险 MUST 被检测到，且停顿 MUST 是**保持型**」）

#### Scenario: 相邻 ALU → ALU（中间无间隔）

- **WHEN** 固件是 `addi x5, x0, 7` → `add x6, x5, x0`（**中间无 NOP**）
- **THEN** `x6` MUST 是 `7`
- **AND** MUST NOT 是该寄存器在写入**之前**的旧值

#### Scenario: 距离 2 的 load → use

- **WHEN** 固件是「一条 load」→「一条无关指令」→「一条读该 load 目标寄存器的指令」
- **THEN** 该消费者 MUST 读到 load 写入的**新值**
- **AND** MUST NOT 读到该寄存器在 load 之前的旧值

#### Scenario: 分支条件用紧邻新值

- **WHEN** 固件是 `addi x1, x0, 1` → `beq x1, x0, label`（**中间无 NOP**）
- **THEN** 分支 MUST 按 `x1` 的**新值**（`1`）判定 ⇒ MUST **不**被 taken
- **AND** MUST NOT 按写入之前的旧值判定

#### Scenario: jump 生产者的写回值 MUST 被前推

- **WHEN** 固件用 `JAL` 跳到子程序，且**距离 ≥ 2** 之后的指令读 `ra`
- **THEN** 该指令 MUST 拿到**返回地址**（`JAL` 的 `pc + 4`）
- **AND** MUST NOT 拿到 ALU 的垃圾结果

#### Scenario: 档位选择 MUST 与数据源的时间对齐

- **WHEN** 前推的档位选择被计算
- **THEN** 档位 MUST **延迟一拍**再生效
  （否则 MUST 把「生产者自己的结果」喂给当前正在执行的指令 ⇒ 静默错值）
- **AND** 档位 `1`（EX/MEM）的数据源 MUST 是 EX/MEM **流水线寄存器**
- **AND** 档位 `2`（MEM/WB）的数据源 MUST 是 MEM/WB **流水线寄存器**
- **AND** 数据源 MUST NOT 取自 EX/MEM 级的**组合输出**（那正是组合环的成因）
