# Capability: bundle-master-slave-io

## ADDED Requirements

### Requirement: bundle 的 prefix 命名 MUST NOT 在基类构造期触碰派生类成员

MUST NOT 在**基类构造期间**调用虚函数去触碰
派生类的成员（尤其是**嵌套 bundle 字段**）—— 那一刻派生类成员**尚未构造**
（仍是未初始化内存）⇒ 对其调用虚函数是未定义行为。

命名 MUST **推迟到字段节点建立的那一刻**（`as_master()` / `as_slave()`），
此时派生类成员已构造、字段节点正要建立 ⇒ 命名与建节点同时完成。

#### Scenario: 带 prefix 构造的嵌套 bundle MUST NOT 崩溃

- **WHEN** 一个 bundle 含**嵌套 bundle 字段**（如 `ch_stream` 成员），
  且以 `prefix` 构造
- **THEN** 构造 MUST NOT 崩溃（未定义行为）
- **AND** 字段的节点名 MUST 带上该 prefix

#### Scenario: 命名 MUST 幂等

- **WHEN** 同一 bundle 先 `as_master()` 后 `as_slave()`（或反之）
- **THEN** 字段的节点 MUST 保持同一个节点
- **AND** 字段名的 prefix MUST 不被重复拼接

### Requirement: 嵌套 bundle 字段的连接语义 MUST 有明确契约

MUST NOT 继续依赖「字段由总线切片派生」这一已被 ADR-042 取代的模型。
适用设施：`bundle_concat` / `bundle_slice_view` /
`deserialize_bits_to_fields()`。

每个此类设施 MUST 满足**二者之一**：

- **实现正确语义**：拷贝后**逐字段建立数据流边**（与 `bundle_base::operator<<=`
  的方向感知逻辑一致）⇒ 副本真正「连着」原件；或
- **显式废弃**：标记 deprecated 并从公开派生类清单中移除

⇒ MUST NOT 留一个「看起来能用、实则静默失效」的公开类型。

#### Scenario: 嵌套 bundle 的副本 MUST 与原件保持连接

- **WHEN** 一个含嵌套 bundle 的结构被拷贝 / 视图化
- **THEN** 副本的嵌套字段 MUST 与原件的对应字段**保持数据流连接**
- **AND** 对原件字段的驱动 MUST 能被副本读到

#### Scenario: 序列化路径 MUST NOT 用切片重绑定字段

- **WHEN** `deserialize_bits_to_fields()` 把位串还原成字段
- **THEN** 每个字段 MUST 拥有**自己的**节点（与「字段一等节点」原则一致）
- **AND** MUST NOT 把字段重绑定成源位串的 `bit_select` / `bits<>` 切片

### Requirement: `flip()` MUST 返回方向取反的副本

MUST 返回一个**方向取反**的副本（master→slave，
slave→master），MUST NOT 硬编码 `as_slave()`。

返回的副本 MUST 是**未连接**的（调用方负责连接）。

#### Scenario: flip 的方向取反

- **WHEN** 对一个 master bundle 调用 `flip()`
- **THEN** 结果 MUST 是 slave 方向
- **AND** 对一个 slave bundle 调用 `flip()`，结果 MUST 是 master 方向

### Requirement: `master()` / `slave()` 工厂 MUST 产出独立 bundle

MUST 产出**节点独立**的 bundle（字段节点不与源 bundle
共享），并按 master / slave 设定方向。

#### Scenario: 工厂产物的字段节点 MUST 独立

- **WHEN** `master(src)` / `slave(src)` 产出副本
- **THEN** 副本的每个字段节点 MUST **不等于**源 bundle 的对应字段节点
- **AND** 副本的方向 MUST 按 master / slave 设定

### Requirement: `operator=` 的「软件拷贝」语义 MUST 被显式记录

MUST 保留 `bundle_base::operator=`（以及普通信号字段的 `operator=`）为
**显式的软件拷贝 / 节点重绑定**语义 —— 它**不是**连接手段。

该语义 MUST 在 capability 中写明，MUST NOT 仅由测试注释钉住。

#### Scenario: `operator=` 不建立连接

- **WHEN** 用 `operator=` 把一个 bundle 的字段赋给另一个 bundle 的字段
- **THEN** 目标字段 MUST 与源字段**共享节点**（软件拷贝语义）
- **AND** 该行为 MUST NOT 被误解为「建立了连接」

### Requirement: bundle 的连接语义 MUST 唯一

MUST NOT 同时存在两套互相冲突的字段连接语义。

`ch_stream` 等**自带自由 `operator<<=`** 的 bundle MUST 收敛到**数据流边**语义；
若因协议特性（如 ready 的反向流动）无法直接收敛，则 MUST **显式写明差异**并
提供**双后端 parity 覆盖**。

#### Scenario: ch_stream 的连接 MUST 与其他 bundle 一致

- **WHEN** `ch_stream` 的两个实例被连接
- **THEN** 连接 MUST 通过**数据流边**建立（MUST NOT 用节点重绑定）
- **AND** 该行为 MUST 有双后端 parity 覆盖

### Requirement: bundle 的边角设施 MUST 有可执行断言

MUST 为下列每一类边角设施提供至少一个 Catch2 断言：嵌套 bundle 字段、
`flip()`、工厂函数、序列化路径、`ch_stream` 的连接语义。

#### Scenario: 边角设施的断言存在且非空转

- **WHEN** reviewer 检查 bundle 能力的测试覆盖面
- **THEN** 上述每一类 MUST 至少有一个对应断言
- **AND** 对「连接退回节点重绑定」「flip 方向硬编码」「工厂共享节点」
  「deserialize 退回切片」这四类改动，相应断言 MUST 失败（证明非空转）

### Requirement: 打包式 bundle API 的适用范围 MUST 写明

`get_bundle_value()` / `set_bundle_value()` 保留，但其**适用范围** MUST 写明：
仅适用于**未连接**的 bundle（已连接字段的值由设计每拍求值产生，TB 的写入会被
下一拍覆写）。

#### Scenario: 已连接字段的 TB 写入 MUST NOT 被当作契约

- **WHEN** 文档 / spec 描述打包式 API 的用途
- **THEN** MUST 明确它只用于**未连接**的 bundle
- **AND** MUST NOT 让用户以为它能驱动已连接的设计逻辑
