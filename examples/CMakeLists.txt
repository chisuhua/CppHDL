# examples/CMakeLists.txt
#
# `examples/` 的**聚合入口**。
#
# ⚠️ **为什么这个文件存在**：`examples/` 下的每个子目录各自有 CMakeLists.txt，
# 而**运行护栏**（`run_all_ported_tests.sh`）需要知道「**哪些 example 被跟踪**」。
# 若该集合散落在多处（脚本里硬编码 28 行 / 各子目录的 `add_executable` /
# `grep '^int main'` 的 32 个源文件），就会出现**新增即漏**的静默缺口
# —— 新 example 注册进 CMake 却忘了加进护栏 ⇒ 它**从未被运行**却看起来
# 「有覆盖」（与 `samples-regression-guard-2026-10` 治的病同型）。
#
# ⇒ **单一来源**：`CH_TRACKED_EXAMPLE_TARGETS` 同时驱动
#   ① 构建侧的聚合 target `all-examples`（本文件末尾）
#   ② 运行侧的护栏 `run_all_ported_tests.sh`
#      （它 `sed` 解析本文件的 `set(CH_TRACKED_EXAMPLE_TARGETS …)` 块，
#        范式对齐 `scripts/run_all_samples.sh:95-99` 解析
#        `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS`）
#
# ⚠️ **未被跟踪**的 main() 程序 / 测试二进制 MUST 登记进
# `examples/disabled_examples.txt`（带理由 + 归位条件）⇒ 「新增即漏」不再静默。
#
# ⚠️ `all-examples` **MUST 只含被跟踪的那 28 个** —— 不可整目录收编：
# `examples/riscv-mini/` 另有 18 个 Catch2 测试 target（见
# `examples/disabled_examples.txt`），`examples/verilator/` 另有 3 个 demo。
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# 被运行护栏跟踪的 example target 列表 = 回归护栏 run_all_ported_tests.sh
# 的覆盖集合。
#
# 格式：一行一个 target 名（与 target 同名 ⇒ 二进制名 = target 名）。
# `# PHASE:` 注释是**展示用**的分段标记，护栏会原样打印成小节标题。
# ---------------------------------------------------------------------------
set(CH_TRACKED_EXAMPLE_TARGETS
    # PHASE: Phase 1: 基础移植验证
    counter_simple_example
    counter_example
    fifo_stream_example
    uart_tx_example
    uart_rx_example

    # PHASE: Phase 2: 核心组件
    pwm_example
    gpio_example
    spi_controller_example
    i2c_controller_example
    timer_example

    # PHASE: Phase 2: 高级外设
    vga_controller_example
    ws2812_controller_example
    quadrature_encoder_example
    sigma_delta_dac_example

    # PHASE: Phase 1-3: 辅助组件
    width_adapter_example
    assert_example
    phase1c_test

    # PHASE: Phase 3: Stream 组件
    stream_mux_demo
    stream_demux_demo
    stream_arbiter_demo
    stream_fork_demo

    # PHASE: Phase 3A: AXI4 总线
    axi4_lite_example
    axi4_full_example
    axi_soc_example
    phase3a_complete

    # PHASE: RISC-V Mini
    rv32i_phase1_test
    rv32i_phase2_test
    rv32i_phase3_test
)

# ---------------------------------------------------------------------------
# 各示例子目录（从根 CMakeLists.txt 迁入 —— 见根文件
# `if(CPPHDL_BUILD_EXAMPLES)` 块内的 `add_subdirectory(examples)`）。
#
# ⚠️ 二进制目录与迁移前**完全一致**（`build/examples/<相对路径>/`），
#    因为 `add_subdirectory(examples)` 的 binary dir 就是 `build/examples`。
# ---------------------------------------------------------------------------

# SpinalHDL 移植示例
add_subdirectory(spinalhdl-ported/counter)
add_subdirectory(spinalhdl-ported/uart)
add_subdirectory(spinalhdl-ported/fifo)
add_subdirectory(spinalhdl-ported/width_adapter)
add_subdirectory(spinalhdl-ported/pwm)
add_subdirectory(spinalhdl-ported/gpio)
add_subdirectory(spinalhdl-ported/spi)
add_subdirectory(spinalhdl-ported/i2c)
add_subdirectory(spinalhdl-ported/timer)
add_subdirectory(spinalhdl-ported/assert)
add_subdirectory(spinalhdl-ported/phase1c)
add_subdirectory(spinalhdl-ported/uart_rx)
add_subdirectory(spinalhdl-ported/vga)
add_subdirectory(spinalhdl-ported/ws2812)
add_subdirectory(spinalhdl-ported/quadrature_encoder)
add_subdirectory(spinalhdl-ported/sigma_delta_dac)

# Phase 3: Stream 组件
add_subdirectory(stream)

# Phase 4: Verilator 仿真后端示例 (ADR-035)
add_subdirectory(verilator)

# Phase 3: AXI4 总线系统
add_subdirectory(axi4)

# Phase 3A: RISC-V SoC
# add_subdirectory(riscv)  # Deprecated, use riscv-mini
add_subdirectory(riscv-mini)  # Phase B: API 兼容,已启用

# ---------------------------------------------------------------------------
# 聚合 target：一次构建全部**被跟踪**的 example。
#
# 镜像 samples 侧既有的 `all-samples`（samples/CMakeLists.txt 末尾）——
# CI job 用它构建护栏所需的全部二进制，避免构建整个测试套件。
# ---------------------------------------------------------------------------
add_custom_target(all-examples)
add_dependencies(all-examples ${CH_TRACKED_EXAMPLE_TARGETS})
