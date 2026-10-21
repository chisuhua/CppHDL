# axi-slave-conformance Specification

## Purpose
定义 CppHDL 中**通用 AXI4-Lite slave**（`include/axi4/axi4_lite_slave.h` 的 `AxiLiteSlave`）
与 **AXI→AXI4-Lite 适配器**（`include/axi4/axi_to_axilite.h` 的 `AxiToAxiLiteConverter`）
的协议规范形态契约。

与 `dual-backend-test-coverage` 的分工（**按适用对象**分界，不是按「轴」分界）：
该 capability 的「AXI4-Lite slave peripheral MUST 收敛到相位分离 + 响应保持的规范形态」
Requirement 管辖 **6 个 `include/axi4/peripherals/*.h` 的 AXI4-Lite peripheral**；
本 capability 管辖**通用 AXI slave**（`AxiLiteSlave`；`Axi4Slave` 属 AXI4 **Full**，
其规范形态与 burst 语义由 `axi4-full-slave-burst` 承载）与**适配器**。
两者规则**同源**（同一套 `busy` / `aw_done` / `b_valid` / `r_valid` 四件套形态），
适用范围不同。`dual-backend-test-coverage` 另管辖「同一场景在 interpreter 与
verilator 两后端的一致性」——本 capability **不重复**该维度，只在新增行为引入时
要求双后端覆盖。

**字节道（WSTRB）语义以 `dual-backend-test-coverage` 的「AXI4 peripheral MUST 支持字节写」
Requirement 为准**（该 Requirement 的适用范围已包含 `AxiLiteSlave`）；本 capability
只在「空写（`wstrb == 0`）的响应」这一点上引用它。

**复位措辞**：本 capability 使用「**低有效，在时钟沿生效**」——
`select(!io().rst_n, <init_val>, ...)` 落在 `ch_reg` 的 `next` 表达式上，由时钟沿采样
（`include/core/node_builder.h:180-182` 的 `ch_reg` 原生复位接的是 context 的
`default_reset`，组件级 `rst_n` 只能走 `next` 表达式），实为**沿上生效**。
## Requirements
### Requirement: 通用 AXI4-Lite slave MUST 提供低有效复位

`AxiLiteSlave` MUST 暴露 `ch_in<ch_bool> rst_n` 端口（低有效，**在时钟沿生效**），
且其全部状态寄存器 MUST 使用 `select(!io().rst_n, <init_val>, ...)` 复位 pattern。

**适用范围与措辞说明（契约的一部分）**：

1. 本 Requirement 管辖 `AxiLiteSlave`（`include/axi4/axi4_lite_slave.h`）。
   `Axi4Slave`（`include/axi4/axi4_full.h`，AXI4 **Full**）的规范形态与 burst 语义
   由独立 capability `axi4-full-slave-burst` 承载
   （`openspec/specs/axi4-full-slave-burst/spec.md`），
   **不在本 Requirement 范围**。
   ⚠️ MUST NOT 用「未实现的 change 名」做交叉引用 ——
   change 归档后目录会移入 `openspec/changes/archive/`，
   指向 change 名的引用会成为 dangling 指针；MUST 指向 **capability**。
2. 本 Requirement 与 `dual-backend-test-coverage` 的「AXI4-Lite slave peripheral MUST 收敛到
   相位分离 + 响应保持的规范形态」Requirement **同源**（同一套四件套形态），
   区别在**适用对象**：那条管辖 6 个 `include/axi4/peripherals/*.h` 的 AXI4-Lite peripheral，
   本条管辖通用 slave。**两者 MUST NOT 出现形态分歧。**
3. 复位措辞：本 Requirement 用「**低有效，在时钟沿生效**」。
   `dual-backend-test-coverage` 与 peripheral 头注释中的「低有效**异步**复位」措辞不准确 ——
   `select(!io().rst_n, <init_val>, ...)` 落在 `ch_reg` 的 `next` 表达式上，由时钟沿采样
   （`include/core/node_builder.h:180-182` 的 `ch_reg` 原生复位接的是 context 的
   `default_reset`，组件级 `rst_n` 只能走 `next` 表达式），实为**沿上生效**。
   措辞统一由后续 change `axi4-eval-order-doc-correction` 收口。
4. **字节道（WSTRB）语义以 `dual-backend-test-coverage` 的「AXI4 peripheral MUST 支持字节写」
   Requirement 为准**（该 Requirement 的适用范围已包含 `AxiLiteSlave`）。
   本 capability 只在「空写（`wstrb == 0`）的响应」这一点上引用它（MUST 仍为 `OKAY`），
   MUST NOT 另立一套字节道语义。

#### Scenario: 复位清零全部状态寄存器

