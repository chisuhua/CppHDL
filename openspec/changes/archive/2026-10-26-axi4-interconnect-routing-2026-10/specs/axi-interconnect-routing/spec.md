# Capability: axi-interconnect-routing

## Purpose

定义 CppHDL 中 **AXI 路由层**（interconnect / matrix）的协议语义契约：
地址映射、路由锁存、仲裁、响应反查、错误响应。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并 delta 文件的 `## Purpose`**
> （新 capability 的主 spec 会被建成 `## Purpose / TBD - created by archiving change ...` 骨架）。
> 因此凡 MUST 长期存在的契约性内容，一律写进 **Requirement 正文**（正文会被整条合并），
> 不依赖本 `## Purpose`。archive 后 MUST 另行补写主 spec 的 `## Purpose`。

适用对象：

- `include/axi4/axi_interconnect_4x4.h` 的 `AxiInterconnect4x4`（AXI4 Full 5 通道，4×4）
- `include/axi4/axi4_lite_matrix.h` 的 `AxiLiteMatrix2x2`（AXI4-Lite，2×2）

与既有 capability 的分工：

- `axi-slave-conformance` 管辖**从设备侧**的规范形态（`AxiLiteSlave` /
  `AxiToAxiLiteConverter`），本 capability 管辖**互连侧**。
- `dual-backend-test-coverage` 管辖「同一场景在 interpreter 与 verilator 两后端的一致性」；
  本 capability **不重复**该维度，只在新增行为引入时要求双后端覆盖。
- 互连层 MUST NOT 改变下游从设备的协议形态；
  从设备侧的规范形态以 `axi-slave-conformance` 为准。

## ADDED Requirements

### Requirement: AXI 互连层 MUST 对全部 (master, slave) 对提供完整路由

`AxiInterconnect4x4` MUST 对 4 × 4 = **16 个 (master, slave) 对**中的每一对提供完整路由：

1. **AW 通道**：`awaddr` / `awid` / `awlen` / `awsize` / `awburst` / `awvalid` / `awready`
2. **W 通道**：`wdata` / `wid` / `wstrb` / `wlast` / `wvalid` / `wready`
3. **AR 通道**：`araddr` / `arid` / `arlen` / `arsize` / `arburst` / `arvalid` / `arready`
4. **B 通道（反向）**：`bid` / `bresp` / `bvalid` / `bready`
5. **R 通道（反向）**：`rdata` / `rid` / `rresp` / `rlast` / `rvalid` / `rready`

> 📌 `WID` 是 AXI3 遗留（AXI4 已删除该信号），仓内 `Axi4Slave` 与互连都带着它
> → 本 capability 只要求**透传**，MUST NOT 语义化。

任何一对的缺失 MUST 被视为**实现缺陷**（不是「暂不支持」），
MUST 由单元测试逐一覆盖。

`AxiLiteMatrix2x2` 的同一要求适用于它的 2 × 2 = 4 个对
（其通道集合为 AXI4-Lite 的 AW / W / B / AR / R）。

#### Scenario: 每个 master 都能到达每个 slave

- **WHEN** 依次让 m0 / m1 / m2 / m3 各自发起一次写事务，目标地址分别落在
  s0 / s1 / s2 / s3 的窗口内
- **THEN** 4 次写 MUST 全部被对应的 slave 接受
- **AND** 每个 slave MUST 收到**自己的**地址（不串扰）

#### Scenario: 每个 slave 的响应 MUST 回到发起它的 master

- **WHEN** m2 向 s3 发起写事务，s3 返回 B 响应
- **THEN** 该 B 响应 MUST 只出现在 m2 的 `bvalid` / `bresp` / `bid` 上
- **AND** m0 / m1 / m3 的 `bvalid` MUST 保持 0

### Requirement: AXI 互连层 MUST 锁存路由目标

互连层 MUST 在 **AW / AR 握手当拍**把目标 slave 索引**锁存**进寄存器，
**握手完成之后**的转发 MUST 基于**锁存值**。

1. 写侧与读侧 MUST **各自独立**锁存（写路由 MUST NOT 用于读通道，读路由反之）。
2. 锁存的依据 MUST 是**该通道自己的地址**
   （写路由来自 `awaddr`，读路由来自 `araddr`），MUST NOT 用写地址驱动读路由。
3. W 通道与 B 通道 MUST 使用**写路由锁存**；
   R 通道与 `RREADY` MUST 使用**读路由锁存**。
