# Capability: axi4-full-slave-burst

## Purpose

定义 CppHDL 中**通用 AXI4 Full slave**（`include/axi4/axi4_full.h` 的 `Axi4Slave`）
的协议规范形态与**多 beat burst** 语义契约。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并 delta 文件的 `## Purpose`**
> （新 capability 的主 spec 会被建成 `## Purpose / TBD - created by archiving change ...` 骨架）。
> 因此凡 MUST 长期存在的契约性内容，一律写进 **Requirement 正文**（正文会被整条合并），
> 不依赖本 `## Purpose`。archive 后 MUST 另行补写主 spec 的 `## Purpose`。

与既有 capability 的分工：

- `axi-slave-conformance` 管辖 **AXI4-Lite** 的通用 slave（`AxiLiteSlave`）
  与 AXI→AXI4-Lite 适配器（`AxiToAxiLiteConverter`）；
  其正文中已**显式声明** `Axi4Slave`（AXI4 **Full**）不在其管辖范围，
  并指向本 capability。
- `dual-backend-test-coverage` 管辖「同一场景在 interpreter 与 verilator 两后端的一致性」；
  本 capability **不重复**该维度，只在新增行为引入时要求双后端覆盖。
- **字节道（WSTRB）语义以 `dual-backend-test-coverage` 的「AXI4 peripheral MUST 支持字节写」
  Requirement 为准**（该 Requirement 的适用范围已包含 `Axi4Slave`）；
  本 capability 只在「burst 内逐 beat 的 WSTRB」这一点上引用它。

**复位措辞**：本 capability 使用「**低有效，在时钟沿生效**」——
`select(!io().rst_n, <init_val>, ...)` 落在 `ch_reg` 的 `next` 表达式上，由时钟沿采样
（`include/core/node_builder.h:180-182` 的 `ch_reg` 原生复位接的是 context 的
`default_reset`，组件级 `rst_n` 只能走 `next` 表达式），实为**沿上生效**。

## ADDED Requirements

### Requirement: 通用 AXI4 Full slave MUST 提供低有效复位

`Axi4Slave` MUST 暴露 `ch_in<ch_bool> rst_n` 端口（低有效，**在时钟沿生效**），
且其全部状态寄存器 MUST 使用 `select(!io().rst_n, <init_val>, ...)` 复位 pattern，
`<init_val>` MUST 携带该寄存器的显式位宽（如 `ch_uint<32>(0_d)`）。

**适用范围与措辞说明（契约的一部分）**：

1. 本 Requirement 管辖 `Axi4Slave`（`include/axi4/axi4_full.h`）。
   `AxiLiteSlave`（`include/axi4/axi4_lite_slave.h`）由 `axi-slave-conformance` 管辖。
2. 本 Requirement 与 `axi-slave-conformance` 的对应 Requirement **同源**
   （同一套 `rst_n` + 复位 pattern 形态），**两者 MUST NOT 出现形态分歧**。
3. 复位措辞 MUST 用「**低有效，在时钟沿生效**」，MUST NOT 写「低有效**异步**复位」。

#### Scenario: 复位清零全部状态寄存器

- **WHEN** `rst_n` 为 0 并经过至少 1 个时钟沿
- **THEN** 写/读状态机、地址锁存寄存器、burst 计数器、ID 锁存寄存器
  MUST 全部读回 0
- **AND** 16 个数据寄存器 MUST 读回各自的初值
- **AND** 复位 MUST NOT 依赖外部 testbench 预先清零数据

#### Scenario: 未接线的 rst_n 恒为永久复位（回归护栏）

- **WHEN** 某实例化点未驱动 `rst_n`
- **THEN** 该实例的 `rst_n` 读出为 0 → 恒为复位态
- **AND** 该陷阱 MUST 在 `include/axi4/AGENTS.md` 登记
- **AND** 全仓 `Axi4Slave` 的实例化点 MUST 在本 change 中被逐个核查并接线

### Requirement: 通用 AXI4 Full slave MUST 收敛到 AW→W 相位分离 + 响应保持的规范形态

`Axi4Slave` MUST 与仓库其余 AXI slave（6 个 peripheral 与 `AxiLiteSlave`）**同形**：