- **WHEN** `rst_n` 为 0 并经过至少 1 个时钟沿
- **THEN** `busy` / `aw_done` / `b_valid` / `r_valid` / `addr_reg` / `ar_addr_reg`
  MUST 全部读回 0
- **AND** 4 个数据寄存器 MUST 读回各自的初值
- **AND** 复位 MUST NOT 依赖外部 testbench 预先清零数据

#### Scenario: 复位提供逃生路径

- **WHEN** 事务进行中被 `rst_n` 打断
- **THEN** slave MUST 在下一个时钟沿回到空闲态（`busy == 0`）
- **AND** MUST 能接受下一笔事务

#### Scenario: 未接线的 rst_n 等价于永久复位

- **WHEN** 实例化点未驱动 `rst_n`（读出为 0）
- **THEN** slave MUST 保持在复位态（全部状态寄存器为初值）
- **AND** 接线要求见 `include/axi4/AGENTS.md` CONVENTIONS → 复位
  （`rst_n` 是**必需端口**，实例化点 MUST 显式接线）

### Requirement: 通用 AXI4-Lite slave MUST 锁存地址

slave MUST 在 AW / AR 握手时把 `io().awaddr` / `io().araddr` 锁存进寄存器，
写侧与读侧的地址解码 MUST 基于**锁存**的地址。

**地址空间定义**：

- 解码步长 MUST 固定为 **4 字节**（`addr >> 2`），**与 `DATA_WIDTH` 无关**
  （`AxiLiteSlave<32,64,4>` 的寄存器同样位于字节地址 `0x00` / `0x04` / `0x08` / `0x0C`）；
- 已实现窗口 = 字节地址 `0x00..0x0C`（4 个寄存器）；
- 窗口外（含 `0x10` 及以上、含低位与已实现寄存器相同的地址如 `0x100`）MUST 视为
  **未映射**，MUST NOT 别名到已实现寄存器。

#### Scenario: 握手后改写写地址不改变写目标

- **WHEN** master 在 AW 握手完成后改写 `awaddr`（AXI 允许）
- **THEN** 该笔事务 MUST 仍然写入 AW 握手时的目标寄存器
- **AND** MUST NOT 写入改写后的地址对应的寄存器

#### Scenario: 握手后改写读地址不改变读目标

- **WHEN** master 在 AR 握手完成后改写 `araddr`
- **THEN** 该笔读事务 MUST 返回 AR 握手时目标寄存器的值
- **AND** `rdata` MUST NOT 随活的 `io().araddr` 抖动

#### Scenario: 地址锁存的静态检查形式

- **WHEN** reviewer 对 `include/axi4/axi4_lite_slave.h` 执行
  `grep -nE 'addr_bits = io\(\)\.aw?r?addr'`
- **THEN** MUST 命中 0 行
- **AND** MUST 存在 `addr_reg` 与 `ar_addr_reg` 两个 `ch_reg`

### Requirement: 通用 AXI4-Lite slave MUST 支持 AW 与 W 相位分离

slave MUST 在 AW 握手之后接受**任意周期**到达的 W 数据。

slave 的握手语义 MUST 与 6 个 AXI4-Lite peripheral 的规范形态**逐字同形**
（`include/axi4/peripherals/axi_gpio.h` 的形式）：

- `aw_handshake = select(io().awvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false))`
- `io().awready = aw_handshake`
- `w_handshake = select(io().wvalid, aw_done, ch_bool(false))`
- `io().wready = w_handshake`

由于 `aw_done` 是**寄存器**（表达式中读到的是**当前值**而非 next 值），
AW 握手当拍 `aw_done` 仍为 0 → **同拍的 W 不被接受**，这是规范形态的固有性质。

#### Scenario: W 可延后多个周期被接受

- **WHEN** master 在 AW 握手后延后 N 个周期（N ≥ 1）才拉高 `wvalid`
- **THEN** 写事务 MUST 在该 W 拍被接受
- **AND** 写目标 MUST 是 AW 握手时锁存的地址

#### Scenario: 同拍 AW 与 W 时 W 不被接受

- **WHEN** master 在同一拍拉高 `awvalid` 与 `wvalid`
- **THEN** 该拍的 `wready` MUST 为 0（W 不被接受）
- **AND** master MUST 保持 `wvalid` 直到 `wready` 拉高（AXI 合规 master 的义务），
  slave MUST 在随后一拍接受该 W
- **NOTE**: 这是与 6 个 peripheral 一致的规范形态，**不是缺陷**

#### Scenario: 不得使用组合单拍形式的 wready

- **WHEN** reviewer 阅读 `io().wready` 的赋值
- **THEN** MUST NOT 出现 `io().wready = select(io().wvalid, aw_handshake, ch_bool(false))`
  （`aw_handshake` 是组合量，该形式会把「AW 已接收」与「W 已被接受」折叠成同一拍，
  使 AW→W 相位分离失效）