4. **握手决策 vs 握手后转发**（本条是契约的一部分，MUST NOT 混淆）：
   - **握手决策**（`s*_awvalid` / `s*_arvalid` 的正向转发、
     `m*_awready` / `m*_arready` 的反查）MUST 用**当拍**地址解码 + **当拍** grant。
     理由：路由锁存是在握手当拍才写入的，握手要完成就必须用当拍地址解码
     （AXI 规定 master 在 `awvalid && !awready` 期间 MUST 保持 `awaddr` 稳定，
     故握手窗口内的「活地址」是安全的）。
   - **握手后的转发**（W / B / R 通道）MUST 用**锁存路由**，
     MUST NOT 用活的 `awaddr` / `araddr` 或活的仲裁 grant。
   - ⚠️ **正向转发 MUST 同时吃「在途门控」**（`!write_active[m]` /
     `!read_active[m]`）：该 master 在自己的事务在途期间合法地举起**下一笔**
     `awvalid` / `arvalid`（AXI4 允许多 outstanding）时，本笔请求 MUST NOT
     被转发给 slave —— 否则 slave 会接受一个**互连从未锁存路由**的请求
     （幻影 AW / AR）⇒ slave 等不到 W、master 等不到 `awready`
     ⇒ **双向永久死锁**。该门控与第 5 条的 `m*_awready` 门控是**同一件事的
     两面**：master 侧看不到 `awready` ⇒ slave 侧也 MUST NOT 看到 `awvalid`。
5. **在途门控**：`m*_awready` MUST 被 `!write_active[m]` 门控，
   `m*_arready` MUST 被 `!read_active[m]` 门控
   —— 在途期间 MUST 为 0，否则 master 认为 AW/AR 被接受而互连悄悄吞掉
   （AW 静默丢失）。
6. 每次多路选择 MUST **单次赋值**（MUST NOT 写成「先赋 m0 再读回 `io().s0_awaddr`」
   这类输出读自己的自环形式 —— 会导致拓扑排序报 `Cycle detected in IR graph`）。

#### Scenario: AW 握手后改写 AWADDR 不改变落点

- **WHEN** master 完成 AW 握手后改写 `awaddr` 为另一个 slave 窗口内的地址
- **AND** 随后提供 W 数据
- **THEN** W 数据 MUST 仍被转发到 **AW 握手时锁存**的那个 slave
- **AND** 该 slave 的寄存器 MUST 收到数据

#### Scenario: 在途事务期间不接新 AW

- **WHEN** master 有一笔写事务在途（AW 已握手、W 未完成）
- **THEN** `m*_awready` MUST 为 0
- **AND** master 发出的第二个 AW MUST NOT 被接受
- **AND** 该 AW MUST NOT 被静默吞掉（`awready` 为 0 时 master 会保持 `awvalid`）

#### Scenario: 读事务按读地址路由

- **WHEN** master 的 `awaddr` 落在 slave 0 窗口、`araddr` 落在 slave 2 窗口
- **THEN** 写事务 MUST 路由到 slave 0
- **AND** 读事务 MUST 路由到 slave 2
- **AND** 读响应 MUST 从 slave 2 返回

### Requirement: AXI 互连层 MUST 为写通道与读通道提供独立的仲裁器

1. AW 通道与 AR 通道 MUST 各有一套独立的 round-robin 仲裁器。
2. 仲裁请求 MUST 取自**该通道自己的 valid 信号**
   （写仲裁用 `awvalid`，读仲裁用 `arvalid`），
   MUST NOT 用写请求驱动读授权。
3. **仲裁 MUST NOT 出现「所有 grant 恒为 0」的饿死状态**：
   当至少一个 master 有请求且**无在途事务**时，MUST 至少有一个 grant 为 1
   （指针指向无请求的 master 时，MUST 让位给有请求的 master）。
   ⚠️ 「无在途事务」限定词是契约的一部分：在途期间 grant MUST 保持给在途 master，
   此时其他 master 的 grant 为 0 是**正确**行为，不构成饿死。
4. 轮转指针 MUST 在**事务真正完成**时轮转：
   写侧 = **当前 grant 指向的**那个 slave 的 `bvalid && bready`；
   读侧 = **当前 grant 指向的**那个 slave 的 `rvalid && rready`。
   MUST NOT 用「某个 slave 的 `awready` 为 1」这类与事务完成无关的条件，
   MUST NOT 用「任一 slave 的 `bvalid && bready`」（后者是原缺陷的换皮）。
