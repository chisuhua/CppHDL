# Capability: bundle-master-slave-io

## ADDED Requirements

### Requirement: bundle 字段的节点身份 MUST 稳定

`as_master()` / `as_slave()` MUST 只设置字段的**方向**，MUST NOT 改变字段的**节点身份**
（MUST NOT 把字段重绑定成某个「聚合总线节点」的位切片）。

若确实需要一个「聚合总线」视图（例如打包 / 解包），该视图 MUST 由字段**派生**
（方向：字段 → 总线），MUST NOT 由总线**切片**得到字段。

理由：字段若被重绑定成「零字面量总线的切片」，则**设计只读不写**的字段恒为 0
（切片读的是那条无人驱动的零字面量总线）⇒ 任何「只消费输入」的 bundle 字段
（如 FIFO 的 `push` / `pop`）永远收不到值。

#### Scenario: 设计只读不写的字段 MUST NOT 恒为 0

- **WHEN** 一个 bundle 字段在设计里**只被读取**、从未被赋值（如 FIFO 的 `push`）
- **AND** 另一个 bundle 的对应字段被 TB 驱动为 1，且两者已正确连接
- **THEN** 该字段 MUST 读到 **1**
- **AND** MUST NOT 恒为 0

#### Scenario: 字段赋值 MUST NOT 改变总线视图的语义

- **WHEN** 设计对某个字段赋值（`field = expr`）
- **THEN** 该赋值 MUST 驱动该字段**自己的**节点
- **AND** MUST NOT 通过「让字段指向另一个节点」实现（那会孤儿化原节点）

#### Scenario: 字段节点身份在方向切换前后 MUST 不变

- **WHEN** 一个 bundle 先 `as_master()` 后 `as_slave()`（或反之）
- **THEN** 每个字段的节点 MUST 保持同一个节点
- **AND** 只有字段的**方向**发生变化

### Requirement: bundle 字段连接 MUST 建立数据流边，不得用节点重绑定

bundle 的字段连接 MUST 通过**数据流边**（`operator<<=` → `set_src(0, ...)`）建立，
MUST NOT 通过 `operator=` 的**节点重绑定**语义。

理由：`operator=` 对普通信号字段（`ch_bool` / `ch_uint<N>` 成员，而非
`ch_in`/`ch_out` 端口）只**改指针**——被重绑定掉的那个节点上已有的**驱动**会被
**孤儿化**（该节点从设计图里消失），而新指向的节点**没有驱动** ⇒ 连接静默失效。

#### Scenario: 连接后原驱动节点仍可达

- **WHEN** 一个 bundle 的某个字段已经有一个驱动源
- **AND** 该字段随后被连接到另一个 bundle 的对应字段
- **THEN** 字段原有的驱动节点 MUST 仍然在设计图里（可达）
- **AND** 该字段 MUST 由对方字段**驱动**（数据流边），而不是指向对方节点

#### Scenario: 连接后值能跨 bundle 边界传播

- **WHEN** master bundle 的一个 output 字段连接到 slave bundle 的同名 input 字段
- **THEN** slave 侧该字段的取值 MUST 随 master 侧变化
- **AND** 该传播 MUST 经由**数据流边**（拓扑排序可观测到依赖关系）

#### Scenario: offset 非零的字段 MUST 正确传播

- **WHEN** 被连接的字段在 bundle 的位布局中**不是第一个**（offset > 0）
- **THEN** 传播的值 MUST 是该字段的**完整值**
- **AND** MUST NOT 是「源节点该位区间的切片」

### Requirement: bundle 的 master / slave 字段方向 MUST 严格镜像

一个 bundle 的 `as_master_direction()` 与 `as_slave_direction()` MUST 对每个字段
声明**互为镜像**的方向：master 声明 output 的字段，slave MUST 声明 input；
master 声明 input 的字段，slave MUST 声明 output。

理由：master/slave 是**同一条总线**的两端；方向不镜像时，一端的输出会接到
另一端的输出上 ⇒ 总线两端同时驱动同一信号。

#### Scenario: fifo_bundle 的方向镜像

- **WHEN** 检查 `fifo_bundle` 的 `as_master_direction()` 与 `as_slave_direction()`
- **THEN** `data` / `push` / `pop` MUST 在 master 侧为 output、slave 侧为 input
- **AND** `full` / `empty` MUST 在 master 侧为 input、slave 侧为 output
- **AND** 该镜像关系 MUST 与 `config_bundle` / `interrupt_bundle` 同构

#### Scenario: 方向不镜像 MUST 被发现

- **WHEN** reviewer 对比同文件内各 bundle 的 master/slave 方向
- **THEN** 存在任一字段方向不镜像的 bundle MUST 被视为缺陷
  （方向不镜像 ⇒ 一端输出接另一端输出）

