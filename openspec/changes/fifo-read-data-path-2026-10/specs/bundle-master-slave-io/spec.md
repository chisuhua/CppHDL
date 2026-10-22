# Capability: bundle-master-slave-io

## MODIFIED Requirements

### Requirement: bundle 字段的方向指认 MUST 与协议语义一致

一个 bundle 的 `as_master_direction()` / `as_slave_direction()` MUST 为每个字段
指认**协议上正确**的一侧：master 拥有该总线的**发起方**（命令 / 写数据），
slave 拥有**响应方**（状态 / 读数据）。

> ⚠️ 「镜像」只是**必要条件**，不是充分条件：镜像但**两侧指认反了**同样是错的
> （今天的 `fifo_bundle` 就是这样 —— 5 个字段里 4 个已镜像，唯独 `push` 放错边）。

> ⚠️ **本段文字按「`fifo_bundle` 增加独立的读数据字段」起草**（形态 A）。
> 该形态 MUST 由对应的 ADR 逐项裁决；若 ADR 裁决了其它形态，本段 MUST 随之
> 订正（**本 Requirement 的标题与主 spec 逐字一致** ⇒ 改动只会体现在正文）。

#### Scenario: fifo_bundle 的方向指认

- **WHEN** 检查 `fifo_bundle` 的 `as_master_direction()` / `as_slave_direction()`
- **THEN** `data`（写数据）/ `push` / `pop` MUST 在 **master** 侧为 output、
  **slave** 侧为 input
- **AND** `full` / `empty` MUST 在 **master** 侧为 input、**slave** 侧为 output
- **AND** 该指认 MUST 与 `config_bundle` / `interrupt_bundle` 的同构关系一致

#### Scenario: 读数据 MUST 有**独立**的字段

- **WHEN** 一个 bundle 的协议既有「写数据」又有「读数据」（如 `fifo_bundle`）
- **THEN** 读数据 MUST 拥有**独立的字段**
- **AND** MUST NOT 与写数据**共用**同一个字段
  （共用 ⇒ slave 侧无法输出读到的数据 ⇒ 该协议的**读通路不存在**）
- **AND** 该字段 MUST 在 **master** 侧为 input、**slave** 侧为 output

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
