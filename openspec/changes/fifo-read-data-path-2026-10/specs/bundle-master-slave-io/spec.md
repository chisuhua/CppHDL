# Capability: bundle-master-slave-io

## MODIFIED Requirements

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