- **AND** MUST NOT 出现 `io().wready = w_handshake` 且 `w_handshake` 由组合量派生

### Requirement: 通用 AXI4-Lite slave 的写响应 MUST 保持到 BREADY

slave 的 `bvalid` MUST 在写数据被接受后置位，并**保持到** `bready` 握手完成；
MUST NOT 是组合单拍脉冲。`busy` MUST 仅在写事务**真正完成**
（`b_valid && bready`）时释放。

**四个状态寄存器的 next 语义（置 1 条件优先于清 0 条件）**：

| 寄存器 | 置 1 条件 | 清 0 条件 | 输出 |
|---|---|---|---|
| `busy` | `aw_handshake` 或 `ar_handshake` | `b_valid && bready` 或 `r_valid && rready` | — |
| `aw_done` | `aw_handshake` | `b_valid && bready` | — |
| `b_valid` | `w_handshake` | `bready` | `bvalid = b_valid` |
| `r_valid` | `ar_handshake` | `rready` | `rvalid = r_valid` |

「置 1 条件优先于清 0 条件」是**强制**的：若写成清 0 优先
（`select(io().bready, false, select(w_handshake, true, b_valid))`），
则 `bready` 常态拉高时 `b_valid` 永远为 0 → `bvalid` 永不拉高 → master 永等不到 BVALID
（死锁），且 `aw_done` 因 `b_valid == 0` 永不清零 → 同一笔 W 被反复接受（重复写）。

#### Scenario: master 不拉 BREADY 时 bvalid 保持

- **WHEN** 写数据被接受且 master 未拉 `bready`
- **THEN** `bvalid` MUST 保持为 1（跨周期不消失）
- **AND** slave MUST NOT 接受下一笔写事务（`awready` 保持 0）

#### Scenario: BREADY 握手后 bvalid 清除

- **WHEN** `bvalid` 为 1 且 master 拉高 `bready`
- **THEN** `bvalid` MUST 在该次握手后清零
- **AND** slave MUST 能接受下一笔事务

#### Scenario: 不得在裸 bready 上释放 busy

- **WHEN** master 在事务进行中常态拉高 `bready`
- **THEN** `busy` MUST NOT 在 `bready` 为高时提前释放
- **AND** MUST NOT 出现两笔写事务交叠

### Requirement: 通用 AXI4-Lite slave 的读响应 MUST 保持到 RREADY

slave 的 `rvalid` MUST 在 AR 握手后置位，并**保持到** `rready` 握手完成；
MUST NOT 是组合单拍脉冲。`busy` MUST 仅在读事务**真正完成**
（`r_valid && rready`）时释放。

#### Scenario: master 不拉 RREADY 时 rvalid 保持

- **WHEN** AR 被接受且 master 未拉 `rready`
- **THEN** `rvalid` MUST 保持为 1（跨周期不消失）
- **AND** `rdata` MUST 保持为锁存地址对应的寄存器值

#### Scenario: RREADY 握手后 rvalid 清除

- **WHEN** `rvalid` 为 1 且 master 拉高 `rready`
- **THEN** `rvalid` MUST 在该次握手后清零
- **AND** slave MUST 能接受下一笔事务

### Requirement: 通用 AXI4-Lite slave MUST 对未映射地址返回 DECERR

slave MUST 对其地址空间之外的访问返回 `DECERR`（AXI 响应编码 `0b11`），
且 MUST NOT 把未映射地址**别名**到已实现的寄存器。已解码但 `wstrb == 0`
的写（空写）响应 MUST 仍为 `OKAY`。

**响应值的有效期**：`bresp` / `rresp` 是组合输出（依锁存地址的解码结果），
其值仅在对应 `bvalid` / `rvalid` 为 1 的拍**有效**；空闲拍上的取值 MUST NOT 被解释为
一笔事务的响应。

#### Scenario: 未映射地址写被拒绝

- **WHEN** master 写一个未映射地址
- **THEN** `bresp` MUST 为 `DECERR`（`0b11`）
- **AND** 任何寄存器 MUST 保持原值

#### Scenario: 未映射地址读被拒绝

- **WHEN** master 读一个未映射地址
- **THEN** `rresp` MUST 为 `DECERR`（`0b11`）
- **AND** `rdata` MUST 为 0（MUST NOT 呈现其它寄存器的值）

#### Scenario: 别名地址不被别名到已实现寄存器

- **WHEN** master 访问窗口外但低位与已实现寄存器相同的地址
  （例如 `AxiLiteSlave` 的 `0x100`）
- **THEN** 响应 MUST 为 `DECERR`
- **AND** 对应寄存器 MUST 保持原值

#### Scenario: 空写的响应仍为 OKAY

