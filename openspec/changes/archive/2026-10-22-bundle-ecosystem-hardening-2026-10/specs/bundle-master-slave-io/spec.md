# Capability: bundle-master-slave-io

## ADDED Requirements

### Requirement: bundle 的 prefix 命名 MUST NOT 在基类构造期触碰派生类成员，且 MUST 内生于字段节点的建立

MUST NOT 在**基类构造期间**调用虚函数去触碰派生类的成员（尤其是**嵌套 bundle
字段**）—— 那一刻派生类成员**尚未构造** ⇒ 对其调用虚函数是未定义行为。

命名 MUST **内生于「字段节点建立」的那一刻**：`create_field_proxy_nodes()` 创建
字段节点时用「prefix + 字段名」命名，MUST NOT 与建节点分离成另一个步骤。

嵌套 bundle 的 prefix MUST 通过**非虚**通道下推（MUST NOT 经由虚函数派发）。

> ℹ️ 「非虚下推」是为了**语义统一**（派生类可 override `set_name_prefix()` 且未必
> 下推 prefix）；「命名内生于建节点」同时给出**幂等**与**无空窗**两条性质。
> 详细理由见本 change 的 proposal（D1）。

#### Scenario: 带 prefix 构造的嵌套 bundle MUST NOT 崩溃

- **WHEN** 一个 bundle 含**嵌套 bundle 字段**（如 `ch_stream` 成员），
  且以 `prefix` 构造
- **THEN** 构造 MUST NOT 崩溃（未定义行为）
- **AND** MUST NOT 对尚未构造完成的派生类成员派发虚调用

#### Scenario: 字段节点名 MUST 带 prefix

- **WHEN** 一个 bundle 以 `prefix` 构造，**且其角色已被设定**
  （`as_master()` / `as_slave()` 至少调用过一次）
- **THEN** 每个叶子字段的节点名 MUST 以 `prefix.` 开头
- **AND** 嵌套 bundle 的叶子节点名 MUST 含**完整**的层级前缀
  （形如 `outer.inner.field`）

#### Scenario: 命名 MUST 幂等

- **WHEN** 同一 bundle 先 `as_master()` 后 `as_slave()`（或反之）
- **THEN** 字段的节点 MUST 保持同一个节点
- **AND** 字段名的 prefix MUST 不被重复拼接

#### Scenario: 事后改 prefix MUST NOT 改名已建立的节点

- **WHEN** 一个字段节点**已经建立**（角色已设定）之后，prefix 被改成另一个值
- **THEN** 已建立的字段节点名 MUST **保持不变**
- **AND** 该变更 MUST 只影响**此后**新建的字段节点

#### Scenario: 拷贝构造 MUST 保留 prefix

- **WHEN** 一个以 `prefix` 构造的 bundle 被拷贝构造，且副本随后设定角色
- **THEN** 副本的叶子字段节点名 MUST 同样带该 prefix

#### Scenario: 角色从未设定的 bundle MUST NOT 有字段节点

- **WHEN** 一个 bundle 从未被赋予角色（`as_master()` / `as_slave()` 均未调用）
- **THEN** 它的字段 MUST 尚未拥有节点
- **AND** prefix 在该路径上 MUST NOT 产生任何可观测效果
  （没有节点可命名）

### Requirement: `bundle_concat` / `bundle_slice_view` MUST 被移除

这两个公开类型 MUST NOT 以「看起来能用、实则静默失效」的形态继续留在公开派生类
清单里。

`bundle_concat` 的字段仍依赖「字段由总线切片派生」这一已被 ADR-042 取代的模型 ⇒
ADR-042 之后它们的字段与原 bundle **断连** ⇒ 静默失效。

`bundle_slice_view` 的 `Start` / `Count` **从未被使用** ⇒ 它**根本不做切片**，其宽度
等于整个原 bundle 的宽度 ⇒ 「让它做切片」属**新增能力**，不在本能力范围内。

⇒ 二者 MUST 被**移除**（MUST NOT 仅标记 deprecated 后继续留在清单里 —— 那会让
「已废弃但仍计入清单」与本能力的清单要求并存矛盾）；公开工厂
`bundle_cat()` / `bundle_slice()`（二者是返回这两个类型的**唯一入口**）
MUST 一并移除，否则调用方一使用即触发 deprecation 告警。

移除 MUST 走 ADR 流程（公开 API 变更）。

#### Scenario: 移除后的公开清单 MUST NOT 含这两个类型

- **WHEN** reviewer 检查 capability 记录的派生类清单
- **THEN** 该清单 MUST NOT 含 `bundle_concat` / `bundle_slice_view`
- **AND** MUST NOT 含返回它们的公开工厂 `bundle_cat()` / `bundle_slice()`

