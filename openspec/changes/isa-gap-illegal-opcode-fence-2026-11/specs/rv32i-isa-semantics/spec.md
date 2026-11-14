# Capability: rv32i-isa-semantics

## ADDED Requirements

### Requirement: 未知 opcode MUST 可观测，且 MUST NOT 被当作 NOP 退休

ID 级 MUST 产出操作码分类（`0` = 合法 · `1` = **已知未实现**（`OP_SYSTEM`，
即 `ECALL`/`EBREAK`/CSR/`MRET`）· `2` = **未知 opcode**）。

⇒ ⇒ 判据 MUST 以「ID 级**有效**」为门控（`valid`）—— 否则 flush 后 IF/ID 寄存器
保留的**残留旧指令**会被误判 ⇒ ⇒ 在**错误路径**上冻结流水线。

⇒ ⇒ **行为策略 = 冻结**（MUST）：非法指令进入 ID 级时 ⇒ ⇒ IF 与 ID **同时停顿**
（PC 不前进 · ID/EX 装**气泡**）⇒ ⇒ 该指令 **MUST NOT** 进入 EX、**MUST NOT** 退休。

⇒ ⇒ **MUST NOT** 把它当作 NOP 继续执行。

#### Scenario: 未知 opcode 在 ID 级被点亮

- **WHEN** ID 级持有一条有效指令，其 opcode 不属于已知指令集合
- **THEN** 操作码分类 MUST 为 `2`（未知）
- **AND** MUST NOT 为 `0`

#### Scenario: 已知但未实现的 opcode 与未知 opcode MUST 可区分

- **WHEN** ID 级持有一条 `OP_SYSTEM`（`0x73`）指令
- **THEN** 操作码分类 MUST 为 `1`（已知未实现）
- **AND** MUST NOT 为 `2`（未知）

#### Scenario: 冻结 —— 该指令永不退休

- **WHEN** 一条未知 opcode 指令进入 ID 级
- **THEN** PC MUST 停在原地（不前进）
- **AND** 该指令 MUST NOT 出现在任何一级的**有效**链上（`MUST NOT` 退休）
- **AND** 后续指令 MUST NOT 被取入并退休

#### Scenario: `flush` 的残留不得触发冻结

- **WHEN** 一条指令在**错误路径**上被冲刷，且 IF/ID 寄存器保留其旧指令字
- **THEN** 因为该拍 `valid` 为假 ⇒ MUST NOT 触发冻结

#### Scenario: 本版只覆盖 opcode

- **WHEN** 一条指令的 opcode 合法但 `funct3` / `funct7` 组合非法
- **THEN** 本版 MUST 不对其报非法（该维度**不在**本 Requirement 覆盖内）

### Requirement: `FENCE` / `FENCE.I` MUST **显式**为合法指令（本版 = `NOP`）

`FENCE` 与 `FENCE.I`（opcode `0x0F`）MUST 被**显式**列入已知指令集合
⇒ ⇒ 本版（单核、无 cache）行为 = **`NOP`**：无副作用、不写寄存器、不访存、
**不**产生操作码分类 `1`/`2`。

⇒ ⇒ 理由：把它们留在「未知」集合里 ⇒ ⇒ 合法的 `fence` 会让核冻结（假非法）。

#### Scenario: `FENCE` 不冻结

- **WHEN** 一条 `FENCE` 指令进入 ID 级
- **THEN** 操作码分类 MUST 为 `0`（合法）
- **AND** 流水线 MUST NOT 冻结（后续指令照常退休）

#### Scenario: `FENCE` 无副作用

- **WHEN** `FENCE` 流过流水线
- **THEN** 它 MUST NOT 产生寄存器写回
- **AND** MUST NOT 产生任何访存访问
