# AGENTS.md - AXI4 Bus & Peripherals

Child of `include/AGENTS.md` → root `AGENTS.md` (ZERO-DEBT POLICY + PHASE GATES).

## OVERVIEW
AXI4 bus protocol (Full/Lite) + 7 peripherals. All header-only, `ch::Component` subclass hierarchy.

## STRUCTURE
```
include/axi4/
├── axi4_full.h              # AXI4 Full slave (5 channels, burst)
├── axi4_lite_slave.h        # Simplified Lite slave (4-register state machine)
├── axi4_lite_matrix.h       # 2x2 AXI4-Lite crossbar (544 lines)
├── axi_interconnect_4x4.h   # 4x4 full AXI crossbar (1094 lines — most complex)
├── interconnect_common.h    # 互连层共用契约类型（地址窗口 + 默认映射 + 合法性校验）
├── axi_resp.h               # AXI 响应码（`AxiResp`，**全 AXI 家族唯一权威**）
├── axi_to_axilite.h        # AXI ↔ AXI-Lite bidirectional converter
├── byte_lane.h             # 字节道（WSTRB）掩码工具（支持 8/16/32/64 bit 总线）
└── peripherals/
    ├── axi_gpio.h           # GPIO: data/dir/interrupt, 4 registers
    ├── axi_uart.h           # UART: TX/RX data, status, ctrl
    ├── axi_timer.h          # Timer: load/count/ctrl, auto-reload
    ├── axi_pwm.h            # 4-channel PWM, duty cycle, wrap IRQ
    ├── axi_i2c.h            # I2C master, 9-state FSM, prescaler
    ├── axi_spi.h            # SPI master (Mode 0), baud rate, overrun detect
    └── axi_dma.h            # Memory-to-memory DMA, burst, status/IRQ
```

## KEY FILES
| Task | File |
|------|------|
| AXI4-Lite slave（4 寄存器状态机） | `axi4_lite_slave.h` |
| 5-channel AXI4 slave | `axi4_full.h` |
| 2x2 crossbar（AXI4-Lite） | `axi4_lite_matrix.h` |
| 4x4 crossbar (full) | `axi_interconnect_4x4.h` |
| 互连层共用契约类型（地址窗口 / 默认映射 / 合法性校验） | `interconnect_common.h` |
| AXI 响应码（唯一权威） | `axi_resp.h` |
| Protocol conversion | `axi_to_axilite.h` |
| 字节道（WSTRB）掩码 | `byte_lane.h` — `byte_lane_mask<DATA_WIDTH>` / `byte_lane_apply<W>` |
| Peripherals | `peripherals/axi_gpio.h`, `axi_uart.h`, `axi_timer.h`, `axi_pwm.h`, `axi_i2c.h`, `axi_spi.h`, `axi_dma.h` |

## CONVENTIONS
- **Namespace**: `axi4`
- **Handshake pattern**: All peripherals use `busy/aw_done/b_valid/r_valid` register quartet for AW/W and AR/R phase separation
  - `aw_done`: set on AW handshake, cleared when the B handshake completes (`b_valid && bready`)
  - `b_valid`: set when WDATA is accepted → `bvalid` rises **after** the write data and **holds until BREADY**
  - `r_valid`: set on AR handshake, cleared on RREADY → `rvalid` holds until RREADY
  - `busy`: gates new AW/AR; cleared when a transaction actually completes
  - ⚠️ `aw_done` MUST NOT be cleared on raw `bready` — a master that holds BREADY high
    would clear `aw_done` before WDATA arrives and the write would never be accepted
- **地址锁存**: AXI 只要求 `AWADDR`/`ARADDR` 保持到对应握手，握手后 master 即可改写它们。
  W 可能延后多个周期才到达，因此 peripheral MUST 在 AW/AR 握手时把地址锁存进寄存器，
  写侧/读侧地址解码 MUST 基于**锁存**的地址 —— MUST NOT 基于活的 `io().awaddr` / `io().araddr`。
  （否则 master 在 AW 握手后改写地址会让写落到错误寄存器 —— 静默数据损坏）
