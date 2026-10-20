# Change: axi4-slave-byte-lane-2026-10 — 通用 AXI slave 的字节道（WSTRB）写语义

## Why

前序 change `axi4-peripheral-deferred-2026-10`（archive: `2026-10-20-axi4-peripheral-deferred-2026-10`）
把 `include/axi4/peripherals/*.h` 的 **6 个 AXI4-Lite slave peripheral** 的写路径改成了消费
`wstrb` 字节掩码（`new = (old & ~mask) | (wdata & mask)`）。

但仓库里还有 **2 个同样暴露 `wstrb` 端口、却整字覆盖写**的 AXI slave 实现：

| 文件 | 类 | 寄存器 | 现状 |
|---|---|---|---|
| `include/axi4/axi4_lite_slave.h` | `AxiLiteSlave<ADDR_WIDTH, DATA_WIDTH, NUM_REGS>` | 4 个 `DATA_WIDTH` | `:84-87` 整字写；`wstrb` 端口在 `:24`（宽度 `DATA_WIDTH/8`） |
| `include/axi4/axi4_full.h` | `Axi4Slave<ADDR_WIDTH, DATA_WIDTH, ID_WIDTH, NUM_REGS, MAX_BURST_LEN>` | 16 个 `DATA_WIDTH` | `:228-243` 整字写；`wstrb` 端口在 `:71`（宽度 `NUM_BYTES = DATA_WIDTH/8`，`:55`） |

后果是**同一仓库里存在两套写语义**：

- master 按 AXI 协议只驱动部分字节道（`wstrb != 0xF`）→ 在这 2 个 slave 上**被静默忽略**，
  整字被写入 → 静默数据损坏（与 peripheral 侧已修好的行为相反）；
- 这 2 个 slave 是**通用**实现（被 `examples/axi4/axi4_lite_example.cpp`、
  `examples/axi4/phase3a_complete.cpp`、`tests/test_axi_lite_verilator_e2e.cpp`、
  `tests/test_verilator_memory_e2e.cpp`、`tests/test_verilator_trace_parity.cpp` 实例化），
  后续把它们接进 SoC 时必然踩坑。

顺带补两处此前**未写明**的字节道语义（见 D4）——它们是判断"某个实现是否正确"的判据，
缺了就无法 review：

1. 字节道索引相对**被访问的字**（word），不是相对寄存器；
2. 亚 32 位寄存器位于所在字的**首字节道**（当前所有寄存器均如此）。

> **基线更新（2026-10-20）**：`axi4-byte-lane-dual-parity-2026-10`
> （archive `2026-10-20-axi4-byte-lane-dual-parity-2026-10`）已把字节道契约升级为
> **双后端**契约 —— `tests/test_axi_byte_lane.cpp` 现为 7 个
> `DUAL_BACKEND_TEST_PAIR`（interpreter + verilator）。本 change 新增的通用 slave
> 用例 MUST 沿用同一模式（见 tasks Task 5），否则 codegen 侧的宽度推导偏差仍无覆盖。

## What Changes

### D1: `include/axi4/byte_lane.h` 泛化到任意总线宽度

现实现的 `byte_lane_mask()` 固定 4-bit `WSTRB` / 32-bit 掩码，而 2 个通用 slave 的
`wstrb` 宽度是 `DATA_WIDTH/8`（`DATA_WIDTH=64` → 8 条字节道）。

- `byte_lane_mask<DATA_WIDTH>(wstrb)`：按总线宽度模板化，展开 `DATA_WIDTH/8` 条字节道
- `byte_lane_valid_mask<W>()` / `byte_lane_apply<W>()`：语义不变
- **32 bit 路径行为 MUST NOT 变化**（6 个 peripheral 已依赖它）

### D2: `AxiLiteSlave`（`include/axi4/axi4_lite_slave.h`）写路径消费 `wstrb`

4 个寄存器的写路径改用 `byte_lane_apply<DATA_WIDTH>(reg, io().wdata, wstrb_mask)`。

### D3: `Axi4Slave`（`include/axi4/axi4_full.h`）写路径消费 `wstrb`

