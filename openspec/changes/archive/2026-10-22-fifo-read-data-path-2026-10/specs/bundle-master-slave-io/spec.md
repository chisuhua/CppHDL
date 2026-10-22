# Capability: bundle-master-slave-io

## MODIFIED Requirements

### Requirement: 打包式 bundle API 的适用范围 MUST 写明

`get_bundle_value()` / `set_bundle_value()` 保留，但其**适用范围** MUST 写明：
仅适用于**未连接**的 bundle（已连接字段的值由设计每拍求值产生，TB 的写入会被
下一拍覆写）。

⚠️ **宽度边界**（ADR-044 裁决 6）：打包式 API 用**一个 `uint64_t`** 表示整个
bundle ⇒ 总宽 > 64 位时**显式拒绝**（MUST NOT 静默截断）。
`fifo_bundle<T>` 在 ADR-044 之后总宽为 `2*W+4` ⇒ **W ≥ 31 时落在该适用范围
之外**（此前是 `W+4` ⇒ W ≤ 60 可用）⇒ 这是一处**能力回退**，MUST 被写明。

#### Scenario: 已连接字段的 TB 写入 MUST NOT 被当作契约

- **WHEN** 文档 / spec 描述打包式 API 的用途
- **THEN** MUST 明确它只用于**未连接**的 bundle
- **AND** MUST NOT 让用户以为它能驱动已连接的设计逻辑

#### Scenario: 打包式 API 的宽度边界 MUST 写明

- **WHEN** 一个 bundle 的总宽超过 64 位
- **THEN** 打包式 API MUST **显式拒绝**（MUST NOT 静默截断）
- **AND** MUST 写明**逐字段**的槽位读写路径**不受**该限制
- **AND** MUST 写明 `fifo_bundle<T>` 的 `W ≥ 31` 落在该适用范围之外
  （ADR-044 加 `rdata` 后的总宽是 `2*W+4`）

### Requirement: bundle 字段的方向指认 MUST 与协议语义一致

一个 bundle 的 `as_master_direction()` / `as_slave_direction()` MUST 为每个字段
指认**协议上正确**的一侧：master 拥有该总线的**发起方**（命令 / 写数据），
slave 拥有**响应方**（状态 / 读数据）。

> ⚠️ 「镜像」只是**必要条件**，不是充分条件：镜像但**两侧指认反了**同样是错的
> （`fifo_bundle` 曾经就是这样 —— 5 个字段里 4 个已镜像，唯独 `push` 放错边）。

> ⚠️ **本 Requirement 的措辞 MUST 与 ADR-044 的裁决一致；ADR 未裁决前
> MUST NOT 归档**（delta 会被**逐字**并入主 spec ⇒ 留待定条款 = 把不确定性
> 固化进 spec）。

#### Scenario: fifo_bundle 的方向指认

- **WHEN** 检查 `fifo_bundle` 的 `as_master_direction()` / `as_slave_direction()`
- **THEN** `data`（写数据）/ `push` / `pop` MUST 在 **master** 侧为 output、
  **slave** 侧为 input
- **AND** `full` / `empty` MUST 在 **master** 侧为 input、**slave** 侧为 output
- **AND** `rdata`（读数据）MUST 在 **master** 侧为 input、**slave** 侧为 output
- **AND** 该指认 MUST 与 `config_bundle` / `interrupt_bundle` 的同构关系一致

#### Scenario: 读数据 MUST 有**独立**的字段

- **WHEN** 检查 `fifo_bundle` 的协议（既有「写数据」又有「读数据」）
- **THEN** 读数据 MUST 拥有**独立的字段**
- **AND** MUST NOT 与写数据**共用**同一个字段
  （共用 ⇒ slave 侧无法输出读到的数据 ⇒ 该协议的**读通路不存在**）
- **AND** 该字段 MUST 在 **master** 侧为 input、**slave** 侧为 output

> ⚠️ **作用域**：本 Scenario 只约束 `fifo_bundle`（**单总线、双工数据**的
> 协议）。⚠️ 它**不**适用于**读写分离通道**的 bundle（如 `axi_lite_bundle` /
> `axi4_channel` —— 它们**本来就**有分离的读/写数据通道）⇒ 把本条写成
> 通用规则会让那些 bundle 变成「违规」⇒ 与本 change 的 Non-Goal 1 冲突。

#### Scenario: 方向指认 MUST 可被自动断言

- **WHEN** reviewer 检查一个 bundle 的方向
- **THEN** 该方向 MUST 能被一个 Catch2 断言逐字段枚举并比对
  （MUST NOT 依赖「reviewer 人工对比」这类不可自动化的判据）

#### Scenario: 读数据通路 MUST 有**行为级**覆盖

- **WHEN** reviewer 检查「读数据通路」的测试覆盖面
- **THEN** MUST 存在至少一个断言：TB 推入一个元素后，**读数据侧真的读到该元素**
  的用例
- **AND** MUST NOT 仅以「字段存在」或「宽度正确」充当该通路的覆盖
  （那类断言在**读通路根本不存在**时**同样通过** ⇒ 零信号）

#### Scenario: 读数据的**时序** MUST 与 FIFO 协议一致

- **WHEN** 消费端读取 `fifo_bundle` 的 `rdata`
- **THEN** `rdata` MUST 是 **0-cycle 组合读**（`ch_mem::aread`）⇒ 与生成的
  Verilog `assign dout = mem[rd_ptr];` 逐拍一致
- **AND** 消费者 MUST 在 **pop 沿之前**采样 `rdata`，才能拿到**正在被弹出**的
  那个元素（pop 沿**之后** `rdata` 指向**下一个**队首）
- **AND** FIFO 为空（`empty`）时 `rdata` MUST 被视为 **don't-care**
  ⇒ MUST NOT 被强制为 0（那会用一个**假的**「数据是 0」掩盖 FIFO 空的
  真实状态）
