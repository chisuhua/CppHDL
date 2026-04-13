#!/bin/bash
# ============================================================
# @file run_all_ported_tests.sh
# @brief 运行所有 SpinalHDL 移植示例的 main() 自测并验证
#
# 用法: ./run_all_ported_tests.sh [build_dir]
#   build_dir: 构建目录，默认为 build
#
# 返回值: 0=全部通过, 非0=有失败测试数
#
# 作者: DevMate
# 日期: 2026-04-12
# ============================================================
set -euo pipefail

BUILD_DIR="${1:-build}"

if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: Build directory '$BUILD_DIR' not found"
    echo "Run: cmake -B build && cmake --build build"
    exit 1
fi

cd "$BUILD_DIR"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

PASS=0
FAIL=0
TOTAL=0

declare -a FAIL_NAMES=()

echo "=========================================="
echo " SpinalHDL 移植示例验证"
echo "=========================================="
echo "Build directory: $(pwd)"
echo "Timeout: 30s per test"
echo ""

run_test() {
    local name="$1"
    local exe="$2"
    TOTAL=$((TOTAL + 1))
    
    printf "  %-40s " "$name"
    
    if [ ! -f "$exe" ]; then
        printf "${YELLOW}SKIPPED (binary not found)${NC}\n"
        return 2
    fi
    
    if timeout 30 "$exe" > /dev/null 2>&1; then
        printf "${GREEN}PASS${NC}\n"
        PASS=$((PASS + 1))
    else
        local rc=$?
        if [ $rc -eq 124 ]; then
            printf "${RED}TIMEOUT (>30s)${NC}\n"
        else
            printf "${RED}FAILED (exit code: $rc)${NC}\n"
        fi
        FAIL=$((FAIL + 1))
        FAIL_NAMES+=("$name")
    fi
}

echo "--- Phase 1: 基础移植验证 ---"
run_test "counter_simple" "examples/spinalhdl-ported/counter/counter_simple_example"
run_test "counter" "examples/spinalhdl-ported/counter/counter_example"
run_test "fifo" "examples/spinalhdl-ported/fifo/fifo_stream_example"
run_test "uart_tx" "examples/spinalhdl-ported/uart/uart_tx_example"
run_test "uart_rx" "examples/spinalhdl-ported/uart_rx/uart_rx_example"

echo ""
echo "--- Phase 2: 核心组件 ---"
run_test "pwm" "examples/spinalhdl-ported/pwm/pwm_example"
run_test "gpio" "examples/spinalhdl-ported/gpio/gpio_example"
run_test "spi" "examples/spinalhdl-ported/spi/spi_controller_example"
run_test "i2c" "examples/spinalhdl-ported/i2c/i2c_controller_example"
run_test "timer" "examples/spinalhdl-ported/timer/timer_example"

echo ""
echo "--- Phase 2: 高级外设 ---"
run_test "vga" "examples/spinalhdl-ported/vga/vga_controller_example"
run_test "ws2812" "examples/spinalhdl-ported/ws2812/ws2812_controller_example"
run_test "quadrature_encoder" "examples/spinalhdl-ported/quadrature_encoder/quadrature_encoder_example"
run_test "sigma_delta_dac" "examples/spinalhdl-ported/sigma_delta_dac/sigma_delta_dac_example"

echo ""
echo "--- Phase 1-3: 辅助组件 ---"
run_test "width_adapter" "examples/spinalhdl-ported/width_adapter/width_adapter_example"
run_test "assert" "examples/spinalhdl-ported/assert/assert_example"
run_test "phase1c_integration" "examples/spinalhdl-ported/phase1c/phase1c_test"

echo ""
echo "--- Phase 3: Stream 组件 ---"
run_test "stream_mux" "examples/stream/stream_mux_demo"
run_test "stream_demux" "examples/stream/stream_demux_demo"
run_test "stream_arbiter" "examples/stream/stream_arbiter_demo"
run_test "stream_fork" "examples/stream/stream_fork_demo"

echo ""
echo "--- Phase 3A: AXI4 总线 ---"
run_test "axi4_lite" "examples/axi4/axi4_lite_example"
run_test "axi4_full" "examples/axi4/axi4_full_example"
run_test "axi_soc" "examples/axi4/axi_soc_example"
run_test "axi4_phase3a" "examples/axi4/phase3a_complete"

echo ""
echo "--- RISC-V Mini ---"
run_test "rv32i_phase1" "examples/riscv-mini/rv32i_phase1_test"
run_test "rv32i_phase2" "examples/riscv-mini/rv32i_phase2_test"
run_test "rv32i_phase3" "examples/riscv-mini/rv32i_phase3_test"

echo ""
echo "=========================================="
skipped=$((TOTAL - PASS - FAIL))
printf "  ${GREEN}PASS:  %-4d${NC} ${RED}FAIL: %-4d${NC} SKIP: %-4d\n" "$PASS" "$FAIL" "$skipped"
echo "=========================================="

if [ ${#FAIL_NAMES[@]} -gt 0 ]; then
    echo ""
    echo "Failed tests:"
    for name in "${FAIL_NAMES[@]}"; do
        echo "  - $name"
    done
    exit "$FAIL"
fi

exit 0
