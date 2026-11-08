# Capability: rtl-hierarchical-output（delta · ADR-066 决策 ③）

> **修订单条 Requirement**（标题与主 spec **逐字一致**）⇒ ⇒ 只改两处：
> ① 其「父层消费子输出 MUST 有连接网」Scenario ⇒ **收窄**为「消费点**不是**父 `output` 端口时」；
> ② **新增** Scenario「父侧带运算 MUST 合成父侧网」。
> 其余 Scenario **逐字保留**（MODIFIED 语义 = 整条 Requirement 被替换 ⇒ 必须完整列出）。

## MODIFIED Requirements

### Requirement: 成层且非根的 module 端口 MUST 覆盖自身 `ch_in` / `ch_out`，且每个端口可追溯

- **MODIFIED**：拒绝的可观测形态按 ADR-058 §2.3 ⑤ = **错误结果**（`ok = false` +
  `error` 点名 + `files` 为空），与「发射器内 MUST NOT 依赖异常」口径一致。
- **MODIFIED（ADR-066 决策 ③ · 2026-11-08）**：「造网」义务**收窄**为
  「父侧消费点**不是**父 `output` 端口」时 ⇒ ⇒ 消费点**就是**父 `output` 端口时
  MUST NOT 强制造网 ⇒ ⇒ 唯一权威 = `docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md`
  决策 ③。

**成层且非根**的 module，其端口集合 MUST **⊇** 它自己的 `ch_in` ∪ `ch_out`
⇒ ⇒ **是否已被父层驱动不影响它是否成为端口**（emitter MUST 忽略 `inputimpl::driver()`）。
且每个端口 MUST 可追溯到五类来源之一：自身端口 · inline 后代的 `ch_out`（**仅根允许**）·
隐式端口 · 冒泡端口 · **时序基础设施**（clock/reset）。

⇒ ⇒ **RTL 顶层 module MUST 走扁平规则**，MUST NOT 套用本条。
⇒ ⇒ **自驱动**：成层组件自己驱动自己的 `ch_in` ⇒ MUST **返回 `ok = false`** 并点名该端口。

#### Scenario: 已被父层驱动的 `ch_in` MUST 仍是 module 端口

- **WHEN** 一个成层组件的 `ch_in` 由父层表达式驱动
- **THEN** 该 `ch_in` MUST 出现在该组件的 module 端口表里
- **AND** 子 module 内 MUST **NOT** 出现 `assign <该输入> = ...`
- **AND** 父层的驱动 MUST 表现为**实例端口连接**

#### Scenario: 父层消费子输出：造网义务按消费点性质区分（收窄 · ADR-066 决策 ③）

- **WHEN** 父 module 消费一个成层子组件的 `ch_out`
- **THEN** 若**消费点不是**父 module 的 `output` 端口（父内运算 / 父内 reg / 子 input 的表达式）
      ⇒ 父 module MUST 为该端口声明一根 `logic` 网 · MUST 由 `.port(net)` 驱动
      · MUST NOT 用 `assign` 接表达式
- **AND** 若**消费点就是**父 module 的 `output` 端口（直通）
      ⇒ MUST NOT 强制造网（子 output MAY 直接作为该 `.port(...)` 的实参）
- **AND** ⚠️ 该「必须造网」的要求 MUST NOT 施加到**接 input 的** `.port(...)`（可为表达式）

#### Scenario: 父侧带运算 MUST 合成父侧网且操作数 MUST 读该网（新增 · ADR-066 决策 ①）

- **WHEN** 父层对成层子组件的 `ch_out` 的使用**带运算 / 表达式**（消费点不是父 `output` 端口）
- **THEN** 发射 MUST 成功（`ok = true`）且 MUST 产出产物
- **AND** 父 module **作用域内** MUST 声明一根 `logic` 网，子 output MUST 以 `.port(net)` 连接该网
- **AND** 父侧运算的操作数 MUST 读**该网**（⇒ 网名 MUST = **驱动端点的 ctx 全局名**；
      MUST NOT 用被驱动节点自体名 ⇒ 否则自指 + 双驱动）
- **AND** 该形态 MUST 与扁平产物**逐拍等价**

#### Scenario: 父侧运算消费子非端口端点 MUST 读父侧落点网名（新增 · ADR-066 决策 ②）

- **WHEN** 父层对子组件的**非端口端点**（`ch_reg` / bundle 字段 / op 节点）的使用带运算
- **THEN** 父侧表达式的标识符 MUST 是**父 module 作用域内可解析**的名字
      （= 父侧落点网名）
- **AND** MUST NOT 引用子 module 的**内部信号名**，也 MUST NOT 直接引用子 module 的**端口名**
      （⚠️ 子侧合成端口名 = `__x_<n>`（ADR-060 §2.2 v1.8）⇒ ⇒ **端口名 ≠ 网名**）
- **AND** 产物 MUST 通过 ADR-060 §2.5 的 lint 门（零 `%Error`）

#### Scenario: 自驱动 ch_in MUST 被拒绝

- **WHEN** 一个成层组件在自己的体内驱动自己的 `ch_in`
- **THEN** emitter MUST **返回 `ok = false`**，`error` MUST 点名该 `ch_in`
- **AND** `files` MUST 为空
- **AND** MUST NOT 在子 module 内对该 `input` 发 `assign`

#### Scenario: RTL 顶层 module MUST 走扁平规则

- **WHEN** 一个设计里 RTL 顶层 module 的下方有被 inline 的子组件
- **THEN** 顶层端口表 MUST 是 clock → reset → 无 driver 的 `input` → **全部** `ch_out`
      （⇒ 含 inline 后代的 `ch_out`）
- **AND** 该端口表 MUST 与扁平产物**逐字逐序相同**

#### Scenario: 非根 module MUST NOT 抬 inline 后代的 `ch_out`

- **WHEN** 一个**非顶层**成层 module 内含被 inline 的子组件
- **THEN** 该子组件的 `ch_out` MUST 是该 module 体内的普通信号
- **AND** MUST NOT 出现在该 module 的端口表里

#### Scenario: 未被消费的子 output 端口 MUST 以空连接出现

- **WHEN** 一个成层子 module 的某个 `ch_out` 端口**未被父层消费**
- **THEN** 该端口 MUST 仍然是该子 module 的 `output` 端口（⇒ 端口表**不收缩**）
- **AND** 父 module 的实例化语句 MUST 以**空连接**（`.port()`）接上它
- **AND** 该产物 MUST 通过 **ADR-060 §2.5 的 lint 门**（退出码 + 无 `%Error`；
      ⚠️ 空连接在默认集下本就合法 ⇒ MUST NOT 靠放宽门来通过）
- **AND** ⚠️ **子 input 端口 MUST NOT** 用空连接糊过去 ⇒ ⇒ 未驱动的子 input MUST
  **冒泡**成顶层 input 并由父层接上（⇒ ⇒ 否则其值是未定义值 X）