- **WHEN** master 写一个**已映射**地址但 `wstrb == 0`（无字节被选中）
- **THEN** `bresp` MUST 为 `OKAY`（`0b00`）
- **AND** 寄存器 MUST 保持原值

### Requirement: 通用 AXI4-Lite slave 的状态寄存器 MUST 遵循单处 next 赋值

每个 `ch_reg` 在同一文件内 MUST 只有**一处** `X->next =` 赋值；
复位 pattern MUST 包裹在该唯一赋值内（不得在别处追加复位赋值）。

#### Scenario: 每个 ch_reg 恰好一处 next 赋值

- **WHEN** reviewer 对 `include/axi4/axi4_lite_slave.h` 中每个 `ch_reg` 统计
  `X->next =` 的出现次数
- **THEN** MUST 恰好为 1
- **AND** 该唯一赋值 MUST 包含 `select(!io().rst_n, <init_val>, ...)` 外层

#### Scenario: 复位初值必须带显式位宽

- **WHEN** reviewer 阅读复位 pattern 的 `<init_val>`
- **THEN** MUST 显式带寄存器位宽（如 `ch_uint<32>(0_d)`、`ch_bool(false)`），
  MUST NOT 是裸 `0_d`

#### Scenario: 禁止逻辑 overload

- **WHEN** 对 `include/axi4/axi4_lite_slave.h` 执行
  `grep -nE 'io\(\)\.[a-z_]+ *(&&|\|\|)'`
- **THEN** MUST 命中 0 行
- **AND** 编译 MUST NOT 产生 `-Wdeprecated-declarations`
- **NOTE**: 与 `dual-backend-test-coverage` 的「禁止逻辑 overload（ADR-036）」同源；
  通用 slave 与 6 个 peripheral MUST 一致

### Requirement: AXI 到 AXI4-Lite 转换器 MUST 透传复位

`AxiToAxiLiteConverter` MUST 暴露 `ch_in<ch_bool> rst_n` 端口并透传给下游
AXI4-Lite slave；其状态寄存器 MUST 使用 `select(!io().rst_n, <init_val>, ...)` 复位 pattern。

#### Scenario: 转换器把 rst_n 传给下游

- **WHEN** reviewer 阅读 `AxiToAxiLiteConverter::describe()`
- **THEN** MUST 存在 `axil_rst_n` 形式的输出端口
- **AND** MUST 被下游 AXI4-Lite slave 的 `rst_n` 驱动

#### Scenario: 转换器状态机可被复位打断

- **WHEN** `rst_n` 为 0 并经过至少 1 个时钟沿
- **THEN** 转换器的写状态机与读状态机 MUST 回到 IDLE

#### Scenario: 不得把 rst_n tie 成常量

- **WHEN** reviewer 检查实例化点
- **THEN** MUST NOT 出现常量 1 绑定的 `rst_n`（会绕过复位语义并掩盖问题）

#### Scenario: 层级 rst_n 的传递 MUST 在同一拍内生效

- **WHEN** 复位信号经组件层级传递（父 → 转换器 → slave）
- **THEN** 子组件 MUST 在**同一拍**内看到父级的复位值
  （`context::topological_sort_visit` 沿 `src` 边递归
  （`src/core/context.cpp:273`）→ 父的 `ch_out` 排在子组件的 `ch_in` 之前，
  子组件的 `ch_in` 又排在读它的组合节点之前）
- **AND** 接线语句在 `describe()` 中的位置 MUST NOT 被当作求值顺序的保证
  （`operator<<=` 只做 `set_driver` / `set_src`，**不创建节点**）
- **NOTE**: 组合求值是**单遍**、不迭代到不动点；纯组合环会在
  `Simulator::initialize()` 触发 `CHFATAL`（响亮失败，非静默错误），
  寄存器则天然打断组合环 → `reg->next = f(reg_current, ...)` 的 hold 模式是安全的

### Requirement: 通用 AXI4-Lite slave 的未实现模板能力 MUST 在编译期暴露

`AxiLiteSlave` 的模板参数 MUST NOT 是「接口谎言」：若某参数在实现中未被 honored，
MUST 用 `static_assert` 在编译期报错，MUST NOT 静默接受。

#### Scenario: NUM_REGS 与实现一致

- **WHEN** 开发者写 `AxiLiteSlave<32, 32, 8>`（实现只有 4 个寄存器）
- **THEN** 编译 MUST 失败，且错误信息 MUST 含 `NUM_REGS` 字样
- **AND** `AxiLiteSlave<32, 32, 4>` MUST 编译通过

#### Scenario: static_assert 存在性

- **WHEN** reviewer 对 `include/axi4/axi4_lite_slave.h` 执行
  `grep -n "static_assert"`
- **THEN** MUST 命中至少 1 行
- **AND** 该断言 MUST 约束 `NUM_REGS`

