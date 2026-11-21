# AGENTS.md - SpinalHDL Ported Examples

Child of root `AGENTS.md`. Read `examples/spinalhdl-ported/README.md` for full port status.

## OVERVIEW
17 SpinalHDL examples ported to CppHDL.

> ⚠️ **两种形态并存**（勿假设「每个 example 都是手写 `main()` 自测」）：
> ① **已迁（8 个产品面 target）** = 三角色分离 = 设计头 `*_top.h`（无 `main`）+
> 激励驱动 `tb_*.cpp`（**默认模式读 `tb_*.verif` 跑断言** · `--emit-rtl` 发射 RTL/TB）+
> 验证源 `tb_*.verif`（**唯一语义来源**）⇒ ⇒ C++ 侧与生成式 TB **同源**。
> ② **未迁** = 传统手写 `main()` 自测 demo。

## STRUCTURE
18 subdirectories, each with 1-2 .cpp files:

| Module | Key Files | Status |
|--------|-----------|--------|
| counter/ | counter_simple_top.h + tb_counter_simple_example.cpp/.verif · counter_top.h + tb_counter_example.cpp/.verif | ✅ 2 counters（`.verif` 单一真相源） |
| uart/ | uart_top.h + tb_uart_tx_example.cpp/.verif | ✅ State machine（`.verif` 单一真相源） |
| uart_rx/ | uart_rx_top.h + tb_uart_rx_example.cpp/.verif | ✅ RX state machine（`.verif` 单一真相源） |
| fifo/ | fifo_top.h + tb_fifo_stream_example.cpp/.verif | ✅ With backpressure（`.verif` 单一真相源） |
| width_adapter/ | stream_width_adapter_example.cpp | ✅ CTest registered |
| pwm/ | pwm_top.h（设计）+ tb_pwm_example.cpp（验证 / `--emit-rtl` 双模式）+ tb_pwm_example.verif | ✅ Duty cycle（`.verif` 单一真相源） |
| gpio/ | gpio_top.h + tb_gpio_example.cpp/.verif | ✅ Edge detect（`.verif` 单一真相源） |
| spi/ | spi_controller.cpp | ✅ Master, state machine |
| i2c/ | i2c_controller.cpp | ⚠️ Simplified (no ACK) |
| timer/ | timer.cpp | ✅ Prescaler + reload |
| vga/ | vga_controller.cpp | ✅ 640x480@60Hz |
| ws2812/ | ws2812_controller.cpp | ✅ LED driver |
| quadrature_encoder/ | quadrature_encoder_top.h + tb_quadrature_encoder_example.cpp/.verif | ✅ AB phase 4x（`.verif` 单一真相源） |
| sigma_delta_dac/ | sigma_delta_dac.cpp | ✅ 1st-order |
| assert/ | assert_example.cpp | ✅ CTest registered |
| phase1c/ | phase1c_test.cpp | ✅ CTest registered |
| stream/ | MOVED → examples/stream/ | See sibling |

## CONVENTIONS
- Each example: `Component` subclass → `Top` wrapper → `main()` self-test
- **已迁移示例（三角色分离 · `rtl-verification-single-source`）**：设计定义 `*_top.h` +
  激励驱动 `tb_*.cpp`（默认模式 = 读 `.verif` 跑断言；`--emit-rtl` = 发射 RTL/TB）+
  验证源 `tb_*.verif`（**唯一语义来源**）⇒ ⇒ 激励与期望值 MUST NOT 写进 `.cpp`
  ⇒ ⇒ 已迁移：`pwm/` · `counter/`（两份）· `uart/` · `uart_rx/` · `fifo/` · `gpio/` ·
  `quadrature_encoder/` ⇒ ⇒ **8 个已登记产品面 target 全部迁移完毕**
- Build to `build/examples/spinalhdl-ported/<module>/<binary>`
- Test via `run_all_ported_tests.sh` (not CTest directly — uses timeout wrapper)

## testbench_template.cpp
Template for creating new ported examples. Copy and modify.

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**. New examples must:
- Register in `CMakeLists.txt` immediately (no orphan binaries)
- Include in `run_all_ported_tests.sh` (no unverified examples)
- Have working `main()` self-test (not empty stubs)
