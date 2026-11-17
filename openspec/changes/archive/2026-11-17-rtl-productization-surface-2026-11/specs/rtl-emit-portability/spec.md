# Capability: rtl-emit-portability

## REMOVED Requirements

### Requirement: 端口清单（manifest）MUST 遵循 `rtl-emit-manifest-v1` schema

**Reason**: schema 升到 **v2** —— 端口清单 MUST 携带**产品端口表**，否则消费方无法从清单
判定**端口面**（v1 只有 `is_top_io`，其语义是「**根组件自有 IO**」⇒ ⇒ 与产物的顶层端口面
**不等**：后代端口在产物里是顶层端口、在 v1 里标 `false`）。本 Requirement 的标题内含
schema 版本号 ⇒ ⇒ 标题**必变** ⇒ ⇒ MODIFIED 改不了标题 ⇒ ⇒ 只能 REMOVED + ADDED。

**Migration**: 消费方 MUST 按 `schema` 字段分支；读到 v2 清单时 MUST 用 `port_table[]`
判定端口面 ⇒ ⇒ MUST NOT 继续把 `is_top_io == true` 当端口面定义。`ports[]` 仍是**全量**
节点清单（10 字段不变）⇒ ⇒ 依赖 `owner` / `driven` / `effective` 的诊断逻辑零改动。

## ADDED Requirements

### Requirement: 端口清单（manifest）MUST 遵循 `rtl-emit-manifest-v2` schema，且 MUST 携带产品端口表

一个 RTL 生成器产出的**端口清单**（manifest）MUST 是一个可机读的文件
⇒ ⇒ 其**每个端口条目** MUST 携带**全部 10 个字段**（**零字段缺失**）⇒ ⇒ 字段表见下方 Scenario
⇒ ⇒ **并且** manifest MUST 携带**产品端口表** `port_table[]`（**顶层数组**）⇒ ⇒
每项 MUST 为 `{name, dir, width, role}`，**按产物端口顺序**排列，且与产物的端口表
**逐字逐序**相同。

⇒ ⇒ schema 名 MUST 是 `rtl-emit-manifest-v2`
⇒ ⇒ `role` 的取值 MUST **只有** `clock` / `reset` / `data` 三值
⇒ ⇒ **端口面的唯一权威 MUST 是 `port_table[]`** ⇒ ⇒ 发射器 MUST 按它产端口面。

#### Scenario: manifest 条目携带全部 10 个字段

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
  | `semantic` | string \| null | **可选** · v2 **无生产者** |

- **AND** MUST NOT 省略其中任一字段（省略 ⇒ 等于「消费方无法判别该端口」）

#### Scenario: manifest MUST 携带产品端口表

- **WHEN** 一个生成器产出一个遵循本 schema 的 manifest
- **THEN** manifest MUST 含**顶层数组** `port_table[]`
- **AND** `port_table[]` 的每一项 MUST 只含 `name` / `dir` / `width` / `role` 四个字段
- **AND** `port_table[]` 的**顺序** MUST 等于产物里顶层端口的**发射顺序**
- **AND** `port_table[]` 的每个条目 MUST 与产物里同位置的端口**逐字逐序**相同
      （名字 / 方向 / 位宽 / 角色四项全等）

#### Scenario: `ports[]` MUST 仍列全量（v2 不收敛旧字段）

- **WHEN** 一个生成器产出一个遵循本 schema 的 manifest
- **THEN** `ports[]` MUST 仍列出**全部**端口条目（含非顶层端口）
- **AND** MUST NOT 把 `ports[]` 削减为**产品端口**子集
      （⇒ ⇒ 节点清单用途 `owner` / `driven` / `effective` 的诊断面会丢失）

#### Scenario: 端口面的唯一权威是产品端口表

- **WHEN** 一个**发射器**决定产物包含哪些顶层端口、以什么顺序
- **THEN** 该决定 MUST 以 `port_table[]` 为准
- **AND** MUST NOT 以 `is_top_io` 为端口面的定义
      （`is_top_io` 的语义是「根组件自有 IO」⇒ ⇒ 与顶层端口面**不等**）

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
- **AND** MUST NOT 被用作**端口面的定义**（⇒ ⇒ 端口面的唯一权威是 `port_table[]`）
- **AND** 发射器 MUST 按 `port_table[]` 产端口面 ⇒ ⇒ MUST NOT 因 `is_top_io`
  而改变端口面或端口顺序

#### Scenario: manifest 不定义端口面与命名规则

- **WHEN** 一个实现产出遵循本 schema 的 manifest
- **THEN** 该实现 MUST NOT 因此**新增或删除**任何端口
- **AND** MUST NOT 因此改变端口的**顺序**
- **AND** MUST NOT 在本 schema 内定义**命名规则**（命名规则由其规范所有者单独规定）
- **AND** ⚠️ 本 Scenario 约束的是**产物不变**（「产 manifest」这一动作 MUST NOT 改变产物）
      ⇒ ⇒ **不是**「manifest 的字段不能描述端口面」：`port_table[]` 是**对产物的描述**，
      MUST NOT 被解读成**对产物的定义**

#### Scenario: `semantic` 在 v2 无生产者

- **WHEN** 一个生成器产出遵循本 schema 的 manifest
- **THEN** 每个端口条目的 `semantic` MUST 为 `null`
- **AND** MUST NOT 由本 schema 的实现方**臆造** `semantic` 取值
