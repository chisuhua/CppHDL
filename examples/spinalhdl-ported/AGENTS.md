# AGENTS.md - SpinalHDL Ported Examples

Child of root `AGENTS.md`. Read `examples/spinalhdl-ported/README.md` for full port status.

## OVERVIEW
17 SpinalHDL examples ported to CppHDL, each with standalone main() self-test + Verilog generation.

## STRUCTURE
18 subdirectories, each with 1-2 .cpp files:

| Module | Key Files | Status |
|--------|-----------|--------|
| counter/ | counter_simple.cpp, counter.cpp | ✅ 2 counters |
| uart/ | uart_tx.cpp | ✅ State machine |
| uart_rx/ | uart_rx.cpp | ✅ RX state machine |
| fifo/ | stream_fifo_example.cpp | ✅ With backpressure |
| width_adapter/ | stream_width_adapter_example.cpp | ✅ CTest registered |
| pwm/ | pwm.cpp | ✅ Duty cycle |
| gpio/ | gpio.cpp | ✅ Edge detect |
| spi/ | spi_controller.cpp | ✅ Master, state machine |
| i2c/ | i2c_controller.cpp | ⚠️ Simplified (no ACK) |
| timer/ | timer.cpp | ✅ Prescaler + reload |
| vga/ | vga_controller.cpp | ✅ 640x480@60Hz |
| ws2812/ | ws2812_controller.cpp | ✅ LED driver |
| quadrature_encoder/ | quadrature_encoder.cpp | ✅ AB phase 4x |
| sigma_delta_dac/ | sigma_delta_dac.cpp | ✅ 1st-order |
| assert/ | assert_example.cpp | ✅ CTest registered |
| phase1c/ | phase1c_test.cpp | ✅ CTest registered |
| stream/ | MOVED → examples/stream/ | See sibling |

## CONVENTIONS
- Each example: `Component` subclass → `Top` wrapper → `main()` self-test
- Build to `build/examples/spinalhdl-ported/<module>/<binary>`
- Test via `run_all_ported_tests.sh` (not CTest directly — uses timeout wrapper)

## testbench_template.cpp
Template for creating new ported examples. Copy and modify.

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**. New examples must:
- Register in `CMakeLists.txt` immediately (no orphan binaries)
- Include in `run_all_ported_tests.sh` (no unverified examples)
- Have working `main()` self-test (not empty stubs)
