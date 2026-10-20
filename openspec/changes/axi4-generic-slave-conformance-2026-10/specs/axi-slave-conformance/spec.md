# Capability: axi-slave-conformance

## Purpose

定义 CppHDL 中**通用 AXI slave**（`include/axi4/axi4_lite_slave.h` 的 `AxiLiteSlave`、
`include/axi4/axi4_full.h` 的 `Axi4Slave`）与**总线层**（`axi_interconnect_4x4.h` /
`axi4_lite_matrix.h` / `axi_to_axilite.h`）的协议规范形态契约。

与 `dual-backend-test-coverage` 的分工：该 capability 管"同一场景在 interpreter 与
verilator 两条后端上的一致性"；本 capability 管"AXI 协议行为本身"（复位、地址锁存、
响应保持、未映射地址、burst 语义、字节道透传）。

## ADDED Requirements

### Requirement: 通用 AXI slave MUST 提供复位

AXI slave MUST 暴露 `ch_in<ch_bool> rst_n`（低有效异步复位）端口，且其全部状态寄存器
MUST 使用 `select(!io().rst_n, <init_val>, ...)` 复位 pattern。

#### Scenario: 复位清零全部寄存器

- **WHEN** `rst_n` 为 0
- **THEN** slave 的全部寄存器 MUST 读回各自的初值
- **AND** 复位 MUST NOT 依赖外部 testbench 预先清零数据

#### Scenario: 未接线的 rst_n 等价于永久复位

- **WHEN** 实例化点未驱动 `rst_n`（读出为 0）
- **THEN** slave MUST 保持在复位态（全部寄存器为初值）
- **AND** 实现 MUST 在实例化点显式接线（`rst_n` 是**必需**端口，不是可选）

### Requirement: 通用 AXI slave MUST 锁存地址

slave MUST 在 AW/AR 握手时把 `io().awaddr` / `io().araddr` 锁存进寄存器，
写/读地址解码 MUST 基于**锁存**的地址。

#### Scenario: 握手后改写地址不改变写目标

- **WHEN** master 在 AW 握手完成后改写 `awaddr`（AXI 允许）
- **THEN** 该笔事务 MUST 仍然写入 AW 握手时的目标寄存器
- **AND** MUST NOT 写入改写后的地址对应的寄存器

#### Scenario: 握手后改写地址不改变读目标

- **WHEN** master 在 AR 握手完成后改写 `araddr`
- **THEN** 该笔读事务 MUST 返回 AR 握手时目标寄存器的值

### Requirement: 通用 AXI slave 的写响应 MUST 保持到 BREADY

slave 的 `bvalid` MUST 在写数据被接受后置位，并**保持到** `bready` 握手完成；
MUST NOT 是组合单拍脉冲。

#### Scenario: master 不拉 BREADY 时 bvalid 保持

- **WHEN** 写数据被接受且 master 未拉 `bready`
- **THEN** `bvalid` MUST 保持为 1（跨周期不消失）
- **AND** slave MUST NOT 接受下一笔写事务（busy 保持）

#### Scenario: BREADY 握手后 bvalid 清除

- **WHEN** `bvalid` 为 1 且 master 拉高 `bready`
- **THEN** `bvalid` MUST 在该次握手后清零
- **AND** slave MUST 能接受下一笔事务

### Requirement: 通用 AXI slave MUST 对未映射地址返回 DECERR

slave MUST 对其地址空间之外的访问返回 `DECERR`，且 MUST NOT 把未映射地址
**别名**到已实现的寄存器。

#### Scenario: 未映射地址写被拒绝

- **WHEN** master 写一个未映射地址
- **THEN** `bresp` MUST 为 `DECERR`
- **AND** 任何寄存器 MUST 保持原值

#### Scenario: 未映射地址读被拒绝

- **WHEN** master 读一个未映射地址
- **THEN** `rresp` MUST 为 `DECERR`
- **AND** `rdata` MUST NOT 呈现其它寄存器的值

### Requirement: 通用 AXI4 Full slave MUST 支持多 beat 写 burst

`Axi4Slave` MUST 接受 `awlen` 指定长度（+1）的全部 W beat，MUST NOT 静默丢弃
后续 beat；每个 beat MUST 写到按 `awburst` 推进的地址。

#### Scenario: 多 beat 写全部落地

- **WHEN** master 用 `awlen = 1`（2 beat）写 burst
- **THEN** 两个 beat MUST 都被接受（不得只落第 1 个）
- **AND** INCR burst 下两个 beat MUST 落到**不同**的寄存器
- **AND** FIXED burst 下两个 beat MUST 落到**同一个**寄存器

#### Scenario: 每个 beat 使用自己的 wstrb

- **WHEN** master 的各 beat 使用不同的 `wstrb`
- **THEN** 每个 beat MUST 只修改该 beat 的 `wstrb` 选中的字节
- **AND** 未被任何 beat 选中的字节 MUST 保持原值

### Requirement: 通用 AXI4 Full slave MUST 支持多 beat 读 burst

`Axi4Slave` MUST 按 `arlen` 返回全部读 beat，`rdata` MUST 按当前 beat 的地址更新，
`rlast` MUST 在最后一拍为 1。

#### Scenario: 多 beat 读返回逐拍数据

- **WHEN** master 用 `arlen = 1`（2 beat）读 burst
- **THEN** `rdata` MUST 在两拍分别给出两个地址的寄存器值
- **AND** 末拍 `rlast` MUST 为 1
- **AND** 读事务结束后 `rvalid` MUST 归零（slave 能接受下一笔事务）

### Requirement: 总线层 MUST 完整透传 WSTRB

interconnect / matrix MUST 把 master 的 `wstrb`（以及 `wid` / `wlast`）按其路由
**完整**转发到目标 slave。未命中通路的 slave MUST NOT 收到 `wstrb == 0`
（那会被 slave 解释为"无字节被选中"的空写）。

#### Scenario: 非零号 master 的字节写经 interconnect 生效

- **WHEN** master m1/m2/m3 经 interconnect 向 s1/s2/s3 发部分字节写
- **THEN** 目标 slave MUST 收到该 `wstrb`
- **AND** 写 MUST 只修改被选中的字节（MUST NOT 变成空写）

#### Scenario: 未命中通路的 wstrb 不得默认为 0

- **WHEN** 某 slave 未被任何 master 授权
- **THEN** 该 slave 的 `wstrb` 输入 MUST 为 0 **且** MUST NOT 有写握手
      （`wvalid` 为 0 时 `wstrb` 的值不影响写）
