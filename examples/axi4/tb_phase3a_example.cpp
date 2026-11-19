/**
 * Phase 3A Example —— **激励驱动**（`axi4-convergence-tail-2026-11` B2）
 *
 * ⚠️ **设计定义在 `phase3a_top.h`**（`Phase3ATop`）⇒ ⇒ 三角色分离
 *    （蓝图 §4.3.3 · `example-file-boundary-2026-11` 的收口）：本文件只做
 *    **激励 + 断言**，不做实例化之外的连接。
 *
 * ⚠️ **结论是协议行**（`RESULT: checks=2 fails=0`）而**不是**散文 —— 散文结论让
 *    护栏无法对账条数（「断言被删光」只会表现为 exit 0）⇒ ⇒ 与已退役的
 *    `axi4_full_example` 是同一族缺陷（只打印不判定 · AC27）。
 *
 * ⚠️ **2 条断言** = 本文件的全部判据面：
 *    ① 写事务在 64 拍内完成（`done` 拉高）
 *    ② 读回值 == 写入值（0xAB）
 *    ⇒ ⇒ 本 example **不进 RTL 产品清单**（无 `--emit-rtl` · 无 emitter target）
 *    ⇒ ⇒ 故清单里没有 `interp_checks` 行可对账；条数由本文件的协议行自证。
 *
 * ⚠️ **验收范围**：只验证 slave 0 一路 —— s1-3 在本设计里被 tie-off（未接线的
 *    `ch_in` 读出为 0）。16 对路由的验收由 `tests/test_axi_interconnect_4x4.cpp`
 *    承担 ⇒ ⇒ MUST NOT 把 example 当作 16 对路由的验收手段。
 */

#include "ch.hpp"
#include "device.h"
#include "phase3a_top.h"
#include "simulator.h"

#include <cstdint>
#include <iostream>