5. **在途事务的 master MUST 保持授权**，直到其事务完成。

#### Scenario: 在途写事务期间授权不切换

- **WHEN** m0 完成 AW 握手，W 数据尚未到达
- **AND** 此时 m1 也在请求
- **THEN** m0 的 W 数据 MUST 仍被转发到 m0 的目标 slave
- **AND** m0 的 B 响应 MUST 回到 m0

#### Scenario: 读事务不被写请求阻塞

- **WHEN** m0 只有读请求（`arvalid = 1`，`awvalid = 0`），m1 只有写请求
- **THEN** m0 的读事务 MUST 能被接受
- **AND** m1 的写事务 MUST 能被接受

### Requirement: AXI 互连层的地址映射 MUST 可配置且能产生 DECERR

1. 地址到 slave 索引的映射 MUST 由**编译期可配置**的参数决定
   （base / size 或等价的窗口定义），MUST NOT 硬编码为固定位宽的移位。
2. 读通道 MUST 用**读地址**解码，MUST NOT 用写地址。
3. 窗口内的地址 MUST 被转发给对应 slave，且**原样转发**（MUST NOT 改写地址），
   除非该互连明确声明了地址重映射语义。
4. `WSTRB` MUST 随 W 通道一起转发到目标 slave（不得在互连层被丢弃或清零）——
   否则下游从设备的字节道语义会被破坏。
5. **未映射地址 MUST 由互连层自己应答**（互连是纯路由，没有下游 slave 可应答）：
   - **写侧**：MUST 被接受（`awready` 拉高），
     并由互连层产生 `bvalid = 1` + `bresp = DECERR`；
     `bvalid` MUST **保持到 `bready`**（MUST NOT 用组合单拍脉冲 ——
     合规 master 看不到 BVALID → 永久死锁）。
   - **读侧**：MUST 被接受（`arready` 拉高），
     并由互连层返回 **`arlen + 1` 个 beat**：每拍 `rvalid = 1` + `rresp = DECERR`，
     末拍 `rlast = 1`（MUST NOT 只回 1 拍 —— master 等不到 `rlast` 会挂死）。
   - 该 responder 的状态 MUST 是**寄存器**（MUST NOT 组合直出）。
6. ⚠️ **默认映射的覆盖度 MUST 被记录**：若默认映射与「地址高 2 bit」等价，
   则**全部**地址空间都被映射 → 默认配置下 DECERR **不可触发**。
   → DECERR 场景 MUST 用**非默认配置**实例化；该事实 MUST 写进文档。

#### Scenario: 未映射地址写返回 DECERR

- **WHEN** master 向一个落在所有窗口之外的地址发起写事务
- **THEN** `bresp` MUST 为 `DECERR`
- **AND** 任何 slave 的寄存器 MUST 保持原值
- **AND** `bvalid` MUST 保持到 `bready`（不是组合单拍脉冲）

#### Scenario: 未映射地址读返回 arlen+1 个 DECERR beat

- **WHEN** master 向一个落在所有窗口之外的地址发起读事务，`arlen = 3`
- **THEN** 互连层 MUST 返回 **4 个** beat
- **AND** 每个 beat 的 `rresp` MUST 为 `DECERR`
- **AND** 只有第 4 个 beat 的 `rlast` MUST 为 1
- **AND** `rdata` MUST 为 0

#### Scenario: 窗口内地址原样转发

- **WHEN** master 向 slave 1 窗口内的地址 `0x4000_0004` 发起写事务
- **THEN** slave 1 的 `awaddr` MUST 等于 `0x4000_0004`（未被改写）

#### Scenario: WSTRB 被完整转发

- **WHEN** master 以 `wstrb = 0x5` 发起写事务
- **THEN** 目标 slave 的 `wstrb` MUST 等于 `0x5`

### Requirement: AXI 互连层 MUST 提供低有效复位

`AxiInterconnect4x4` 与 `AxiLiteMatrix2x2` MUST 暴露 `ch_in<ch_bool> rst_n`
（低有效，**在时钟沿生效** —— `select(!io().rst_n, <init_val>, ...)` 落在 `ch_reg` 的
`next` 表达式上，由时钟沿采样），其**全部**状态寄存器
（路由锁存、`active` 标志、AW/AR 仲裁指针、error responder 状态）MUST 使用该复位 pattern，
`<init_val>` MUST 带该寄存器的**显式位宽**（同 `axi-slave-conformance` 的
「复位初值必须带显式位宽」）。