#### Scenario: 移除 MUST 走 ADR 流程

- **WHEN** 公开 API 因移除而消失
- **THEN** MUST 有对应的 ADR 记录该决策与迁移路径

### Requirement: `deserialize_bits_to_fields()` MUST 产出字段一等节点

MUST NOT 把字段重绑定成源位串的 `bit_select` / `bits<>` 切片；每个字段 MUST 拥有
**自己的**节点。

理由：该函数是**活的**路径（经 `deserialize<>()` 被 TLM 转换器与 bundle 序列化测试
消费）⇒ 它 MUST 与「字段一等节点」原则一致，否则「只读不写的字段恒为 0」这条缺陷
会从序列化路径重新长出来。

> ℹ️ 既有断言的形态：该路径的现有测试断言**全是宽度断言**
> （`tests/test_bundle_serialization.cpp`、`tests/test_tlm_bundle_phase1.cpp` 中
> `.width ==` / `bundle_width_v<>` 形式，**零** `impl()` 身份断言）⇒ 它们对
> 「切片重绑定」与「字段一等节点」两种语义**同样通过** ⇒ 既**不需要**反转，
> 也**证明**该路径此前**零行为覆盖** ⇒ MUST **新增**行为级断言。

#### Scenario: 序列化路径 MUST NOT 用切片重绑定字段

- **WHEN** `deserialize_bits_to_fields()` 把位串还原成字段
- **THEN** 每个字段 MUST 拥有**自己的**节点（与「字段一等节点」原则一致）
- **AND** MUST NOT 把字段重绑定成源位串的 `bit_select` / `bits<>` 切片

#### Scenario: 反序列化后的字段 MUST 可独立寻址

- **WHEN** 反序列化产出的 bundle 的某个字段被读取
- **THEN** 该字段 MUST 在 `data_map` 中有槽位（可寻址）
- **AND** MUST NOT 出现「Value not found for signal node ID」告警

#### Scenario: 反序列化后的字段 MUST 有行为级覆盖

- **WHEN** reviewer 检查该路径的测试覆盖面
- **THEN** MUST 存在**至少一个**断言「反序列化产出的字段节点 != 源位串的切片节点」
  的用例
- **AND** MUST NOT 仅以宽度断言充当该路径的覆盖

### Requirement: `flip()` MUST 返回方向取反的副本

MUST 返回一个**方向取反**的副本（master→slave，
slave→master），MUST NOT 硬编码 `as_slave()`。

**未设定角色**（`role_ == unknown`）的 bundle MUST 按 **unknown→slave** 处理。

> ℹ️ 该裁决的**行为影响**：现存 8 个 `flip()` 调用点里，**2 个**翻转的是已设定
> `master` 角色的 bundle（`samples/axi_lite_demo.cpp:171`、
> `tests/test_bundle_connection.cpp:166`），**6 个**翻转的是从未设定角色的 bundle
> ⇒ unknown→slave 让这 8 处**全部保持既有行为**（零行为变化）。

返回的副本 MUST 是**未连接**的（调用方负责连接）。

#### Scenario: flip 的方向取反

- **WHEN** 对一个 master bundle 调用 `flip()`
- **THEN** 结果 MUST 是 slave 方向
- **AND** 对一个 slave bundle 调用 `flip()`，结果 MUST 是 master 方向

#### Scenario: 未设定角色的 bundle flip MUST 走 unknown→slave

- **WHEN** 对一个从未调用过 `as_master()` / `as_slave()` 的 bundle 调用 `flip()`
- **THEN** 结果 MUST 是 slave 方向
- **AND** MUST NOT 因为「不知道原方向」而抛错或产生未定义行为

### Requirement: `master()` / `slave()` 工厂 MUST 产出字段节点独立的 bundle

MUST 产出**字段节点独立**的 bundle（每个字段节点不与源 bundle 的对应字段共享
节点），并按 master / slave 设定方向。

> ℹ️ 「独立」的边界：**只约束字段节点**。bundle 的**总线节点**按拷贝构造语义
> **仍然共享**（既有测试依赖该语义）⇒ 本 Requirement MUST NOT 被解读为要求
> 总线节点独立。

#### Scenario: 工厂产物的字段节点 MUST 独立

- **WHEN** `master(src)` / `slave(src)` 产出副本
- **THEN** 副本的每个字段节点 MUST **不等于**源 bundle 的对应字段节点
- **AND** 副本的方向 MUST 按 master / slave 设定

#### Scenario: 总线节点的共享 MUST 保持

