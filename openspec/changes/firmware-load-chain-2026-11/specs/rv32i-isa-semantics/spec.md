# Capability: rv32i-isa-semantics

## ADDED Requirements

### Requirement: SoC 的存储映射 MUST 显式声明，且 PC 复位值 MUST 与映射 base 一致

RV32I SoC MUST 以**显式映射表**声明其存储区域
⇒ ⇒ 且 **PC 的复位值 MUST 等于映射中 I-TCM 的 `base`**。

⇒ ⇒ 理由：PC 复位值与 I-TCM 的 `base` **恰好**都是 `0x8000_0000`
⇒ ⇒ 任何一侧改动而另一侧未改 ⇒ ⇒ **静默失配** ⇒ ⇒ 从零取指 ⇒ ⇒ **无任何报错**。

⇒ ⇒ ⚠️ 本 Requirement 是**新增**条款 ⇒ ⇒ 不改任何既有 ISA 语义条款
⇒ ⇒ 既有 `JAL` / `JALR` / `SRA` / 立即数提取等条款**逐字保留**。

#### Scenario: PC 复位值 MUST 由映射派生

- **WHEN** SoC 声明了存储映射
- **THEN** PC 的复位值 MUST 由映射中 **I-TCM 的 `base`** 派生
- **AND** MUST NOT 出现「PC 复位值」与「I-TCM base」两个**各自独立**的字面量

#### Scenario: 映射 MUST 显式

- **WHEN** 检查 SoC 的存储布局
- **THEN** 存储区域 MUST 以**显式映射表**声明（`{base, size}`）
- **AND** MUST NOT 依赖「高位被丢弃」的隐式截断行为
