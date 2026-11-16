# AGENTS.md - AXI4 Bus Examples

Child of root `AGENTS.md`.

## OVERVIEW
4 AXI4 examples at increasing integration levels: Lite timing → Full burst → SoC → Interconnect.

## FILES
| File | Focus | CTest |
|------|-------|-------|
| `axi4_lite_example.cpp` | AXI4-Lite slave timing, VCD + assertions | ✅ |
| `axi4_full_example.cpp` | AXI4 Full 5-channel burst slave | ✅ |
| `axi_soc_example.cpp` | SoC: Matrix 2x2 + GPIO + UART（4 个 IP） | ✅ |
| `phase3a_complete.cpp` | Phase 3A: 4x4 Interconnect + AXI-to-Lite Converter | ✅ |
| `emit_rtl_soc.cpp` | SoC 层 RTL 发射入口（层次化产物 · 5 module + `--check-products`） | — |
| `tb_axi_soc_example.sv` | SoC 层独立 TB（14 顶层端口 + 4 场景 · `checks=14 fails=0`） | — |

## PATTERNS
- All use `Component` subclass → `Top` wrapper → `ch_device<T>` → `Simulator` in `main()`
- `ch_module<T>` instantiation inside `Component::describe()` (see root AGENTS.md)
- Peripheral headers: `include/axi4/peripherals/axi_gpio.h`, `axi_uart.h`

## RUN
```bash
ctest -L spinalhdl-ported -R "axi4"
./run_all_ported_tests.sh  # includes all 4
```