`rst_n` 是**必需端口**，实例化点 MUST 显式接线；
未接线读出为 0 = **永久复位**（同 `axi-slave-conformance` 的
「未接线的 rst_n 等价于永久复位」）。

**与 `axi-slave-conformance` 的分工**：互连层是 rst_n 链路的**中间节点**，
MUST 与下游 slave 在**同一复位窗口内**复位 —— 否则 slave 的在途标志被清零
而互连的路由锁存残留，该 master 永久等不到响应（**静默死锁**）。

**为什么 MUST 而非豁免**（裁决记录，2026-10-21）：

1. 改造完成后互连层**不再是纯路由**（路由锁存 + `active` 标志 + 双仲裁指针 +
   error responder 状态），「纯路由故由 INIT 保证复位」的前提不成立。
2. 「backend reset pulse 保证复位」**只在 t=0 成立**：
   `Simulator::reset()` 把 `data_map_` 全清 0 且**不委派给 backend**
   （`src/simulator.cpp:422-441`），
   interpreter 清 0 而 verilator 加载 INIT → **两后端中途复位语义分歧**。
   唯一两后端一致、testbench 可控的中途复位机制 = 组件级 `rst_n`。
3. 互连在 rst_n 链路上是**唯一断点**：
   `examples/axi4/phase3a_complete.cpp` 的顶层 `rst_n`（`:60`）已透传到
   converter（`:126`）与 slave（`:127`），唯独互连（`:75`）没有该端口；
   `examples/axi4/axi_soc_example.cpp` 同理（顶层 `:47` → gpio/uart `:263-264`，
   唯独 matrix `:195` 断开）。
4. 「可选端口」在本框架**不可行**：未接线的 `ch_in<ch_bool>` 读出为 0，
   配标准 pattern 即永久复位；「未接线 = 不复位」需要运行期「是否被驱动」查询，
   而 `num_srcs()` / `driver()` 是**构图期**属性（子组件 `describe()` 执行时
   父的 `<<=` 接线尚未发生）→ 不可靠。

#### Scenario: 复位清零全部互连状态

- **WHEN** `rst_n` 为 0 并经过至少 1 个时钟沿
- **THEN** 全部路由锁存 MUST 为 0，全部 `active` 标志 MUST 为 0
- **AND** 仲裁指针 MUST 回到初值
- **AND** error responder 状态 MUST 为空闲
- **AND** 复位 MUST NOT 依赖外部 testbench 预先清零状态

#### Scenario: 中途复位提供逃生路径

- **WHEN** 一笔事务在途（AW 已握手、W 未完成）时拉低 `rst_n` 至少 1 拍
- **THEN** 全部 `active` 标志 MUST 被清零
- **AND** 复位后互连层 MUST 能接受一笔**全新**的事务
- **AND** 该行为 MUST 在 interpreter 与 verilator 两后端下一致
  （两后端的中途复位语义只由组件级 `rst_n` 保证一致）

#### Scenario: 实例化点不得 tie 常量

- **WHEN** 审查互连层的实例化点
- **THEN** `rst_n` MUST 透传自父级的复位源
- **AND** MUST NOT 出现 `interconnect.io().rst_n <<= ch_bool(true)` 之类的 tie 常量
  （那会永久掩盖复位路径，使中途复位无法验证）

### Requirement: AXI 互连层 MUST 有单元测试覆盖

`AxiInterconnect4x4` 与 `AxiLiteMatrix2x2` MUST 各有专门的单元测试文件，
且 MUST 走 `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR` macro
（interpreter + verilator 双后端跑）。

测试 MUST 包含**数据断言**（不只比较握手信号）——
只比较握手信号的测试无法发现「W 被路由到错误的 slave」这类缺陷。

#### Scenario: 全扇出被逐一覆盖

- **WHEN** 枚举单元测试中出现的 (master, slave) 对
- **THEN** 结果 MUST 覆盖 `AxiInterconnect4x4` 的全部 16 对
- **AND** MUST 覆盖 `AxiLiteMatrix2x2` 的全部 4 对

#### Scenario: 缺陷回归护栏存在

- **WHEN** 任一 (master, slave) 对的路由被移除，或路由目标改用活地址
- **THEN** 对应的单元测试 MUST 变红
- **AND** MUST NOT 出现「只断言握手信号因而仍然绿」的情况
