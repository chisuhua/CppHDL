## ADDED Requirements

### Requirement: `miss` 期间的地址稳定性

`miss` 由 1 拍 `sread` 推导（上一拍采样的地址），`axi_addr` 取当拍地址 ⇒ 两者 1 拍错位
⇒ **调用方 MUST 在 `miss` 期间保持 `addr` 不变**。

#### Scenario: 遵守约束

- **WHEN** 调用方在 `miss` 期间保持 `addr` 不变
- **THEN** `axi_addr` MUST 等于 `addr & 0xFFFFFFF0`（块地址）
- **AND** MUST 与 `miss` 所指的那次采样一致

#### Scenario: 违反约束的后果可观测

- **WHEN** 调用方在 `miss` 期间改变 `addr`
- **THEN** `axi_addr` MUST 跟随当拍地址
- **AND** MUST 不再等于 `miss` 所指采样的块地址 ⇒ 取回的是新块的数据
