## ADDED Requirements

### Requirement: 宽度适配类组合子 MUST 可被仿真且 ready 所有权与转发类同型

- **WHEN** 使用 `chlib` 的 stream 宽度适配组合子
      （`stream_narrow_to_wide` / `stream_wide_to_narrow` / `stream_width_adapter`）
- **THEN** 它们 MUST **构建出可实例化的图**（构造 `Simulator` MUST NOT 崩溃）
- **AND** 其 ready 所有权 MUST 与「转发类组合子的 ready 所有权 MUST 是**默认值 +
      可覆盖**」**完全相同**（形态 E + 节点持久 + 用 `<<=` 驱动上游 ⇒ **不重述**，
      见该 Requirement）
- **AND** 输出 `valid` / `payload` MUST 是**寄存器**（⇒ `tick()` 后即可观测）

#### Scenario: 三个适配器可被仿真

- **WHEN** 用任意合法位宽组合构造三个适配器并创建 `Simulator`
- **THEN** MUST NOT 崩溃（`SIGSEGV`）
- **AND** 上游 `ready` MUST 由**调用方持有的 handle** 观测到（⇒ 调用方 MUST
      先 `as_master()`，否则 `ch_stream::ready` 无节点 ⇒ 组合子的驱动无处置放）
- **NOTE**: 「输出 MUST 是寄存器」是**可测试性约束**，不是实现偏好 ——
      组合输出在本仓 `set_input → tick → get_port_value` 读模型下**不可观测**
      （读到沿后组合值，而正确值出现在沿前）⇒ 写不出有效的 parity 判据
      ⇒ 见 ADR-064 D3 / §1.4。

#### Scenario: 累积型适配器的宽字 MUST 含最后一个窄字且组间隔离

- **WHEN** `stream_narrow_to_wide<ch_uint<32>, ch_uint<8>>` 连续接收
      `0xF0, 0xE1, 0xD2, 0xC3`（下游恒就绪），随后接收 `0x0F, 0x1E, 0x2D, 0x3C`
- **THEN** 第一个宽字 MUST 是 `0xC3D2E1F0`（低字节先到）
- **AND** 第二个宽字 MUST 是 `0x3C2D1E0F`
- **AND** 第二个宽字 MUST NOT 含第一组的任何位 ⇒ 累积器 MUST 在组完成时**清零**
- **NOTE**: 测试数据 MUST 满足「组 1 字节 **或** 组 2 字节 ≠ 组 2 字节」——
      否则按位或会**掩盖**跨组污染（实测 `0xA0 | 0xB0 == 0xB0` ⇒ 该变异恒绿）。

#### Scenario: 背压 MUST 到达上游且不丢已产生的宽字

- **WHEN** 宽字已装载（`valid=1`）而下游 `ready=0`
- **THEN** 上游 `ready` MUST 为 `0`
- **AND** 输出的 `valid` / `payload` MUST 保持不变（MUST NOT 被新数据覆盖）
- **AND** 下游恢复 `ready=1` 后，未取走的宽字 MUST 仍可被取走（MUST NOT 丢失）

#### Scenario: 拆分型适配器 MUST 按低字节优先输出

- **WHEN** `stream_wide_to_narrow<ch_uint<8>, ch_uint<32>>` 接收 `0xA1B2C3D4`
- **THEN** 依次输出 `0xD4` / `0xC3` / `0xB2` / `0xA1`
- **AND** 输出 `valid` MUST 保持到 `ready`，且输出满时上游 `ready` MUST 为 `0`
