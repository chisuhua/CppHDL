# rtl-emit-portability Specification

## Purpose
TBD - created by archiving change rtl-emit-manifest-schema-2026-11. Update Purpose after archive.

## Requirements

### Requirement: 端口清单（manifest）MUST 遵循 `rtl-emit-manifest-v1` schema

一个 RTL 生成器产出的**端口清单**（manifest）MUST 是一个可机读的文件
⇒ ⇒ 其**每个端口条目** MUST 携带 `rtl-emit-manifest-v1` 规定的**全部 10 个字段**
（**零字段缺失**）⇒ ⇒ 字段表见下方 Scenario
⇒ ⇒ `role` 的取值 MUST **只有** `clock` / `reset` / `data` 三值
⇒ ⇒ `semantic` 在 v1 MUST 恒为 `null`（字段位保留，**无生产者**）。

#### Scenario: manifest 条目携带全部字段

- **WHEN** 一个生成器产出一个 manifest
- **THEN** 每个端口条目 MUST 同时携带下列**全部 10 个字段**：

  | 字段 | 类型 | 语义 |
  |---|---|---|
  | `name` | string | 端口名（**RTL 全名**，即产物里的名字） |
  | `dir` | enum | `input` / `output` |
  | `width` | uint | 位宽（bit） |
  | `role` | enum | `clock` / `reset` / `data` |
  | `driven` | bool | **节点级**：该端口**是否被消费** |
  | `effective` | bool | **功能级**：驱动它**是否产生可观测行为** |
  | `owner` | string | **所属组件路径**（组件实例全路径） |
  | `is_top_io` | bool | 是否**根组件自有 IO**（**不是** inline 后代输出） |
  | `cpp_member` | string | **C++ 成员路径** |
  | `semantic` | string \| null | **可选** · v1 **无生产者** |

- **AND** MUST NOT 省略其中任一字段（省略 ⇒ 等于「消费方无法判别该端口」）

#### Scenario: `driven`（节点级）与 `effective`（功能级）是两个独立字段

- **WHEN** 一个端口**有连线**（节点级有消费者）**但**驱动它**不产生任何可观测行为**
- **THEN** 该端口的 `driven` MUST 为 `true`
- **AND** 该端口的 `effective` MUST 为 `false`
- **AND** MUST NOT 用**单个**布尔字段表达这两种状态（⇒ ⇒ 「有连线但无功能」这一类
  MUST 有独立的表达位）

#### Scenario: 「有连线但无功能」的标准样例

- **WHEN** 评估 `ext_clk`（SoC 顶层时钟输入：接到 `pipeline.io().clk`，而流水线内
      对 `io().clk` **零消费者**）
- **THEN** 它的 `driven` MUST 为 `true`（**节点级确有消费者**）
- **AND** 它的 `effective` MUST 为 `false`（**功能级零可观测行为**）

#### Scenario: `driven` 的判定 MUST 复用既有驱动诊断

- **WHEN** 生成器判定某端口的 `driven`
- **THEN** MUST 复用仓内既有的驱动状态诊断（`diagnose_drive()`）
- **AND** MUST NOT 另造一套驱动状态判定（两套判定 ⇒ ⇒ 必然漂移）

#### Scenario: `is_top_io` 的筛选是消费方的选择

- **WHEN** 一个下游工具（如 TB 生成器）按 `is_top_io == true` 筛选端口
- **THEN** 该筛选 MUST 只影响**该消费方自身**的行为
- **AND** MUST NOT 成为**发射器**的默认行为（⇒ ⇒ 发射器 MUST NOT 因 `is_top_io`
  而改变端口面或端口顺序）

#### Scenario: manifest 不定义端口面与命名规则

- **WHEN** 一个实现产出遵循本 schema 的 manifest
- **THEN** 该实现 MUST NOT 因此**新增或删除**任何端口
- **AND** MUST NOT 因此改变端口的**顺序**
- **AND** MUST NOT 在本 schema 内定义**命名规则**（命名规则由其规范所有者单独规定）

#### Scenario: `semantic` 在 v1 无生产者

- **WHEN** 一个生成器产出遵循本 schema 的 manifest
- **THEN** 每个端口条目的 `semantic` MUST 为 `null`
- **AND** MUST NOT 由本 schema 的实现方**臆造** `semantic` 取值