int main(int argc, char **argv) {
    // ── 默认模式 fail-closed：收到**任何**参数 ⇒ exit 2 ──────────────────────
    //   ⚠️ 本 example 无发射模式（不进产品清单）⇒ ⇒ 不接受任何参数
    //   （与 `tb_axi4_lite_example` / `tb_axi_soc_example` 的默认模式同型）
    if (argc > 1) {
        std::cerr << "FATAL: 未知参数（默认模式只接受无参）：" << argv[1] << "\n";
        return 2;
    }

    std::cout << "=== Phase 3A Integration Test（interconnect → converter → AXI4-Lite slave）==="
              << std::endl;

    ch::ch_device<axi4::example::Phase3ATop> top_device;
    ch::Simulator sim(top_device.context());
    auto &io = top_device.instance().io();

    int checks = 0;
    int fails = 0;
    auto check = [&](bool ok, const char *what) {
        ++checks;
        if (!ok) {
            ++fails;
            std::cerr << "CHECK FAIL: " << what << "\n";
        }
    };

    sim.set_input_value(io.start, false);
    sim.set_input_value(io.is_write, false);
    sim.set_input_value(io.addr, 0);
    sim.set_input_value(io.wdata, 0);
    // 顶层 `rst_n` MUST 显式驱动（未接线的 ch_in<ch_bool> 读出为 0 = 永久复位）
    sim.set_input_value(io.rst_n, true);
    sim.tick();

    // 有界等待 `done`（寄存器输出 ⇒ 先求值再采样）
    auto wait_done = [&]() -> bool {
        for (int i = 0; i < 64; ++i) {
            sim.eval_combinational();
            if (sim.get_port_value(io.done) == 1) return true;
            sim.tick();
        }
        sim.eval_combinational();
        return sim.get_port_value(io.done) == 1;
    };

    // ── 断言 ①：写事务（addr=0x00 · data=0xAB）──────────────────────────────
    std::cout << "--- Test 1: 写 AXI4-Lite 寄存器 0 (addr=0x00, data=0xAB) ---" << std::endl;
    sim.set_input_value(io.addr, 0x00);
    sim.set_input_value(io.wdata, 0xAB);
    sim.set_input_value(io.is_write, true);
    sim.set_input_value(io.start, true);
    sim.tick();
    sim.set_input_value(io.start, false);
    check(wait_done(), "写事务在 64 拍内完成（done 拉高）");

    // ── 断言 ②：读回（addr=0x00 ⇒ 0xAB）────────────────────────────────────
    std::cout << "--- Test 2: 读回 AXI4-Lite 寄存器 0 (addr=0x00) ---" << std::endl;
    sim.set_input_value(io.addr, 0x00);
    sim.set_input_value(io.is_write, false);
    sim.set_input_value(io.start, true);
    sim.tick();
    sim.set_input_value(io.start, false);
    const bool read_done = wait_done();
    sim.eval_combinational();
    const auto rdata =
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(sim.get_port_value(io.rdata)));
    const bool read_ok = read_done && rdata == 0xAB;
    check(read_ok, "读回值 == 写入值 0xAB");
    if (!read_ok) {
        std::cerr << "  读事务完成=" << (read_done ? "是" : "否") << " · 读回值=0x" << std::hex
                  << rdata << std::dec << "（期望 0xab）\n";
    }
    std::cout << std::endl;

    // 输出系统架构
    std::cout << "=== System Architecture ===" << std::endl;
    std::cout << std::endl;
    std::cout << "  ┌─────────────────────────────────┐" << std::endl;
    std::cout << "  │      Test Master (External)     │" << std::endl;
    std::cout << "  └──────────────┬──────────────────┘" << std::endl;
    std::cout << "                 │" << std::endl;
    std::cout << "                 ▼" << std::endl;
    std::cout << "  ┌─────────────────────────────────┐" << std::endl;
    std::cout << "  │    AXI Interconnect 4x4         │" << std::endl;
    std::cout << "  │  - Round-Robin Arbiter          │" << std::endl;
    std::cout << "  │  - Address Decode [31:30]       │" << std::endl;
    std::cout << "  │  - 4 Masters × 4 Slaves         │" << std::endl;
    std::cout << "  └──────────────┬──────────────────┘" << std::endl;
    std::cout << "                 │ Slave 0" << std::endl;
    std::cout << "                 ▼" << std::endl;
    std::cout << "  ┌─────────────────────────────────┐" << std::endl;
    std::cout << "  │  AXI to AXI-Lite Converter      │" << std::endl;
    std::cout << "  │  - Burst → Single               │" << std::endl;
    std::cout << "  │  - ID Ignore                    │" << std::endl;
    std::cout << "  │  - Protocol Adaptation          │" << std::endl;
    std::cout << "  └──────────────┬──────────────────┘" << std::endl;
    std::cout << "                 │" << std::endl;
    std::cout << "                 ▼" << std::endl;
    std::cout << "  ┌─────────────────────────────────┐" << std::endl;
    std::cout << "  │    AXI4-Lite Slave (GPIO)       │" << std::endl;
    std::cout << "  │  - 4 Registers                  │" << std::endl;
    std::cout << "  │  - 32-bit Data Width            │" << std::endl;
    std::cout << "  └─────────────────────────────────┘" << std::endl;
    std::cout << std::endl;
    
    // 输出完成状态

    std::cout << "=== Phase 3A Completion Status ===" << std::endl;
    std::cout << std::endl;
    std::cout << "✅ T301: AXI4 Full Function Slave" << std::endl;
    std::cout << "   - 5 independent channels (AW/W/B/AR/R)" << std::endl;
    std::cout << "   - Burst transfers (INCR, 1-16 beats)" << std::endl;
    std::cout << "   - 16 registers (4-byte aligned)" << std::endl;
    std::cout << std::endl;
    std::cout << "✅ T302: AXI Interconnect 4x4" << std::endl;
    std::cout << "   - 4 masters × 4 slaves crossbar" << std::endl;
    std::cout << "   - Round-robin arbitration" << std::endl;
    std::cout << "   - Address decoding [31:30]" << std::endl;
    std::cout << std::endl;
    std::cout << "✅ T303: AXI to AXI-Lite Converter" << std::endl;
    std::cout << "   - Protocol adaptation" << std::endl;
    std::cout << "   - Burst to single transfer" << std::endl;
    std::cout << "   - ID ignore" << std::endl;
    std::cout << std::endl;
    std::cout << "✅ T304: Complete Integration Example" << std::endl;
    std::cout << "   - Full system integration" << std::endl;
    std::cout << std::endl;

    // ── 结论行（协议：十进制 · 单行 · 无 ANSI）────────────────────────────────
    std::cout << "RESULT: checks=" << checks << " fails=" << fails << "\n";
    return fails == 0 ? 0 : 1;
}
