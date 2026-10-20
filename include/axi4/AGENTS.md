# AGENTS.md - AXI4 Bus & Peripherals

Child of `include/AGENTS.md` → root `AGENTS.md` (ZERO-DEBT POLICY + PHASE GATES).

## OVERVIEW
AXI4 bus protocol (Full/Lite) + 7 peripherals. All header-only, `ch::Component` subclass hierarchy.

## STRUCTURE
```
include/axi4/
├── axi4_full.h              # AXI4 Full slave (5 channels, burst)
├── axi4_lite_slave.h        # Simplified Lite slave (4-register state machine)
├── axi4_lite_matrix.h       # 2x2 crossbar, round-robin, address decode
├── axi_interconnect_4x4.h   # 4x4 full AXI crossbar (635 lines — most complex)
├── axi_to_axilite.h        # AXI ↔ AXI-Lite bidirectional converter
├── byte_lane.h             # AXI4-Lite 字节道（WSTRB）掩码工具
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
| 2x2 crossbar | `axi4_lite_matrix.h` |
| 4x4 crossbar (full) | `axi_interconnect_4x4.h` |
| Protocol conversion | `axi_to_axilite.h` |
| 字节道（WSTRB）掩码 | `byte_lane.h` — `byte_lane_mask` / `byte_lane_apply<W>` |
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
- **复位**: AXI4-Lite slave peripherals MUST expose `ch_in<ch_bool> rst_n` (低有效异步复位),
  and every `ch_reg` MUST use the `select(!io().rst_n, <init_val>, ...)` reset pattern
  (`<init_val>` MUST carry the register's explicit width, e.g. `ch_uint<32>(0_d)`)
  - ⚠️ 未驱动的 `ch_in<ch_bool>` 读出为 **0** —— 即"永久复位"。
    新增 `rst_n` 端口时 MUST 检查所有实例化点都已接线。
- **Address decode**: `addr >> 2 & mask` (4-byte aligned per register)
- **Response**: All peripherals return `bresp=OKAY(0)`. `axi_interconnect_4x4.h` returns `DECERR` for unmapped addresses.
- **Peripheral registers**: 4–8 registers per device, 4-byte aligned
- **字节道语义（WSTRB）**: AXI4-Lite slave peripheral 的写路径 MUST 消费 `wstrb`，
  MUST NOT 整字覆盖寄存器。语义（`include/axi4/byte_lane.h` 实现）：
  1. `WSTRB` 位 i ↔ 字节道 i（bit0 → byte0 … bit3 → byte3），每道 8 bit
  2. 写路径：`new = (old & ~mask) | (wdata & mask)`，`mask` 由 `WSTRB` 逐字节展开
  3. `mask` MUST 与目标寄存器的**有效字节道**相交：宽度 W 的寄存器有效道数为
     `ceil(W / 8)`（W=8 → 仅 byte0；W=16 → byte0..1；W=32 → byte0..3）；
     落在无效道上的写被忽略（寄存器值不变，不报错）
  4. 结果 MUST 按寄存器宽度 W 截断
  5. `WSTRB == 0` → 无字节被选中 → 寄存器保持原值（地址已解码，响应仍 `OKAY`）
  6. 只读寄存器 MUST NOT 进入写路径（写其地址 → `DECERR`，值不变）
  - 实现入口：`axi4::detail::byte_lane_mask(wstrb)` 展开一次，
    `axi4::detail::byte_lane_apply<W>(old, wdata, mask)` 逐寄存器套用
  - ⚠️ master 侧 MUST 驱动 `WSTRB`（AXI4-Lite 要求）—— 未驱动的 `wstrb` 读出为 0，
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
    `next_bits == 8`）而非当前值：本框架的组合求值是**单遍、无拓扑序**
    （`src/simulator.cpp::eval_combinational`），寄存器比较的求值时机受节点
    创建顺序影响，用当前值判定会让状态机早/晚一拍 → 丢失最后一个数据位

## ANTI-PATTERNS
- **IO port `&&`**: Do NOT use `&&` on IO ports (e.g., `io().awvalid && (!busy)`). Use `ch::select()` instead.
  Verify with: `grep -nE 'io\(\)\.[a-z_]+ *(&&|\|\|)' include/axi4/peripherals/*.h` (MUST hit 0 lines)
- **Peripheral without busy flag**: Every peripheral must have a `busy` register that gates new requests.
- **Unmapped address DECERR**: Any address not decoded should return `DECERR`, not OKAY.
- **整字覆盖写（忽略 `wstrb`）**: Do NOT write `reg->next = select(we, io().wdata, reg)` ——
  写路径 MUST 走 `byte_lane_apply<W>`。Verify:
  `grep -c 'byte_lane_apply' include/axi4/peripherals/axi_*.h`（每个 peripheral MUST ≥ 1）
- **用寄存器当前值判定"结束"**: Do NOT 用 `phase == 9` / `bits == 7` 这类**当前值**
  比较来判定状态机结束 —— 组合求值单遍无拓扑序，比较的求值时机受节点创建顺序
  影响，会让状态机早/晚一拍（实测：RX 引擎只采到 7 位）。
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
  AXI 禁止；且 CppHDL 的组合求值是**单遍、无拓扑序**（`src/simulator.cpp:257`
  `eval_combinational()` 单遍遍历 `combinational_instr_list_`，不迭代到不动点），
  跨模块组合路径中"消费方节点先于生产方节点创建"时就会读到**上一拍**的陈旧值。
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
| `test_axi_i2c.cpp` | `[axi][i2c]` | I2C master |

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**.
New peripheral → `axi4/peripherals/` + `tests/test_axi_*.cpp` + include in `examples/axi4/`.