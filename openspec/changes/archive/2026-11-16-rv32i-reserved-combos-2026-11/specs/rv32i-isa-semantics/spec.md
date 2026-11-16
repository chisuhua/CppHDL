# Capability: rv32i-isa-semantics

## MODIFIED Requirements

### Requirement: 未知 opcode MUST 可观测，且 MUST NOT 被当作 NOP 退休

ID 级 MUST 产出操作码分类（`0` = 合法 · `1` = **已知未实现**（`OP_SYSTEM`，
即 `ECALL`/`EBREAK`/CSR/`MRET`）· `2` = **未知 opcode**
· **`3` = 已知 opcode 但 `funct3` / `funct7` 是保留组合**（见下）。

⇒ ⇒ **保留组合判定（v1.2 · 2026-11-16 扩展）**：判定 MUST 覆盖 `funct3` / `funct7` 的
**ISA 保留组合** ⇒ ⇒ 判定表 = `docs/adr/ADR-071-fence-and-illegal-opcode-semantics.md` **§2.6.1**
⇒ ⇒ **唯一权威在那份表**（本 Requirement 只要求「MUST 判」+ reason 值）。
⇒ ⇒ ⚠️ **关键边界**：**U 型（`LUI` / `AUIPC`）与 J 型（`JAL`）没有 `funct3` 字段**
（bits 14:12 是**立即数位**）⇒ ⇒ 它们的 bits 14:12 **任何取值都合法**
⇒ ⇒ **MUST NOT** 把「`funct3` MUST 为 `000`」套到它们上（否则**误判合法指令为非法**）。
⇒ ⇒ **优先级**：`1`（`OP_SYSTEM`）> `3`（保留组合）> `2`（未知）> `0`（合法）。
⇒ ⇒ **判定 MUST 在库组件 `Rv32iDecoder`**（**单一来源**）⇒ ⇒ ID 级**只转发** + 保留 `valid` 门控
⇒ ⇒ **MUST NOT** 在两处各写一份判据（漂移风险）⇒ ⇒ 两份判据会**静默分叉**。

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

- ⚠️ **标题保留为可追溯性**（openspec MODIFIED 语义要求 Scenario **同名**）⇒ ⇒
  **判据以本 Scenario 正文为准**（2026-11-16 · v1.2：**本版已覆盖该维度**）
- **WHEN** 一条指令的 opcode **合法**但 `funct3` / `funct7` 是 **ISA 保留组合**
      （判定表 = **ADR-071 §2.6.1**；例：`LOAD` 的 `funct3 = 011` · `OP` 的 `funct7 = 0x40`）
- **THEN** 操作码分类 MUST 为 **`3`**（保留组合）
- **AND** 该指令 MUST **冻结**（同「未知 opcode」的处置）
- **AND** ⚠️ **负向对照**：`LUI` / `AUIPC` / `JAL` / `FENCE` 的 bits 14:12 **不是 `funct3`**
      ⇒ ⇒ 它们的**任何**取值 MUST 判为**合法**（`0`）