### Requirement: connect() MUST 方向感知

`bundle_base::connect(src, dst)` MUST 逐字段按**已声明的方向**决定驱动方向，
MUST NOT 无条件执行 `dst.field = src.field`。

#### Scenario: connect() 不覆盖接收方的输出字段

- **WHEN** `connect(master, slave)`，且某字段在 master 侧是 input、slave 侧是 output
- **THEN** 该字段 MUST 由 **slave 驱动 master**
- **AND** slave 侧该字段**自身的驱动** MUST NOT 被 master 的（未驱动）输入覆盖

#### Scenario: 同方向 bundle 的 connect 行为不变

- **WHEN** `connect(a, b)`，且 a 与 b 的对应字段方向**相同**
- **THEN** 行为 MUST 与既有语义一致（b 的字段由 a 的字段驱动）
- **AND** MUST NOT 退化为节点重绑定

### Requirement: Simulator 的 bundle 字段读写 MUST 走 data_map 槽位

`Simulator::get_bundle_value()` / `set_bundle_value()` MUST 通过字段节点的
`data_map` 槽位读写，MUST NOT 使用编译期常量转换，MUST NOT 使用节点重绑定。

理由：bundle 字段在真实设计里由**运行时逻辑**驱动（比较结果、FIFO 状态位等），
常量转换对它们必然失败并静默返回 0；重绑定则把 TB 的值写到设计图之外的节点上。

#### Scenario: 读取运行时驱动的 bundle 字段

- **WHEN** 一个 bundle 字段由运行时逻辑驱动（如 `empty = (rd_ptr == wr_ptr)`）
- **THEN** `get_bundle_value()` MUST 返回该字段的**当前运行时值**
- **AND** MUST NOT 返回 0（MUST NOT 依赖编译期常量转换）

#### Scenario: 写入 bundle 字段并被设计观察到

- **WHEN** 测试台用 `set_bundle_value()` 写入一个字段
- **THEN** 该字段 MUST 在**设计图内**被观察到（TB 的值 MUST 到达被驱动的逻辑）
- **AND** MUST NOT 通过节点重绑定把值写到设计图之外的节点

#### Scenario: 读写 MUST 对称

- **WHEN** `get_bundle_value()` 与 `set_bundle_value()` 都被审查
- **THEN** 二者 MUST 使用**同一套**槽位寻址逻辑（同一个节点的同一个槽位）
- **AND** MUST NOT 一侧读写槽位、另一侧走常量转换或重绑定

#### Scenario: 总宽超过 64 位的 bundle MUST NOT 被静默截断

- **WHEN** 一个 bundle 的**总宽 > 64 位**（实测 `axi_lite_bundle<32,32>` 为 142 位）
- **THEN** 字段的读写 MUST **逐字段**进行（每个字段有自己的槽位，宽度任意）
- **AND** MUST NOT 把整个 bundle 打包进一个 64 位整数
- **AND** 若某个「打包成整数」的便捷 API 存在，MUST 明确其 ≤ 64 位约束
  （MUST NOT 静默截断）

### Requirement: bundle master/slave 连接 MUST 有可执行回归护栏

MUST 至少有一个 Catch2 `TEST_CASE` 覆盖「bundle 之间的 master/slave 连接」，
且 MUST 覆盖**跨边界的值传播**（TB → slave / slave → master 两个方向）。

#### Scenario: 回归护栏存在且非空转

- **WHEN** 运行该 `TEST_CASE`
- **THEN** 它 MUST 断言 TB 写入的值**到达** slave 侧的被驱动逻辑
- **AND** 它 MUST 断言 slave 产生的值**到达** master 侧
- **AND** 对「字段节点身份被重建」「字段连接退回节点重绑定」「bundle 值读写退回
  常量转换」这三类改动，该 `TEST_CASE` MUST 失败（证明非空转）

#### Scenario: 护栏 MUST 覆盖 offset 非零与只读字段

- **WHEN** 审查该 `TEST_CASE` 的 DUT
- **THEN** DUT MUST 使用**多字段** bundle
- **AND** MUST 含至少一个 `offset > 0` 的字段
- **AND** MUST 含至少一个「设计只读不写」的字段
- **AND** 单字段（offset == 0）的 bundle MUST NOT 被当作充分覆盖

#### Scenario: 护栏 MUST 在 ctest 内

- **WHEN** reviewer 检查该护栏的注册方式
- **THEN** 它 MUST 是被 `ctest` 注册的 Catch2 用例
- **AND** MUST NOT 仅依赖 sample 自检（sample 不在 ctest / CI，
  历史上正是「只靠 sample」让 `samples/fifo_bundle` 的失败长期不可见）