- **WHEN** 工厂产出副本后检查 bundle 的总线节点
- **THEN** 该节点 MUST 仍与源 bundle 共享（拷贝构造语义）
- **AND** MUST NOT 因「节点独立」而改变该既有语义

### Requirement: `operator=` 的「软件拷贝」语义 MUST 被显式记录

MUST 保留 `bundle_base::operator=`（以及普通信号字段的 `operator=`）为
**显式的软件拷贝 / 节点重绑定**语义 —— 它**不是**连接手段。

该语义 MUST 在 capability 中写明，MUST NOT 仅由测试注释钉住。

#### Scenario: `operator=` 不建立连接

- **WHEN** 用 `operator=` 把一个 bundle 的字段赋给另一个 bundle 的字段
- **THEN** 目标字段 MUST 与源字段**共享节点**（软件拷贝语义）
- **AND** 该行为 MUST NOT 被误解为「建立了连接」

### Requirement: bundle 的连接运算符语义 MUST 唯一

**连接**（`operator<<=` / `connect()`）MUST 通过**数据流边**建立，MUST NOT 通过
`operator=` 的**节点重绑定**语义。

`ch_stream` 等**自带自由 `operator<<=`** 的 bundle MUST 收敛到**数据流边**语义
（`ch_stream` 的 `ready` 反向流动由「两条边」表达）。

> ℹ️ 本 Requirement 约束的是**连接运算符**。`operator=` 的「软件拷贝」语义
> （见上）保留但**不是**连接手段 ⇒ 「语义唯一」MUST NOT 被解读为「全仓禁止
> `operator=`」。

#### Scenario: ch_stream 的连接 MUST 与其他 bundle 一致

- **WHEN** `ch_stream` 的两个实例被连接
- **THEN** 连接 MUST 通过**数据流边**建立（MUST NOT 用节点重绑定）
- **AND** 该行为 MUST 有双后端 parity 覆盖

#### Scenario: 嵌套 stream 字段的连接 MUST 同样走数据流边

- **WHEN** 一个 bundle 含 `ch_stream` 类型的字段，且两个这样的 bundle 被连接
- **THEN** 该嵌套字段的连接 MUST 同样通过**数据流边**建立
- **AND** MUST NOT 因「递归连接时选中了另一个重载」而退回节点重绑定

### Requirement: bundle 的边角设施 MUST 有可执行断言

MUST 为下列每一类边角设施提供至少一个 Catch2 断言：嵌套 bundle 字段、
`flip()`、工厂函数、序列化路径、`ch_stream` 的连接语义。

断言 MUST 是**行为级**的（驱动源字段 → 目标字段读到值），
MUST NOT 仅断言 `impl() != nullptr` 之类的结构性事实
（结构性断言在「节点重绑定」与「数据流边」两种语义下**都会通过** ⇒ 零信号）。

#### Scenario: 边角设施的断言存在且非空转

- **WHEN** reviewer 检查 bundle 能力的测试覆盖面
- **THEN** 上述每一类 MUST 至少有一个对应断言
- **AND** 对「连接退回节点重绑定」「flip 方向硬编码」「工厂共享节点」
  「deserialize 退回切片」这四类改动，相应断言 MUST 失败（证明非空转）

#### Scenario: MUST 有变异验证记录

- **WHEN** 本次变更的验证记录被审查
- **THEN** 四类变异（连接退回重绑定 / flip 硬编码 / 工厂共享节点 / deserialize 退回切片）
  MUST 各自被验证过「改坏 ⇒ 断言变红」
- **AND** 变异结果 MUST 写进本能力的**验证记录文件**
  （该文件的**具体路径**由实施任务钉住 —— 本 Requirement 只约束「必须有一份
  可寻址的记录」，MUST NOT 在 spec 里写死带占位符的路径）

### Requirement: 打包式 bundle API 的适用范围 MUST 写明

`get_bundle_value()` / `set_bundle_value()` 保留，但其**适用范围** MUST 写明：
仅适用于**未连接**的 bundle（已连接字段的值由设计每拍求值产生，TB 的写入会被
下一拍覆写）。

#### Scenario: 已连接字段的 TB 写入 MUST NOT 被当作契约

- **WHEN** 文档 / spec 描述打包式 API 的用途
- **THEN** MUST 明确它只用于**未连接**的 bundle
- **AND** MUST NOT 让用户以为它能驱动已连接的设计逻辑

## MODIFIED Requirements

### Requirement: bundle 字段 MUST 拥有自己的节点，且节点起源 MUST 明确

