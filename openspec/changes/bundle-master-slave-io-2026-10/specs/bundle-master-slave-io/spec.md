# Capability: bundle-master-slave-io

## ADDED Requirements

### Requirement: bundle 字段 MUST 拥有自己的节点，且节点起源 MUST 明确

每个 bundle 字段 MUST 拥有**自己的**节点（**一等节点**），其节点 MUST NOT 是
某个「聚合总线节点」的位切片。

节点起源 MUST 明确规定：**何时创建**（bundle 构造时 / `as_master()` / `as_slave()`）、
**什么节点种类**（必须能与 `set_src(0, ...)` 兼容、必须能进 `context::get_eval_list()`
从而获得 `data_map` 槽位）、**未驱动时的取值语义**。

理由：字段若被重绑定成「零字面量总线的切片」，则**设计只读不写**的字段恒为 0
（切片读的是那条无人驱动的零字面量总线）⇒ 任何「只消费输入」的 bundle 字段
（如 FIFO 的 `push` / `pop`）永远收不到值。

> ⚠️ 该 Requirement 的覆盖面 MUST 与修法一致：除 `set_role()` 外，
> `deserialize_bits_to_fields()`（`include/core/bundle/bundle_serialization.h:85-97`）
> 同样把字段重绑定成 `bit_select` / `bits<>` 切片 ⇒ 它要么一并修，要么
> MUST 在本 Requirement 里被**显式排除**（MUST NOT 宣称「MUST NOT 把字段重绑定成
> 位切片」却只修一处）。

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
> 属于**约定**（当前仓库里 `ch_stream` 的自由 `operator<<=` 与
> `deserialize_bits_to_fields()` 仍在用重绑定，见 `proposal.md` 的「遗留债务」）
> —— 本 Requirement MUST NOT 被解读为「全仓禁止用 `operator=` 驱动字段」。

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

### Requirement: bundle 字段的方向指认 MUST 与协议语义一致

一个 bundle 的 `as_master_direction()` / `as_slave_direction()` MUST 为每个字段
指认**协议上正确**的一侧：master 拥有该总线的**发起方**（命令 / 写数据），
slave 拥有**响应方**（状态 / 读数据）。

> ⚠️ 「镜像」只是**必要条件**，不是充分条件：镜像但**两侧指认反了**同样是错的
> （今天的 `fifo_bundle` 就是这样 —— 5 个字段里 4 个已镜像，唯独 `push` 放错边）。

#### Scenario: fifo_bundle 的方向指认

- **WHEN** 检查 `fifo_bundle` 的 `as_master_direction()` / `as_slave_direction()`
- **THEN** `data`（写数据）/ `push` / `pop` MUST 在 **master** 侧为 output、
  **slave** 侧为 input
- **AND** `full` / `empty` MUST 在 **master** 侧为 input、**slave** 侧为 output
- **AND** 该指认 MUST 与 `config_bundle` / `interrupt_bundle` 的同构关系一致

#### Scenario: 方向指认 MUST 可被自动断言

- **WHEN** reviewer 检查一个 bundle 的方向
- **THEN** 该方向 MUST 能被一个 Catch2 断言逐字段枚举并比对
  （MUST NOT 依赖「reviewer 人工对比」这类不可自动化的判据）

### Requirement: connect() MUST 方向感知

`bundle_base::connect(src, dst)` MUST 逐字段按**已声明的方向**决定驱动方向，
MUST NOT 无条件执行 `dst.field = src.field`。

参数顺序 MUST 明确为：`connect(src, dst)` ≡
`dst.connect_field_based_on_direction_and_source(dst.field, src.field)`。

#### Scenario: connect() 不覆盖接收方的输出字段

- **WHEN** `connect(master, slave)`，且某字段在 master 侧是 input、slave 侧是 output
- **THEN** 该字段 MUST 由 **slave 驱动 master**
- **AND** slave 侧该字段**自身的驱动** MUST NOT 被 master 的（未驱动）输入覆盖

#### Scenario: 同方向字段时 src 驱动 dst

- **WHEN** `connect(a, b)`，且 a 与 b 的对应字段方向**相同**
- **THEN** b 的该字段 MUST 由 a 的该字段驱动
- **AND** MUST NOT 退化为节点重绑定

### Requirement: Simulator 的 bundle 字段读写 MUST 走 data_map 槽位

`Simulator::get_bundle_value()` / `set_bundle_value()` MUST 通过字段节点的
`data_map` 槽位读写，MUST NOT 使用编译期常量转换，MUST NOT 使用节点重绑定。

理由：bundle 字段在真实设计里由**运行时逻辑**驱动（比较结果、FIFO 状态位等），
常量转换对它们必然失败并静默返回 0；重绑定则把 TB 的值写到设计图之外的节点上。