1. **AW→W 相位分离**：W 可在 AW 之后的任意周期被接受。
   ⚠️ 由于 `aw_done` 是**寄存器**（表达式中读到的是**当前值**），
   **同拍 AW+W 时该拍 W 不被接受** —— master MUST 保持 `WVALID` 到 `WREADY`
   （AXI 合规 master 的义务）。这是规范形态的固有性质，**不是缺陷**。
2. **地址锁存**：写/读地址 MUST 在 AW/AR 握手时锁存进寄存器，
   解码 MUST 基于**锁存**的地址，MUST NOT 基于活的 `io().awaddr` / `io().araddr`。
3. **响应保持**：`bvalid` MUST 保持到 `bready`，`rvalid` MUST 保持到 `rready`。
4. **置 1 条件 MUST 优先于清 0 条件**（`b_valid` / `r_valid`）——
   写成清 0 优先会在 `bready` 常态拉高时让 `b_valid` 永远为 0。
5. **单 outstanding 事务（按方向独立）**：
   - **写通道**与**读通道**各自最多 1 笔在途事务；两者**互不阻塞**
     （AXI4 Full 的 AW/W/B 与 AR/R 是独立通道组，AXI4 允许读写并发）。
   - 写方向：新的 AW MUST NOT 在上一笔**写**事务真正完成（B 握手）前被接受。
   - 读方向：新的 AR MUST NOT 在上一笔**读**事务真正完成（R 握手）前被接受。
   - ⚠️ 本条**不是**共享 `busy`（`AxiLiteSlave` 的形态是读写互斥，见
     `openspec/specs/axi-slave-conformance/spec.md`）。
     把 `Axi4Slave` 降级为读写互斥属于**行为回退**，MUST NOT 发生。
6. **`bid` / `rid` MUST 是握手时锁存的 ID**（既有行为保留：
   `include/axi4/axi4_full.h` 的 `write_id_reg` / `read_id_reg` 已在 AW/AR 握手时锁存）。

#### Scenario: AW 与 W 相隔多个周期仍能完成写事务

- **WHEN** master 在 AW 握手后等待 N 个周期（N ≥ 1）才拉高 `WVALID`
- **THEN** `wready` MUST 在 `WVALID` 拉高后的某一拍拉高
- **AND** 该 W beat MUST 被接受并写入正确的寄存器
- **AND** `bvalid` MUST 在 W 被接受后拉高并保持到 `bready`

#### Scenario: 同拍 AW+W 不被接受（既有行为，非本 change 引入）

- **WHEN** master 在同一拍拉高 `AWVALID` 与 `WVALID`
- **THEN** 该拍的 W MUST NOT 被接受
- **AND** `wready` MUST 在 AW 握手之后的某一拍才拉高
- **AND** master MUST 保持 `WVALID` 到 `WREADY`（AXI 合规 master 的义务）
- **AND** ⚠️ 本行为是 `Axi4Slave` 的**既有行为**（`wready` 由 `write_state == 1` 门控，
  而 state 1 只能在 AW 握手后一拍进入），
  本 change MUST NOT 把它描述为「行为变化」

#### Scenario: AW 握手后改写 AWADDR 不影响落点

- **WHEN** master 完成 AW 握手后改写 `io().awaddr` 为另一个寄存器的地址
- **AND** 随后提供 W 数据
- **THEN** 数据 MUST 写入 **AW 握手时锁存的**那个寄存器
- **AND** 被改写后的地址对应的寄存器 MUST 保持原值

#### Scenario: BVALID 保持到 BREADY

- **WHEN** 写事务完成且 master 未拉高 `bready`
- **THEN** `bvalid` MUST 持续为 1（不得是组合单拍脉冲）
- **AND** `bvalid` MUST 在 `bready` 拉高后撤销

### Requirement: 通用 AXI4 Full slave MUST 支持多 beat 写 burst

`Axi4Slave` MUST 在一笔 AW 事务内接受 `awlen + 1` 个 W beat
（AXI 的 `awlen` 编码是「beat 数 − 1」）。

1. 写状态机 MUST 在整笔 burst 期间持续拉高 `wready`（直到 `wlast` 到达或长度超限），
   MUST NOT 在第 1 个 beat 之后停止。
   → burst 结束后 `wready` MUST 撤销：本笔事务**不再接受**额外的 W beat
     （否则越界 beat 会静默写入寄存器）。