每个 bundle 字段 MUST 拥有**自己的**节点（**一等节点**），其节点 MUST NOT 是
某个「聚合总线节点」的位切片。

节点起源 MUST 明确规定：**何时创建**（bundle 构造时 / `as_master()` / `as_slave()`）、
**什么节点种类**（必须能与 `set_src(0, ...)` 兼容、必须能进 `context::get_eval_list()`
从而获得 `data_map` 槽位）、**未驱动时的取值语义**。

理由：字段若被重绑定成「零字面量总线的切片」，则**设计只读不写**的字段恒为 0
（切片读的是那条无人驱动的零字面量总线）⇒ 任何「只消费输入」的 bundle 字段
（如 FIFO 的 `push` / `pop`）永远收不到值。

> ✅ 该 Requirement 的覆盖面已与修法**对齐**：`deserialize_bits_to_fields()`
> （`include/core/bundle/bundle_serialization.h`）已一并改为**字段一等节点**
> ⇒ **不再**存在「只修一处」的分叉，也**不再**需要对它做显式排除。

#### Scenario: 设计只读不写的字段 MUST NOT 恒为 0

- **WHEN** 一个 bundle 字段在设计里**只被读取**、从未被赋值（如 FIFO 的 `push`）
- **AND** 另一个 bundle 的对应字段被 TB 驱动为 1，且两者已正确连接
- **THEN** 该字段 MUST 读到 **1**
- **AND** MUST NOT 恒为 0

#### Scenario: 字段节点 MUST 能在仿真中被寻址

- **WHEN** 一个 bundle 的字段被 `get_bundle_value()` / `set_bundle_value()` 访问
- **THEN** 该字段的节点 MUST 在 `data_map` 中有槽位（可寻址）
- **AND** MUST NOT 出现「Value not found for signal node ID」告警

#### Scenario: 未驱动的字段取值 MUST 确定

- **WHEN** 一个从未被驱动过的 bundle 字段被读取
- **THEN** 其取值 MUST 确定（零或实现定义的确定值）
- **AND** MUST NOT 是未定义行为或随机值

#### Scenario: 字段节点身份在方向切换前后 MUST 不变

- **WHEN** 一个 bundle 先 `as_master()` 后 `as_slave()`（或反之）
- **THEN** 每个字段的节点 MUST 保持同一个节点
- **AND** 只有字段的**方向**发生变化

### Requirement: bundle 字段连接 MUST 建立数据流边，不得用节点重绑定

bundle 的**连接** MUST 通过**数据流边**（`operator<<=` → `set_src(0, ...)`）建立，
MUST NOT 通过 `operator=` 的**节点重绑定**语义。

理由：`operator=` 对普通信号字段（`ch_bool` / `ch_uint<N>` 成员，而非
`ch_in`/`ch_out` 端口）只**改指针**——被重绑定掉的那个节点上已有的**驱动**会被
**孤儿化**（该节点从设计图里消失），而新指向的节点**没有驱动** ⇒ 连接静默失效。

> ℹ️ 本 Requirement 约束的是**连接语义**。设计侧「用哪个运算符驱动自己的字段」
> 属于**约定** ⇒ 本 Requirement MUST NOT 被解读为「全仓禁止用 `operator=` 驱动字段」。
>
> ✅ **遗留债务已消除**：`ch_stream` 的自由 `operator<<=` 与
> `deserialize_bits_to_fields()` 已收敛到数据流边 ⇒ 本 Requirement 的覆盖面不再有
> 已知的例外点。`operator=` 保留为**显式的软件拷贝 / 节点重绑定**语义
> （见本 capability 的对应 Requirement），但 MUST NOT 被用作**连接**手段。

#### Scenario: 连接后原驱动节点仍可达

- **WHEN** 一个 bundle 的某个字段已经有一个驱动源
- **AND** 该字段随后被连接到另一个 bundle 的对应字段
- **THEN** 字段原有的驱动节点 MUST 仍然在设计图里（可达）
- **AND** 该字段 MUST 由对方字段**驱动**（数据流边），而不是指向对方节点

#### Scenario: 连接后值能跨 bundle 边界传播

- **WHEN** master bundle 的一个 output 字段连接到 slave bundle 的同名 input 字段
- **THEN** slave 侧该字段的取值 MUST 随 master 侧变化
- **AND** 该传播 MUST 经由**数据流边**

#### Scenario: offset 非零的字段 MUST 正确传播

- **WHEN** 被连接的字段在 bundle 的位布局中**不是第一个**（offset > 0）
- **THEN** 传播的值 MUST 是该字段的**完整值**
- **AND** MUST NOT 是「源节点该位区间的切片」