16 个寄存器的写路径改用 `byte_lane_apply<DATA_WIDTH>(reg, io().wdata, wstrb_mask)`。

- **每次 W 握手 MUST 用当拍 `wstrb`**（AXI4 语义：`WSTRB` 逐 beat 有效）；
  掩码在 W 握手拍重新展开（`wstrb` 是组合输入），MUST NOT 在 AW 拍锁存
- ⚠️ **实测更正（2026-10-20）**：`Axi4Slave` 的写状态机为
  `wready = (write_state == 1) && wvalid`，且 `!wlast` 后进入的状态 2 不再拉
  `wready` —— **每笔事务只接受 1 个 W beat**，`awlen > 0` 的多 beat burst
  后续 beat 被静默丢弃。多 beat burst 属本 change 非目标（单独立项），
  故 D3 的可验证契约收窄为"每次 W 握手取当拍 `wstrb`"

### D4: 文档补两条字节道语义 + 登记通用 slave 契约

- `include/axi4/AGENTS.md` CONVENTIONS → 字节道语义：补
  "字节道索引相对被访问的**字**" 与 "亚 32 位寄存器位于所在字的首字节道"
- 同一节登记：通用 slave（`AxiLiteSlave` / `Axi4Slave`）与 6 个 peripheral 遵守**同一套**字节道语义

## 影响面与风险

- **既有实例化点全部已驱动 `wstrb=0xF`**（`wstrb=0xF` 与整字写等价 → 行为不变）：
  `examples/axi4/axi4_lite_example.cpp:204`、`examples/axi4/phase3a_complete.cpp:161`、
  `tests/test_axi_lite_verilator_e2e.cpp:229`、`tests/test_verilator_memory_e2e.cpp:386`、
  `tests/test_verilator_trace_parity.cpp:376`
- `examples/axi4/axi4_full_example.cpp` 纯协议演示（不驱动 AXI 信号、无断言）→ 不受影响
- 2 个 slave 均**无 `rst_n` 端口** → 改写路径不触碰复位契约
- `Axi4Slave` 的 burst 当前**所有 beat 写同一个寄存器**（`addr_index` 取自活的 `io().awaddr`，
  `:192`，地址不递增），且写状态机每笔事务只接受 1 个 W beat ——
  本 change 不改这两点（见非目标），但**每次 W 握手取当拍 `wstrb` 的语义仍然要正确**

## 非目标（Out of Scope）

- ❌ 修 `AxiLiteSlave` 的其它规范形态偏差：**无 `rst_n`**、`bvalid` 是组合单拍（不保持到
  `BREADY`）、**地址不锁存**（用活的 `io().awaddr`）—— 与 peripheral 的规范形态差距很大，
  单独立项
- ❌ 修 `Axi4Slave` 的 burst 语义（beat 地址不递增 → 所有 beat 写同一寄存器）—— 单独立项
- ❌ `AxiDma`（AXI4 **master**，不做寄存器写路径）
- ❌ 6 个 `include/axi4/peripherals/*.h`（已由 archive 的
  `2026-10-20-axi4-peripheral-deferred-2026-10` 覆盖）
- ❌ capability 改名：AXI4 peripheral 的行为契约目前都塞在 `dual-backend-test-coverage` 这个
  名不副实的 capability 里（`AXI4 peripheral MUST 支持字节写` / `MUST 提供串行收发引擎` /
  `MUST 驱动响应错误信号` …）—— 建议后续单独立项做 capability 拆分，本次沿用同一 capability
  以保持 delta 最小、避免 spec 内容在搬迁中丢失

## Capabilities

### MODIFIED Requirements

见 `specs/dual-backend-test-coverage/spec.md` —— 把既有的
`AXI4 peripheral MUST 支持字节写` 的适用对象从 `include/axi4/peripherals/*.h` **扩到**
通用 AXI slave（`include/axi4/axi4_lite_slave.h` / `include/axi4/axi4_full.h`），
并补齐 burst per-beat 与两条索引语义。