2. 目标地址 MUST 按 burst 类型逐 beat 计算（见下一条 Requirement），
   MUST NOT 恒用同一个地址索引。
3. 每一拍 MUST 用**当拍**的 `io().wstrb` 重新展开字节道掩码
   （MUST NOT 在 AW 拍锁存 `wstrb`）。
4. `wlast` MUST 与预期的 beat 数一致：
   - 第 `awlen + 1` 个 beat 携带 `wlast = 0`（`wlast` 缺失）→
     该笔事务 MUST 以 `SLVERR` 结束
   - 第 `awlen + 1` 个 beat 之前就携带 `wlast = 1`（`wlast` 提前）→
     该笔事务 MUST 以 `SLVERR` 结束
   - ⚠️ MUST NOT 表述为「beat 数超过 `awlen + 1`」—— 检测「超过」要求先接受
     第 `awlen + 2` 个 beat，与「只接受 `awlen + 1` 个」自相矛盾
5. `awlen + 1 > MAX_BURST_LEN` → 该笔事务 MUST 以 `SLVERR` 结束，
   MUST NOT 静默截断。
   → 该笔事务的 W beat **不被写入**（事务整体被拒绝），响应为 `SLVERR`。
6. **INCR burst 越界**：burst 中途某 beat 的地址跨出已实现寄存器窗口时，
   该 beat MUST 被抑制（写：寄存器保持原值；读：`rdata = 0`），
   且该笔事务 MUST 以 `DECERR` 结束。

#### Scenario: 4-beat INCR 写写入 4 个不同寄存器

- **WHEN** master 发出 `awlen = 3`（4 beat）、`awburst = INCR`、`awsize = 2`（4 字节）的 AW
- **AND** 随后提供 4 个 W beat，每个 beat 带各自的 `wstrb` 与 `wdata`
- **THEN** 4 个 beat MUST 分别写入 4 个**连续**的寄存器
- **AND** 第 4 拍 MUST 携带 `wlast = 1`
- **AND** `bresp` MUST 为 `OKAY`

#### Scenario: 逐 beat 消费 wstrb

- **WHEN** 同一笔 burst 的第 1 拍 `wstrb = 0xF`、第 2 拍 `wstrb = 0x1`
- **THEN** 第 1 拍 MUST 写满 4 个字节道
- **AND** 第 2 拍 MUST 只写 byte0，其余 3 字节 MUST 保持第 1 拍的值

#### Scenario: burst 长度不符以 SLVERR 结束

- **WHEN** master 声明 `awlen = 3`（4 beat）但只提供 2 个 beat 即拉高 `wlast`
- **THEN** `bresp` MUST 为 `SLVERR`
- **AND** 已写入的 beat MUST NOT 被静默回滚（部分写入 + 错误响应是 AXI 允许的行为）

#### Scenario: 超过 MAX_BURST_LEN 的 burst 以 SLVERR 结束

- **WHEN** master 发出 `awlen` 使 beat 数超过 `MAX_BURST_LEN`
- **THEN** `bresp` MUST 为 `SLVERR`
- **AND** MUST NOT 静默接受前 `MAX_BURST_LEN` 个 beat 并返回 `OKAY`

### Requirement: 通用 AXI4 Full slave MUST 支持多 beat 读 burst

`Axi4Slave` MUST 在一笔 AR 事务内返回 `arlen + 1` 个 beat。

1. 每一拍的 `rdata` MUST 取**该拍对应地址**的寄存器值，
   MUST NOT 恒返回首拍的值。
2. `rlast` MUST 只在**最后一个 beat** 拉高。
3. 读 beat 计数 MUST 与**锁存的** `arlen` 比较，MUST NOT 依赖活的 `io().arlen`。
   → 被拒绝的读事务（burst 类型不支持 / 超 `MAX_BURST_LEN`）每个 beat MUST 呈现
     `rdata = 0` 且 `rresp = SLVERR`。
