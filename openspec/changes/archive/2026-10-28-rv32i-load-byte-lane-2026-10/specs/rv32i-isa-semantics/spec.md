# 变更规格：RV32I load 字节道语义（LB / LH / LBU / LHU）

## ADDED Requirements

### Requirement: load 字节道 MUST 按地址选车道并按 `funct3` 扩展

- **ADDED**: 「字节道**写**」的**成对**条款（写 = lane 掩码；读 = 车道选择 + 扩展）。
  实现依据：`mem_stage.h:99-166`。

- **WHEN** 一条 load 指令读回一个**字**
- **THEN** MUST 按**地址低位**选车道（`LB`/`LBU`：`addr[1:0]`；`LH`/`LHU`：`addr[1]`）
- **AND** MUST 按 `funct3` 扩展（`LB`/`LH` **符号**、`LBU`/`LHU` **零**）
- **AND** 符号扩展 MUST 把**被选车道**的**最高位**复制到结果的**全部**高位
      （bit7 → bit31..8 / bit15 → bit31..16）
- **AND** 未被选中的车道 MUST NOT 影响结果
- **NOTE**: `LW` 不扩展；取值选取属实施细节 ⇒ 见 `design.md` §2.1/§2.2。

#### Scenario: LB —— 字节道选择 + 符号扩展

- **WHEN** 内存 `[0x100]` = `0xA1B2C3D4`，执行 `lb` 读地址 `0x101`
- **THEN** 读到的字节 MUST 是该字的 **lane 1** = `0xC3`
- **AND** 结果 MUST 是**符号扩展**后的 `0xFFFFFFC3`
- **AND** 反向期望（MUST NOT）：lane 恒取 lane 0 ⇒ `0xFFFFFFD4`；
      零扩展 ⇒ `0x000000C3`；
      符号位填充位置错（高位未补满）⇒ `0xFF0000C3`

#### Scenario: LBU —— 字节道选择 + 零扩展

- **WHEN** 内存 `[0x100]` = `0xA1B2C3D4`，执行 `lbu` 读地址 `0x101`
- **THEN** 读到的字节 MUST 是 **lane 1** = `0xC3`
- **AND** 结果 MUST 是**零扩展**后的 `0x000000C3`
- **AND** 反向期望（MUST NOT）：lane 恒取 lane 0 ⇒ `0x000000D4`

#### Scenario: LH —— 半字道选择 + 符号扩展

- **WHEN** 内存 `[0x100]` = `0xA1B2C3D4`，执行 `lh` 读地址 `0x102`
- **THEN** 读到的半字 MUST 是该字的**高半字** = `0xA1B2`
- **AND** 结果 MUST 是**符号扩展**后的 `0xFFFFA1B2`
- **AND** 反向期望（MUST NOT）：half 恒取 half0 ⇒ `0xFFFFC3D4`；
      零扩展 ⇒ `0x0000A1B2`；
      符号位填充位置错（高位未补满）⇒ `0xFF00A1B2`

#### Scenario: LHU —— 半字道选择 + 零扩展

- **WHEN** 内存 `[0x100]` = `0xA1B2C3D4`，执行 `lhu` 读地址 `0x102`
- **THEN** 读到的半字 MUST 是**高半字** = `0xA1B2`
- **AND** 结果 MUST 是**零扩展**后的 `0x0000A1B2`
- **AND** 反向期望（MUST NOT）：half 恒取 half0 ⇒ `0x0000C3D4`

#### Scenario: 车道选择的**可观测性**（契约事实）

- **WHEN** 同一字的**不同车道**含有**不同的值**
- **THEN** 读**不同车道** MUST 得到**不同**的值
- **AND** 反向期望（MUST NOT）：读不同车道得到相同值 ⇒ 车道选择未生效
      ⇒ **违反**本 Requirement

#### Scenario: 符号扩展的**可观测性**（契约事实）

- **WHEN** 被选车道的**最高位为 0**
- **THEN** 符号扩展与零扩展的**结果相同**（⇒ 该取值下「符号 vs 零扩展」**不可观测**）
- **AND** 反向期望（MUST NOT）：最高位为 0 的车道被填出**非零**高位
      ⇒ 违反「符号扩展 = 把最高位复制到全部高位」