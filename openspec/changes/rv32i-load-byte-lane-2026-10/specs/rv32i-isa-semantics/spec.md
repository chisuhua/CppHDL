# 变更规格：RV32I load 字节道语义（LB / LH / LBU / LHU）

## ADDED Requirements

### Requirement: load 字节道 MUST 按地址选车道并按 `funct3` 扩展

- **ADDED**: 既有「字节道**写**」条款的**成对**条款（写 = lane 掩码；读 = 车道选择 + 扩展）。
  本次是**补契约**而非**改行为** ⇒ 既有固件的 golden **零变化**。
  实现依据：`mem_stage.h:82-163`（load 路径**已实现**）。

- **WHEN** 一条 load 指令（`LB`/`LH`/`LBU`/`LHU`）读回一个**字**
- **THEN** MUST 按**地址低位**选出被读车道（`LB`/`LBU`：`addr[1:0]`；`LH`/`LHU`：`addr[1]`）
- **AND** MUST 按 `funct3` 扩展（`LB`/`LH` 符号、`LBU`/`LHU` 零）
- **AND** 未被选中的车道 MUST NOT 影响结果
- **NOTE**: `LW` 取整字、不扩展（既有行为，不在本条范围）

#### Scenario: LB —— 字节道选择 + 符号扩展

- **WHEN** 内存 `[0x100]` = `0xA1B2C3D4`，执行 `lb` 读地址 `0x101`
- **THEN** 读到的字节 MUST 是该字的 **lane 1** = `0xC3`
- **AND** 结果 MUST 是**符号扩展**后的 `0xFFFFFFC3`
- **AND** 反向期望（MUST NOT）：lane 恒取 lane 0 ⇒ `0xFFFFFFD4`；
      零扩展 ⇒ `0x000000C3`

#### Scenario: LBU —— 字节道选择 + 零扩展

- **WHEN** 内存 `[0x100]` = `0xA1B2C3D4`，执行 `lbu` 读地址 `0x101`
- **THEN** 读到的字节 MUST 是 **lane 1** = `0xC3`
- **AND** 结果 MUST 是**零扩展**后的 `0x000000C3`
- **AND** 反向期望（MUST NOT）：lane 恒取 lane 0 ⇒ `0x000000D4`

#### Scenario: LH —— 半字道选择 + 符号扩展

- **WHEN** 内存 `[0x100]` = `0xA1B2C3D4`，执行 `lh` 读地址 `0x102`
- **THEN** 读到的半字 MUST 是该字的**高半字** = `0xA1B2`
- **AND** 结果 MUST 是**符号扩展**后的 `0xFFFFA1B2`
- **AND** 反向期望（MUST NOT）：lane 恒取 lane 0 ⇒ `0xFFFFC3D4`

#### Scenario: LHU —— 半字道选择 + 零扩展

- **WHEN** 内存 `[0x100]` = `0xA1B2C3D4`，执行 `lhu` 读地址 `0x102`
- **THEN** 读到的半字 MUST 是**高半字** = `0xA1B2`
- **AND** 结果 MUST 是**零扩展**后的 `0x0000A1B2`
- **AND** 反向期望（MUST NOT）：lane 恒取 lane 0 ⇒ `0x0000C3D4`

#### Scenario: 非零 offset MUST 被覆盖（判别力的前提）

- **WHEN** 同一字的**四个字节道**与**两个半字道**分别被读
- **THEN** 每个道的值 MUST **各不相同**（⇒ 用例 MUST 用「每字节都不同」的字，
      如 `0xA1B2C3D4` ⇒ 否则全 0 / 全 F 的字会让 lane 选错**不可观测**）
- **AND** 只读 offset 0 的用例 MUST NOT 被当作本 Requirement 的判别证据
