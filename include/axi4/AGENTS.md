# AGENTS.md - AXI4 Bus & Peripherals

Child of `include/AGENTS.md` → root `AGENTS.md` (ZERO-DEBT POLICY + PHASE GATES).

## OVERVIEW
AXI4 bus protocol (Full/Lite) + 7 peripherals. All header-only, `ch::Component` subclass hierarchy.

## STRUCTURE
```
include/axi4/
├── axi4_lite.h              # AXI4-Lite struct defs (AW/W/B/AR/R channels)
├── axi4_full.h              # AXI4 Full slave (5 channels, burst)
├── axi4_lite_slave.h        # Simplified Lite slave (4-register state machine)
├── axi4_lite_matrix.h       # 2x2 crossbar, round-robin, address decode
├── axi_interconnect_4x4.h   # 4x4 full AXI crossbar (635 lines — most complex)
├── axi_to_axilite.h        # AXI ↔ AXI-Lite bidirectional converter
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
| AXI4-Lite signals | `axi4_lite.h` — channel structs + AxiResp/AxiProt enums |
| 5-channel AXI4 slave | `axi4_full.h` |
| 2x2 crossbar | `axi4_lite_matrix.h` |
| 4x4 crossbar (full) | `axi_interconnect_4x4.h` |
| Protocol conversion | `axi_to_axilite.h` |
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

## ANTI-PATTERNS
- **IO port `&&`**: Do NOT use `&&` on IO ports (e.g., `io().awvalid && (!busy)`). Use `ch::select()` instead.
  Verify with: `grep -nE 'io\(\)\.[a-z_]+ *(&&|\|\|)' include/axi4/peripherals/*.h` (MUST hit 0 lines)
- **Peripheral without busy flag**: Every peripheral must have a `busy` register that gates new requests.
- **Unmapped address DECERR**: Any address not decoded should return `DECERR`, not OKAY.
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
| `test_axi_timer.cpp` | `[axi][timer]` | Timer 通道/复位语义 |
| `test_axi_uart.cpp` | `[axi][uart]` | UART 通道/复位语义 |
| `test_axi_pwm_simple.cpp` | `[axi][pwm]` | PWM basic |
| `test_axi_pwm_timed.cpp` | `[axi][pwm]` | PWM with timing |
| `test_axi_spi.cpp` | `[axi][spi]` | SPI master |
| `test_axi_i2c.cpp` | `[axi][i2c]` | I2C master |

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**.
New peripheral → `axi4/peripherals/` + `tests/test_axi_*.cpp` + include in `examples/axi4/`.