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
- **Handshake pattern**: All peripherals use `busy/aw_done/r_valid` register trio for AW/W and AR/R phase separation
- **Address decode**: `addr >> 2 & mask` (4-byte aligned per register)
- **Response**: All peripherals return `bresp=OKAY(0)`. `axi_interconnect_4x4.h` returns `DECERR` for unmapped addresses.
- **Peripheral registers**: 4–8 registers per device, 4-byte aligned

## ANTI-PATTERNS
- **IO port `&&`**: Do NOT use `&&` on IO ports (e.g., `io().awvalid && (!busy)`). Use `ch::select()` instead. Found at `axi_timer.h:77-81`.
- **Peripheral without busy flag**: Every peripheral must have a `busy` register that gates new requests.
- **Unmapped address DECERR**: Any address not decoded should return `DECERR`, not OKAY.

## EXAMPLES
- `examples/axi4/axi4_lite_example.cpp` — Lite slave timing, VCD recording, assertion checks
- `examples/axi4/axi4_full_example.cpp` — Full 5-channel burst slave
- `examples/axi4/axi_soc_example.cpp` — Matrix 2x2 + GPIO + UART + Timer SoC
- `examples/axi4/phase3a_complete.cpp` — 4x4 interconnect + converter + slave

## TESTS
| File | Tag | Focus |
|------|-----|-------|
| `test_axi4lite.cpp` | `[axi][lite]` | Lite protocol |
| `test_axi_lite_bundle.cpp` | `[bundle][axi]` | Bundle integration |
| `test_axi_dma.cpp` | `[axi][dma]` | DMA controller |
| `test_axi_pwm_simple.cpp` | `[axi][pwm]` | PWM basic |
| `test_axi_pwm_timed.cpp` | `[axi][pwm]` | PWM with timing |
| `test_axi_spi.cpp` | `[axi][spi]` | SPI master |
| `test_axi_i2c.cpp` | `[axi][i2c]` | I2C master |

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**.
New peripheral → `axi4/peripherals/` + `tests/test_axi_*.cpp` + include in `examples/axi4/`.