4. 读地址 MUST 按 burst 类型逐 beat 计算。
5. **`rvalid` 的形态 MUST 明确**：本 capability 采用**流式**形态 ——
   `rvalid` 在整个 burst 期间**连续保持为 1**，每拍呈现一个 beat 的数据，
   直到 `rready` 接受该 beat 为止；MUST NOT 采用「每 beat 独立等待 `rready` 再出下一拍」
   的形态（后者会让 master 看到 `rvalid` 断续，行为与 6 个 peripheral 不一致）。
6. ⚠️ **写侧的末拍判定 MUST 用「下一个值」**（`next_beat_cnt == awlen + 1` 之类），
   MUST NOT 用当前寄存器值比较（`beat_cnt == awlen`）——
   仓库已登记该坑（`include/axi4/AGENTS.md` ANTI-PATTERNS：
   UART RX 引擎因用当前值判定结束而只采到 7 位）。
   同理，burst 结束判定 MUST 基于 `w_handshake` 组合出的**下一状态**，
   MUST NOT 基于当前寄存器值。
7. ⚠️ **读侧的 `rlast` MUST 与「正在呈现的 beat」同源**：
   `rdata` 与 `rlast` MUST 由**同一个** beat 索引驱动
   （该索引 = 已被 `rready` 接受的 beat 数），即 `rlast = (beat_idx == arlen)`
   —— 这里用**当前值**是**正确**的，MUST NOT 改用「下一值」形式
   （`next_beat_cnt == arlen`）：
   流式形态下末拍在 `rready` 拉低期间**仍停留在总线上**，
   「下一值」形式会在该周期给出错误的 `rlast = 0`
   （master 会认为 burst 未结束 → 末拍被重复接受 / 事务挂起）。
   「下一个值」规则针对的是**写侧**的「本拍是否为末拍」判定
   与「burst 结束判定基于 `w_handshake` 的下一状态」这两条。
8. **INCR burst 越界**：某 beat 的地址跨出已实现窗口时，
   该 beat MUST 呈现 `rdata = 0` 且 `rresp = DECERR`，
   该笔事务 MUST 以 `DECERR` 结束（与写侧对称）。

#### Scenario: 4-beat INCR 读依次返回 4 个不同数据

- **WHEN** 4 个连续寄存器被写入不同值
- **AND** master 发出 `arlen = 3`、`arburst = INCR`、`arsize = 2` 的 AR
- **THEN** `rdata` MUST 依次返回 4 个不同的值，顺序与写入顺序一致
- **AND** `rlast` MUST 只在第 4 个 beat 为 1

### Requirement: 通用 AXI4 Full slave MUST 显式处理 burst 类型

`Axi4Slave` MUST 按 `awburst` / `arburst` 区分地址推进方式：

1. `INCR`（`0b01`）：地址每 beat 递增 `2^awsize` / `2^arsize` 字节
   （MUST 使用端口上的 `awsize` / `arsize`，MUST NOT 硬编码 4 字节）
   - ⚠️ `2^awsize` **MUST NOT 用运行期移位**计算 ——
     `ch_uint::operator<<` 按右操作数类型宽度推算结果宽度
     （`include/axi4/AGENTS.md` 已登记该坑）→ MUST 用 `select` 链查表
     （`awsize` 0..7 → 1/2/4/8/16/32/64/128 字节）
2. `FIXED`（`0b00`）：地址每 beat 保持不变
3. `WRAP`（`0b10`）：本 capability **不要求**支持；
   MUST 以 `SLVERR` **明确拒绝**，MUST NOT 静默当作 `INCR` 处理
   （静默降级是数据损坏级别的缺陷）
   → 该笔事务的 W beat **不被写入**（事务整体被拒绝）
4. **步长与总线宽度的关系**：解码步长固定为 4 字节（`addr >> 2`），
   而寄存器宽度为 `DATA_WIDTH`。当 `DATA_WIDTH == 64` 且 `awsize < 3` 时，
   相邻 beat 会落到**同一个** 64 位寄存器（窄传输）。
   本 capability **不实现窄传输的字节道重编号**（Non-Goal），
   MUST 在文档中显式声明该限制，MUST NOT 静默产生错误的数据落点。

**说明**：`AxiBurst` 枚举（`include/axi4/axi4_full.h`）的 `FIXED` / `INCR` / `WRAP`
MUST 与 AXI 规范的 `AxBURST` 编码一致。