- **复位**: AXI4-Lite slave peripherals MUST expose `ch_in<ch_bool> rst_n`
  (低有效，**在时钟沿生效** —— `select(!io().rst_n, <init_val>, ...)` 落在 `ch_reg` 的
  `next` 表达式上，由时钟沿采样；**不是**异步复位),
  and every `ch_reg` MUST use the `select(!io().rst_n, <init_val>, ...)` reset pattern
  (`<init_val>` MUST carry the register's explicit width, e.g. `ch_uint<32>(0_d)`)
  - ⚠️ 未驱动的 `ch_in<ch_bool>` 读出为 **0** —— 即"永久复位"。
    新增 `rst_n` 端口时 MUST 检查所有实例化点都已接线。
    这是**零编译错误的静默破坏**：未接线的实例化点编译通过、运行时不报错，
    但寄存器永远停在初值（写不进、读全 0）。
  - 🛡️ **自动化护栏（2026-10-21 起）**：
    - 静态：`scripts/verify_rst_n_wiring.sh`（已挂进 CI 的 2 个 job）。
      **per-test-case** 粒度：「**仿真**了带必需 `rst_n` 端口的组件的用例 MUST 驱动它」
      +「wrapper MUST 透传 `rst_n <<=`」。
      只认**真实驱动**（`set_input_value(...rst_n...)` / `rst_n <<=`）——
      **注释里提到 `rst_n` 不算数**（否则护栏会被注释骗过）。
      组件清单在脚本头部 `COMPONENTS_W_RST_N`，新增带 `rst_n` 的组件时 MUST 同步维护。
    - 运行时：`tests/test_axi_rst_n_guard.cpp`（双后端）——
      故意不驱动 `rst_n` → 断言无 B/R 响应 + 寄存器保持初值。
      ⚠️ 该文件因此需要**豁免**静态护栏的「MUST 驱动 `rst_n`」检查：
      文件里 MUST 带标记行 `rst_n_guard: intentionally-unwired`。
      豁免**仅对文件名含 `rst_n_guard` 的文件生效**（往普通测试文件塞标记**不豁免**），
      且护栏会在 CI 日志里打印 `::notice` 点名被豁免的文件 —— **豁免不是静默的**。
  - 层级传递时**无需额外约束** —— `context::topological_sort_visit`
    （`src/core/context.cpp:273`）沿 `src` 边递归，父的 `ch_out` 排在子组件的
    `ch_in` 之前 → 同一拍内传递生效。`operator<<=` 只做 `set_driver`
    （**不创建节点**），接线语句在 `describe()` 中的位置**不影响**求值顺序。
- **互连层（interconnect / matrix）**：
  契约见 `openspec/specs/axi-interconnect-routing/spec.md`
  （`axi4-interconnect-routing-2026-10` 落地，管辖**互连侧**；
  `axi-slave-conformance` 管**从设备侧**，两者不重叠）。
  ✅ 该主 spec 已由 change `axi4-interconnect-routing-2026-10` 归档时建立
  （`openspec/changes/archive/2026-10-26-axi4-interconnect-routing-2026-10/`）。
  适用对象：`axi_interconnect_4x4.h` 的 `AxiInterconnect4x4`（AXI4 Full，4×4）
  与 `axi4_lite_matrix.h` 的 `AxiLiteMatrix2x2`（AXI4-Lite，2×2）。
  1. **路由锁存**：目标 slave 在 **AW / AR 握手当拍**由**该通道自己的地址**解码
     并锁存；写侧（`wr_route`）与读侧（`rd_route`）**各自独立**。
     - **握手决策**（`s*_awvalid` / `s*_arvalid` 的正向转发、`m*_awready` /
       `m*_arready` 的反查）MUST 用**当拍**地址解码 + **当拍** grant
       （AXI 要求 master 在 `awvalid && !awready` 期间保持地址稳定 ⇒ 握手窗口内的
       「活地址」是安全的）；
     - **握手后的转发**（W / B / R 通道）MUST 用**锁存路由**，
       MUST NOT 用活的 `awaddr` / `araddr` 或活的仲裁 grant；
     - ⚠️ **正向转发 MUST 同时吃「在途门控」**（`!write_active[m]` /
       `!read_active[m]`）—— 该 master 在自己的事务在途期间合法地举起**下一笔**
       `awvalid` / `arvalid`（AXI4 允许多 outstanding）时，本笔请求 MUST NOT
       被转发给 slave（否则 slave 接受了一个**互连从未锁存路由**的请求 ⇒ 幻影
       AW / AR ⇒ slave 等不到 W、master 等不到 `awready` ⇒ 双向永久死锁）。
       该门控与第 2 条的 `m*_awready` 门控是**同一件事的两面**。
  2. **在途门控**：`m*_awready` MUST 被 `!write_active[m]` 门控、
     `m*_arready` MUST 被 `!read_active[m]` 门控 —— 在途期间 MUST 为 0，
     否则 master 认为 AW/AR 被接受而互连悄悄吞掉（**AW 静默丢失**）。
  3. **独立仲裁**：AW 与 AR **各有一套** round-robin（请求取自该通道自己的
     `awvalid` / `arvalid`）；指针指向无请求的 master 时**让位**（修永久饿死）；
     在途事务保持授权；轮转条件 = 该方向**事务真正完成**
     （写 = B 握手；读 = 末拍 R 握手）。
  4. **地址映射可配置**：`AxiAddrWindow{base, size}` + 尾部模板参数 `Map`
     （向后兼容 `AxiInterconnect4x4<32,32,4>` / `AxiLiteMatrix2x2<32,32>`）。
     窗口命中判据 = `(addr & ~(size-1)) == base`（**MUST NOT** 用运行期移位）。
     ⚠️ **默认映射覆盖全部地址空间** ⇒ **默认配置下 DECERR 不可触发**
     （4×4 默认 = 4×1GiB，与旧 `awaddr >> 30` 逐位等价；
       2×2 默认 = 2×2GiB，与旧 `awaddr >> 31` 逐位等价）
     ⇒ 测 DECERR MUST 用**非默认映射**实例化。
  5. **error responder**（未映射地址由互连层自己应答 —— 互连是纯路由，
     没有下游 slave 可应答）：
     - **写**：`awready` 拉高 + `bvalid`（**寄存器**，保持到 `bready`）
       + `bresp = DECERR`（MUST NOT 用组合单拍脉冲 ⇒ 合规 master 永久死锁）；
     - **读**：`arready` 拉高 + 返回 **AXI4 Full `arlen + 1` 个 beat** /
       **AXI4-Lite 恒 1 个 beat**（AXI4-Lite 无 `arlen`），每拍 `rresp = DECERR`，
       末拍 `rlast = 1`（MUST NOT 只回 1 拍 ⇒ master 等不到 `rlast` 会挂死）。
  6. **响应载荷也按在途标志门控**：`m*_bresp` / `m*_bid` / `m*_rdata` /
     `m*_rresp` MUST 在**没有在途事务**时为 0 —— 否则本 master 会镜像某个
     从设备的当前值（可能是**另一个 master 正在用**的那个）⇒ 表面上像
     「响应串到了别人身上」。
  7. **复位**：互连层暴露 `ch_in<ch_bool> rst_n`（低有效，**在时钟沿生效**），
     全部状态寄存器（路由锁存 / `active` / 仲裁指针 / error responder 状态）
     MUST 带 `select(!io().rst_n, <init>, ...)` 复位 pattern。
     ⚠️ 互连层是 rst_n 链路的**中间节点**，MUST 与下游 slave 在**同一复位窗口**
     内复位 —— 否则 slave 的在途标志被清零而互连的路由锁存残留 ⇒ 永久死锁。
     ⚠️ `rst_n` 是**必需端口**：未接线读出为 0 = **永久复位**（零编译错误的
     静默破坏）⇒ 实例化点 MUST 显式接线，已由
     `scripts/verify_rst_n_wiring.sh` 的 `COMPONENTS_W_RST_N` 清单守护
     （新增带 `rst_n` 的组件时 MUST 同步维护该清单）。

- **通用 AXI slave 规范形态**（`axi4_lite_slave.h` 的 `AxiLiteSlave`）：
  与 6 个 peripheral **同源**（同一套 `busy/aw_done/b_valid/r_valid` 四件套），
  契约见 `openspec/specs/axi-slave-conformance/spec.md`。要点：
  1. `rst_n` + 全部 `ch_reg` 的复位 pattern（初值 MUST 带显式位宽）
  2. **AW→W 相位分离**：W 可在 AW 之后的任意周期被接受；
     ⚠️ `aw_done` 是**寄存器**（表达式中读到的是**当前值**），因此
     **同拍 AW+W 时该拍 W 不被接受** —— master MUST 保持 `WVALID` 到 `WREADY`
     （AXI 合规 master 的义务）。这是规范形态的固有性质，**不是缺陷**
  3. 地址锁存（写侧 `addr_reg` / 读侧 `ar_addr_reg`）
  4. 响应保持：`bvalid` 保持到 `BREADY`，`rvalid` 保持到 `RREADY`
  5. **置 1 条件 MUST 优先于清 0 条件**（`b_valid` / `r_valid`）——
     写成清 0 优先会在 `bready` 常态拉高时让 `b_valid` 永远为 0 →
     `bvalid` 永不拉高（master 死等）且 `aw_done` 永不清零（同一笔 W 被反复接受）
  6. `busy` 只在事务**真正完成**（`b_valid && bready` / `r_valid && rready`）时释放
  7. 未映射地址 → `DECERR`(0b11)，MUST NOT 别名到已实现寄存器；
     已映射但 `wstrb == 0` 的空写 → 响应仍为 `OKAY`(0b00)
  8. 解码步长**固定 4 字节**（`addr >> 2`），与 `DATA_WIDTH` 无关；
     窗口 = 字节地址 `0x00..0x0C`
  9. `NUM_REGS` 模板参数**未 honor**（固定 4 个寄存器）→ 用 `static_assert` 在
     编译期暴露，MUST NOT 静默接受。
     ✅ `Axi4Slave`（AXI4 **Full**）的 `NUM_REGS` / `MAX_BURST_LEN` / `DATA_WIDTH`
     三个参数已在 `axi4-full-generic-slave-burst-2026-10` 中 honor 化 / 编译期暴露
     （见下方「AXI4 Full burst 语义」段）
- **AXI4 Full burst 语义**（`axi4_full.h` 的 `Axi4Slave`）：
  契约见 `openspec/specs/axi4-full-slave-burst/spec.md`
  （`axi4-full-generic-slave-burst-2026-10` 落地）。
  1. **规范形态与 Lite 同源**：`rst_n`（低有效，**在时钟沿生效**）+ 全部 36 个
     `ch_reg` 的复位 pattern + AW→W 相位分离 + 写/读**地址锁存** +
     `bvalid` 保持到 `bready` / `rvalid` 保持到 `rready` +
     未映射地址 → `DECERR`
  2. ⚠️ **写 / 读方向各自独立 outstanding**（AXI4 的 AW/W/B 与 AR/R 是独立通道组）：
     本组件**不使用**共享 `busy`（`AxiLiteSlave` 的读写互斥形态）→
     降级为互斥属于行为回退，MUST NOT 发生
  3. `awlen` / `arlen` 是「beat 数 − 1」的 AXI 编码 → 一笔事务 = `len + 1` 个 beat
  4. `INCR`：每 beat 地址前进 `2^awsize` / `2^arsize` 字节
     （**查表**实现 —— MUST NOT 用运行期移位算 `2^awsize`）
  5. `FIXED`：每 beat 地址不变
  6. `WRAP`：**不支持** → 以 `SLVERR` **明确拒绝**（MUST NOT 静默当作 INCR）
  7. 每一拍按**当拍** `wstrb` 重新展开字节道掩码（MUST NOT 在 AW 拍锁存）
  8. `wlast` 与预期 beat 数不符（提前 / 缺失）→ `SLVERR`
     （已写入的 beat **不**回滚 —— 部分写入 + 错误响应是 AXI 允许的行为）
  9. `awlen + 1 > MAX_BURST_LEN` → `SLVERR`（MUST NOT 静默截断并返回 `OKAY`）
  10. INCR burst 越界（某 beat 跨出 0x00..0x3C 窗口）→ 该 beat 被抑制
      （写：寄存器保持原值；读：`rdata = 0` 且 `rresp = DECERR`），事务以 `DECERR` 结束
  11. 读侧 `rvalid` 采用**流式**形态：整个 burst 期间连续为 1，每拍呈现一个 beat，
      直到该 beat 被 `rready` 接受
  12. ⚠️ **窄传输（`awsize < log2(DATA_WIDTH/8)`）的字节道重编号未实现**：
      `DATA_WIDTH == 64` 且 `awsize < 3` 时相邻 beat 落到**同一个** 64 位寄存器
      （解码步长固定 4 字节，寄存器宽度为 `DATA_WIDTH`）
  13. 模板参数：`NUM_REGS` 固定 16（`static_assert` 暴露）、
      `MAX_BURST_LEN` 真正生效（超限 → `SLVERR`）、`DATA_WIDTH` MUST ≤ 64
- **Address decode**: `addr >> 2` (4-byte aligned per register；通用 slave 不用掩码截断，
  截断会把窗口外地址别名到已实现寄存器)
- **Response**: 6 个 peripheral 对已映射地址返回 `bresp=OKAY(0)`；
  通用 slave `AxiLiteSlave` 对**未映射地址**返回 `DECERR`(0b11)；
  ✅ **互连层**（`axi_interconnect_4x4.h` / `axi4_lite_matrix.h`）也有自己的
  DECERR error responder（见下方「互连层（interconnect / matrix）」段）——
  此前它们的 DECERR 分支**不可达**（地址解码硬编码 ⇒ 整个地址空间都被映射），
  已由 `axi4-interconnect-routing-2026-10` 修正为**编译期可配置**的窗口映射。
- **Peripheral registers**: 4–8 registers per device, 4-byte aligned
- **字节道语义（WSTRB）**: AXI slave 的写路径 MUST 消费 `wstrb`，
  MUST NOT 整字覆盖寄存器。**适用范围**：6 个 AXI4-Lite peripheral
  （`peripherals/axi_*.h`）与通用 slave（`axi4_lite_slave.h` 的 `AxiLiteSlave`、
  `axi4_full.h` 的 `Axi4Slave`）遵守**同一套**语义。
  实现（`include/axi4/byte_lane.h`）：
  1. `WSTRB` 位 i ↔ 字节道 i（bit0 → byte0 …），每道 8 bit；
     `WSTRB` 宽度 = `DATA_WIDTH / 8`（一条总线有 `DATA_WIDTH / 8` 条字节道）
  2. 写路径：`new = (old & ~mask) | (wdata & mask)`，`mask` 由 `WSTRB` 逐字节展开
  3. `mask` MUST 与目标寄存器的**有效字节道**相交：宽度 W 的寄存器有效道数为
     `ceil(W / 8)`（W=8 → 仅 byte0；W=16 → byte0..1；W=32 → byte0..3）；
     落在无效道上的写被忽略（寄存器值不变，不报错）
  4. 结果 MUST 按寄存器宽度 W 截断
  5. `WSTRB == 0` → 无字节被选中 → 寄存器保持原值（地址已解码，响应仍 `OKAY`）
  6. 只读寄存器 MUST NOT 进入写路径（写其地址 → `DECERR`，值不变）
  7. **字节道索引相对被访问的*字***（`DATA_WIDTH` 宽的 word），
     MUST NOT 相对寄存器重新编号 —— 寄存器地址先解码到字，再按 `WSTRB` 数道
  8. **亚 32 位寄存器位于所在字的首字节道**（byte0 起）—— 因此其有效道就是
     低 `ceil(W / 8)` 条（当前全部寄存器均如此）
  9. `WSTRB` **逐 W 握手有效**：`Axi4Slave` MUST 在每次 W 握手拍按当拍
     `io().wstrb` 重新展开掩码，MUST NOT 在 AW 拍锁存
     （✅ 多 beat burst 的逐 beat 语义已在 `axi4-full-generic-slave-burst-2026-10`
     中落地，见上方「AXI4 Full burst 语义」段）
  - 实现入口：`axi4::detail::byte_lane_mask<DATA_WIDTH>(wstrb)` 展开一次，
    `axi4::detail::byte_lane_apply<W>(old, wdata, mask)` 逐寄存器套用
    （`BUS_WIDTH` 默认 `max(W, 32)`，非推导上下文；32 bit 调用点写 `byte_lane_mask<32>`）
  - ⚠️ master 侧 MUST 驱动 `WSTRB`（AXI 要求）—— 未驱动的 `wstrb` 读出为 0，
    此时写为"无字节被选中"的空写，寄存器值不变
- **UART 串行语义（`AxiLiteUart<UART_BIT_PERIOD>`）**:
  - 帧格式 8N1：1 起始位(0) + 8 数据位(LSB 先出) + 1 停止位(1)
  - **bit 周期 = `UART_BIT_PERIOD` 个时钟周期**（编译期常量；波特率 = 时钟频率 / bit 周期）。
    该参数此前名为 `PRESCALE_WIDTH` 且从未被使用，现改为 `UART_BIT_PERIOD` 并真正生效
  - `uart_tx` 空闲为**高**；`uart_rx` 空闲为**高**（低电平被视作起始位）——
    测试台 MUST 把 `uart_rx` 置 1，否则 RX 引擎会持续把低电平当起始位
  - `STATUS`（0x08）位定义：bit0 = `TX_EMPTY`、bit1 = `RX_VALID`、
    bit2 = `RX_FRAME_ERROR`（停止位错误，**仅 `rst_n` 清除**）
  - `CTRL`（0x0C）当前**保留**（不参与 TX/RX 门控）
  - ⚠️ 组合逻辑的"结束/完成"判定 MUST 用**下一个值**（如 `next_phase == 10`、
    `next_bits == 8`）而非当前值：组合求值是**单遍**、不迭代到不动点，
    而寄存器把组合 DAG 切成了时序边 —— 用**当前值**判定会让状态机
    早/晚一拍（实测：RX 引擎只采到 7 位）
    - 📌 **求值顺序事实**（2026-10-21 核实）：组合指令列表来自
      `context::get_eval_list()` 的**拓扑排序**
      （`src/core/context.cpp:273` `topological_sort_visit` 沿 `src` 边递归），
      **数据依赖是被尊重的**；纯组合环在 `Simulator::initialize()` 触发
      `CHFATAL`（响亮失败），寄存器则天然打断组合环。
      → 「单遍、**无拓扑序**」是**过时且不准确**的表述，勿再沿用。

## ANTI-PATTERNS
- **互连层用活的地址做握手后的转发**: Do NOT decode the route from the live
  `io().awaddr` / `io().araddr` for W / B / R channels — MUST use the **latched**
  route. Verify: `grep -cE "wr_route|rd_route" include/axi4/axi_interconnect_4x4.h`
  (MUST ≥ 4) + `grep -cE "route_s1|rd_route" include/axi4/axi4_lite_matrix.h`
  (MUST ≥ 2).
- **互连层用写通道的 grant 反查读通道**: Do NOT drive `s*_rready` from the
  **write**-side grant — the read channel MUST use its own **latched read route**.
- **IO port `&&`**: Do NOT use `&&` on IO ports (e.g., `io().awvalid && (!busy)`). Use `ch::select()` instead.
  Verify with: `grep -nE 'io\(\)\.[a-z_]+ *(&&|\|\|)' include/axi4/peripherals/*.h` (MUST hit 0 lines)
- **Peripheral without busy flag**: Every peripheral must have a `busy` register that gates new requests.
- **Unmapped address DECERR**: Any address not decoded should return `DECERR`, not OKAY.
- **整字覆盖写（忽略 `wstrb`）**: Do NOT write `reg->next = select(we, io().wdata, reg)` ——
  写路径 MUST 走 `byte_lane_apply<W>`。Verify:
  `grep -c 'byte_lane_apply' include/axi4/peripherals/axi_*.h`（每个 peripheral MUST ≥ 1；
  `axi_dma.h` 是 AXI4 **master**，无寄存器写路径，不适用）+
  `grep -c 'byte_lane_apply' include/axi4/axi4_lite_slave.h include/axi4/axi4_full.h`
  （通用 slave MUST ≥ 1）
- **用寄存器当前值判定"结束"**: Do NOT 用 `phase == 9` / `bits == 7` 这类**当前值**
  比较来判定状态机结束 —— 寄存器把组合 DAG 切成了时序边，用当前值判定会让
  状态机早/晚一拍（实测：RX 引擎只采到 7 位）。
  MUST 用**下一个值**判定（`next_phase == 10` / `next_bits == 8`）
- **Combinational BVALID/RVALID**: Do NOT drive `bvalid`/`rvalid` from a combinational handshake
  (`bvalid = w_handshake`). The response MUST hold until the master raises BREADY/RREADY,
  otherwise a compliant master waits forever and the peripheral deadlocks.
- **Unlatched address with phase separation**: Do NOT decode the register offset from the live
  `io().awaddr` / `io().araddr` once W/R may arrive in a later cycle. Latch the address at the
  AW/AR handshake. Verify: `grep -nE 'addr_bits = io\(\)\.aw?r?addr' include/axi4/peripherals/*.h`
  (MUST hit 0 lines)
- **Master 用从设备的 ready gate 自己的 valid**: Do NOT write
  `io().wvalid = select(write_start, io().awready, false)`（或用 `io().bvalid` 推 `bready`）。
  AXI 禁止；且 CppHDL 的组合求值是**单遍**、不迭代到不动点
  （`src/simulator.cpp:257` `eval_combinational()` 单遍遍历
  `combinational_instr_list_`），跨模块组合路径的**求值顺序由拓扑排序保证**
  （`src/core/context.cpp:273`），但**单遍**意味着"消费方读到的是本拍开头
  求值出的值"，跨模块组合环会被 `initialize()` 的 `CHFATAL` 直接打断。
  Master 的 `wvalid` / `bready` MUST 由**自身状态**驱动（状态保持到 `write_done`），
  且 `awaddr` / `wdata` MUST 在整个事务期间保持稳定。

## EXAMPLES
- `examples/axi4/axi4_lite_example.cpp` — Lite slave timing, VCD recording, assertion checks
- `examples/axi4/axi4_full_example.cpp` — Full 5-channel burst slave
- `examples/axi4/axi_soc_example.cpp` — Matrix 2x2 + GPIO + UART + Timer SoC
- `examples/axi4/phase3a_complete.cpp` — 4x4 interconnect + converter + slave

## TESTS
| File | Tag | Focus |
|------|-----|-------|
| `test_axi_lite_bundle.cpp` | `[bundle][axi]` | Bundle integration |
| `test_axi_dma.cpp` | `[axi][dma]` | DMA controller |
| `test_axi_gpio.cpp` | `[axi][gpio]` | GPIO 通道/复位语义（含 AW→W 相位分离、BVALID 保持） |
| `test_axi_byte_lane.cpp` | `[axi][byte-lane][dual]` | 字节道（WSTRB）写语义（6 个 peripheral 的部分字节写 / 只读寄存器；**双后端**） |
| `test_axi_timer.cpp` | `[axi][timer]` | Timer 通道/复位语义 |
| `test_axi_uart.cpp` | `[axi][uart]` | UART 通道/复位语义 |
| `test_axi_pwm_simple.cpp` | `[axi][pwm]` | PWM basic |
| `test_axi_pwm_timed.cpp` | `[axi][pwm]` | PWM with timing |
| `test_axi_spi.cpp` | `[axi][spi]` | SPI master |
| `test_axi4_full_burst.cpp` | `[axi][axi4][burst][dual]` | 通用 AXI4 Full slave 的多 beat burst 语义（INCR / FIXED / WRAP→SLVERR / 逐 beat wstrb / 越界 DECERR / 超 MAX_BURST_LEN→SLVERR / rst_n 复位；**双后端**） |
| `test_axi_i2c.cpp` | `[axi][i2c]` | I2C master |
| `test_axi_interconnect_4x4.cpp` | `[axi][interconnect][dual]` | 4×4 互连的 16 对全路由 / 路由锁存 / 独立仲裁（恰一个 grant）/ 在途门控 + **幻影 AW·AR** / 多 beat mapped 写读 / **多 beat unmapped 写（逐 beat 收完才应答）** / DECERR 与 **事务 ID 回显** / rst_n 中途复位 + error responder 复位（**双后端**，23 对 = 46 TEST_CASE / 474 断言） |
| `test_axi_lite_matrix_2x2.cpp` | `[axi][matrix][dual]` | 2×2 矩阵的 4 对全路由 / 读侧 `ar_route` 锁存 / `s*_rready` 用读路由 / 写读独立仲裁 / 在途门控 + **幻影 AW·AR** / DECERR / rst_n + error responder 复位（**双后端**，17 对 = 34 TEST_CASE / 170 断言） |

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**.
New peripheral → `axi4/peripherals/` + `tests/test_axi_*.cpp` + include in `examples/axi4/`.