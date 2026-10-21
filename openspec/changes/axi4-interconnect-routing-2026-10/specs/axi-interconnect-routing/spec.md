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

1. **AW 通道**：`awaddr` / `awid` / `awlen` / `awsize` / `awburst` / `awvalid`
2. **W 通道**：`wdata` / `wid` / `wstrb` / `wlast` / `wvalid`
3. **AR 通道**：`araddr` / `arid` / `arlen` / `arsize` / `arburst` / `arvalid`
4. **B 通道（反向）**：`bid` / `bresp` / `bvalid` / `bready`
5. **R 通道（反向）**：`rdata` / `rid` / `rresp` / `rlast` / `rvalid` / `rready`

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
后续通道的转发 MUST 基于**锁存值**。

1. 写侧与读侧 MUST **各自独立**锁存（写路由 MUST NOT 用于读通道，读路由反之）。
2. 锁存的依据 MUST 是**该通道自己的地址**
   （写路由来自 `awaddr`，读路由来自 `araddr`），MUST NOT 用写地址驱动读路由。
3. W 通道与 B 通道 MUST 使用**写路由锁存**；
   R 通道与 `RREADY` MUST 使用**读路由锁存**。
4. 互连层 MUST NOT 用「活的」`awaddr` / `araddr` 或「活的」仲裁 grant 决定转发目标。
5. 每次多路选择 MUST **单次赋值**（MUST NOT 写成「先赋 m0 再读回 `io().s0_awaddr`」
   这类输出读自己的自环形式 —— 会导致拓扑排序报 `Cycle detected in IR graph`）。

#### Scenario: AW 握手后改写 AWADDR 不改变落点

- **WHEN** master 完成 AW 握手后改写 `awaddr` 为另一个 slave 窗口内的地址
- **AND** 随后提供 W 数据
- **THEN** W 数据 MUST 仍被转发到 **AW 握手时锁存**的那个 slave
- **AND** 该 slave 的寄存器 MUST 收到数据

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
3. 轮转指针 MUST 在**事务真正完成**时轮转：
   写侧 = 该 slave 的 `bvalid && bready`；读侧 = 该 slave 的 `rvalid && rready`。
   MUST NOT 用「某个 slave 的 `awready` 为 1」这类与事务完成无关的条件。
4. **在途事务的 master MUST 保持授权**，直到其事务完成。
5. 仲裁 MUST NOT 出现「所有 grant 恒为 0」的饿死状态：
   当且仅当至少一个 master 有请求时，MUST 至少有一个 grant 为 1。

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
2. MUST 存在**未映射区域**的语义；未映射地址的写/读 MUST 返回 `DECERR`（`0b11`），
   MUST NOT 别名到某个 slave，也 MUST NOT 静默丢弃。
3. 窗口内的地址 MUST 被转发给对应 slave，且**原样转发**（MUST NOT 改写地址），
   除非该互连明确声明了地址重映射语义。
4. `WSTRB` MUST 随 W 通道一起转发到目标 slave（不得在互连层被丢弃或清零）——
   否则下游从设备的字节道语义会被破坏。

#### Scenario: 未映射地址返回 DECERR

- **WHEN** master 向一个落在所有窗口之外的地址发起写事务
- **THEN** `bresp` MUST 为 `DECERR`
- **AND** 任何 slave 的寄存器 MUST 保持原值

#### Scenario: 窗口内地址原样转发

- **WHEN** master 向 slave 1 窗口内的地址 `0x4000_0004` 发起写事务
- **THEN** slave 1 的 `awaddr` MUST 等于 `0x4000_0004`（未被改写）

#### Scenario: WSTRB 被完整转发

- **WHEN** master 以 `wstrb = 0x5` 发起写事务
- **THEN** 目标 slave 的 `wstrb` MUST 等于 `0x5`

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