#### Scenario: FIXED burst 写同一地址

- **WHEN** master 发出 `awburst = FIXED` 的 4-beat 写
- **THEN** 4 个 beat MUST 全部写入同一个寄存器
- **AND** 该寄存器的最终值 MUST 等于第 4 个 beat 的写入结果

#### Scenario: 不支持的 burst 类型以 SLVERR 拒绝

- **WHEN** master 发出 `awburst = WRAP`（`0b10`）的写事务
- **THEN** `bresp` MUST 为 `SLVERR`
- **AND** MUST NOT 把该事务当作 `INCR` 执行

### Requirement: 通用 AXI4 Full slave 的模板参数 MUST 被 honor

`Axi4Slave` 的 `NUM_REGS` 与 `MAX_BURST_LEN` 模板参数 MUST 真正生效。

1. `MAX_BURST_LEN` MUST 作为 burst 长度上限参与检查（超限 → `SLVERR`）。
   ⚠️ `awlen` 端口宽度为 4 bit（物理上限 16 beat），
   `MAX_BURST_LEN > 16` 恒空转 → MUST 有 `static_assert(MAX_BURST_LEN <= 16)`
   暴露（把 `awlen` 宽度参数化属公开 API 变更，**不在本 change 范围**）。
2. `NUM_REGS` MUST 决定寄存器数量。MUST NOT 用「变长数组」实现
   （本仓库无寄存器数组原语，变长数组会触发 CHWARN + O(N) 节点爆炸）。
   可行路径：把寄存器集合固定为具名的 16 个 + 编译期 `static_assert` 暴露
   与实际值不符的 `NUM_REGS`（与 `AxiLiteSlave` 的处理同源），
   或把参数改名为明确「固定 16」语义的名称。
   ⚠️ 这两条路径是**互斥的决策点**，实施前 MUST 在 change 内显式裁决其一
   （不得由实施者临场随机选择）。
   补充事实：现有 3 个实例化点全部使用**位置参数**
   （`Axi4Slave<32,32,4,16,16>`）→ 参数**改名**对它们源码兼容，
   实际 API 破坏面比直觉小。
3. `DATA_WIDTH` MUST ≤ 64：`include/axi4/byte_lane.h` 的 `static_assert` 已封顶 64，
   且 Verilator 后端拒绝 >64 bit 端口（ADR-035 R3）
   → MUST 有 `static_assert(DATA_WIDTH <= 64)`。
   ⚠️ `include/axi4/axi4_full.h:9` 的头注释当前宣称支持 128–1024 位数据宽度，
   与该上限矛盾 → MUST 同步修正头注释。
4. 任一参数未被 honor → MUST 用 `static_assert` 在**编译期**暴露，MUST NOT 静默接受。

#### Scenario: 模板参数与实际实现不符时编译失败

- **WHEN** 有人以 `NUM_REGS = 8` 实例化 `Axi4Slave`
- **THEN** 编译 MUST 失败并给出明确的 `static_assert` 诊断
- **AND** MUST NOT 静默生成 16 个寄存器

### Requirement: 通用 AXI4 Full slave MUST 对异常地址与异常状态返回非 OKAY 响应

1. 落在已实现寄存器窗口之外的地址 → `bresp` / `rresp` MUST 为 `DECERR`（`0b11`），
   MUST NOT 别名到已实现寄存器，MUST NOT 静默丢弃写。
2. 已映射地址但 `wstrb == 0` 的空写 → 响应 MUST 仍为 `OKAY`（`0b00`），
   寄存器值 MUST 保持不变。
3. `bresp` / `rresp` MUST NOT 恒为 `OKAY`。
4. `bid` / `rid` MUST 是握手时锁存的 ID，MUST NOT 组合直出。

#### Scenario: 未映射地址写返回 DECERR

- **WHEN** master 写一个落在已实现窗口之外的地址
- **THEN** `bresp` MUST 为 `DECERR`
- **AND** 任何已实现寄存器 MUST 保持原值

#### Scenario: 空写返回 OKAY

- **WHEN** master 对一个已映射地址发出 `wstrb = 0` 的写
- **THEN** `bresp` MUST 为 `OKAY`
- **AND** 目标寄存器 MUST 保持原值
