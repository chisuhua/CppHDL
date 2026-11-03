## ADDED Requirements

### Requirement: B 型立即数 MUST 按 `imm[12|10:5|4:1|11]` 提取

- **WHEN** 一条条件分支指令（`BEQ` / `BNE` / `BLT` / `BGE` / `BLTU` / `BGEU`）被译码
- **THEN** 立即数 MUST 按 RISC-V 规范的字段布局提取：
      `imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7]`
- **AND** `instr[7]` MUST 落在 **`imm[11]`**（即 **左移 11 位**），
      **MUST NOT** 落在 `imm[0]`
- **AND** 结果 MUST 做 **13 位 → 32 位符号扩展**（`sext<32>`）
- **NOTE**: 本条款同时约束**仓内两份**立即数提取实现
      （`include/cpu/pipeline/rv32i_decoder.h` 与
      `include/cpu/pipeline/stages/id_stage.h`）——
      二者是**重复实现**，B 型 bit 11 的笔误曾**同时**存在于两处
      ⇒ 只改一处的复发 MUST 被判别性判据挡住（见 ADR-063 D4）。

#### Scenario: 负偏移（`imm[11] = 1`，符号扩展）

- **WHEN** 译码 `beq x1, x2, -8`（`0xFE208CE3`）
- **THEN** 立即数 MUST 是 `0xFFFFFFF8`
- **AND** `0xFFFFF7F9`（bit 11 落到 bit 0 后再符号扩展）MUST NOT 出现

#### Scenario: 正偏移（`imm[11] = 1`）

- **WHEN** 译码 `beq x1, x2, +2048`（`0x002080E3`）
- **THEN** 立即数 MUST 是 `0x00000800`
- **AND** `0x00000001`（漏移位）MUST NOT 出现

#### Scenario: 小偏移（`imm[11] = 0`，负向对照）

- **WHEN** 译码 `beq x1, x2, +8`（`0x00208463`）
- **THEN** 立即数 MUST 是 `0x00000008`
- **NOTE**: 该情形在缺陷存在时**也**给出 `0x8`（`imm[11]=0` ⇒ 误置的 bit 0 亦为 0）
      ⇒ 本条**不能**单独证明修复生效，只作负向对照。

#### Scenario: 两份实现的一致性

- **WHEN** 同一组指令（B 型 `imm[11]=1` 正/负 · I/S/U/J 各 ≥1 · `|imm|<2048` 的 B 型）
      同时送进 `Rv32iDecoder` 与 `IdStage`
- **THEN** 两者的立即数输出 MUST **逐位相等**
- **NOTE**: 该判据只能发现「两份实现不一致」；**两份一起错**时它恒成立
      ⇒ 「一起错」由上面三条 Scenario 的绝对期望值负责抓。