> ℹ️ 适用面：**已连接**的字段其值由设计每拍求值产生，TB 的写入会被下一拍覆写
> ⇒ `set_bundle_value()` 的合法用途是**未连接**的 bundle 字段（调试 / 独立驱动）。

#### Scenario: 读取运行时驱动的 bundle 字段

- **WHEN** 一个 bundle 字段由运行时逻辑驱动（如 `empty = (rd_ptr == wr_ptr)`）
- **THEN** `get_bundle_value()` MUST 返回该字段的**当前运行时值**
- **AND** MUST NOT 返回 0（MUST NOT 依赖编译期常量转换）

#### Scenario: 写入未连接字段并被设计观察到

- **WHEN** 测试台用 `set_bundle_value()` 写入一个**未连接**的 bundle 字段
- **THEN** 该字段 MUST 在**设计图内**被观察到
- **AND** MUST NOT 通过节点重绑定把值写到设计图之外的节点

#### Scenario: 读写 MUST 对称

- **WHEN** `get_bundle_value()` 与 `set_bundle_value()` 都被审查
- **THEN** 二者 MUST 使用**同一套**槽位寻址逻辑（同一个节点的同一个槽位）
- **AND** MUST NOT 一侧读写槽位、另一侧走常量转换或重绑定

#### Scenario: 打包 API 的宽度约束 MUST 被强制

- **WHEN** 一个 bundle 的**总宽 > 64 位**（实测 `axi_lite_bundle<32,32>` 为 142 位）
- **THEN** 调用打包式 `get_bundle_value()` / `set_bundle_value()` MUST **显式拒绝或告警**
  （MUST NOT 静默截断）
- **AND** 字段级读写 MUST NOT 受总宽限制
- **AND** 单个字段宽度 > 64 位时 MUST 显式拒绝或告警（MUST NOT 静默截断）

#### Scenario: 64 位宽字段 MUST NOT 触发位移溢出

- **WHEN** 一个 bundle 含**宽度恰为 64** 的字段
- **THEN** 位段提取 MUST NOT 使用 `1ULL << 64` 形式的移位
  （该形式是未定义行为）

### Requirement: bundle master/slave 连接 MUST 有可执行回归护栏

MUST 至少有一个 Catch2 `TEST_CASE` 覆盖「bundle 之间的 master/slave 连接」，
且 MUST 覆盖**跨边界的值传播**（TB → slave / slave → master 两个方向）。

#### Scenario: 回归护栏存在且非空转

- **WHEN** 运行该 `TEST_CASE`
- **THEN** 它 MUST 断言 TB 写入的值**到达** slave 侧的被驱动逻辑
- **AND** 它 MUST 断言 slave 产生的值**到达** master 侧
- **AND** 对「字段节点身份被重建」「字段连接退回节点重绑定」「bundle 值读写退回
  常量转换」这三类改动，该 `TEST_CASE` MUST 失败（证明非空转）

#### Scenario: 护栏 MUST 用 bundle 级接线

- **WHEN** 审查该 `TEST_CASE` 的 DUT 接线方式
- **THEN** DUT MUST 经 **bundle 级** `connect(master, slave)` 或 `bundle <<= bundle` 接线
- **AND** MUST NOT 逐字段手工 `<<=`（否则方向感知 / 方向指认的回归不会被发现）

#### Scenario: 护栏 MUST 覆盖 offset 非零与只读字段

- **WHEN** 审查该 `TEST_CASE` 的 DUT
- **THEN** DUT MUST 使用**多字段** bundle
- **AND** MUST 含至少一个 `offset > 0` 的字段
- **AND** MUST 含至少一个「设计只读不写」的字段
- **AND** 单字段（offset == 0）的 bundle MUST NOT 被当作充分覆盖

#### Scenario: 护栏 MUST 另有一个「未连接 bundle」用例

- **WHEN** 审查该 `TEST_CASE` 的覆盖面
- **THEN** MUST 另有一个针对**未连接** bundle 的用例
  （覆盖 `set_bundle_value()` / `get_bundle_value()` 的槽位读写语义）
- **AND** 已连接的 master↔slave DUT MUST NOT 被用来断言「TB 写入被设计观察到」

#### Scenario: 护栏 MUST 在 ctest 内

- **WHEN** reviewer 检查该护栏的注册方式
- **THEN** 它 MUST 是被 `ctest` 注册的 Catch2 用例
- **AND** MUST NOT 仅依赖 sample 自检（sample 不在 ctest / CI，
  历史上正是「只靠 sample」让 `samples/fifo_bundle` 的失败长期不可见）
