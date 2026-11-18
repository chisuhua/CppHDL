/**
 * AXI4-SoC Example —— **激励驱动 + 发射入口**（`example-file-boundary-2026-11` · B6）
 *
 * ⚠️ **设计定义在 `axi_soc_example_top.h`**（`AxiSocTop` + `AxiLiteTestMaster`）；
 *    本文件只做三件事（三个模式）：
 *      · **默认模式** = 激励 + 断言 + `RESULT:` 协议行
 *      · **`--emit-rtl <dir>`** = 发射（**恒** `--hierarchical` · 蓝图 §4.3.2 分支 A）
 *      · **`--check-products`** = 发射 + 产品级自检（`soc_product_check.h` 的 4 项判据）
 *
 * ⚠️ **激励是手写过渡态**（蓝图 §7 R13）：UART 8N1 帧解码超出 `.verif` 封闭原语集
 *    （ADR-074 D2）⇒ ⇒ 扩展该原语集是**独立立项**；在此之前 C++ 侧 = **最小可用集**
 *    （GPIO 写 / 读 + 复位），**不要求**与 `tb_axi_soc_example.sv` 的 14 条对齐。
 *
 * ⚠️ **零落盘**：不再调 `toVerilog("axi_soc_example.v")`（那会把产物写进 CWD）；
 *    Verilog 由 `--emit-rtl <dir>` 发射。
 */

#include "ch.hpp"
#include "axi_soc_example_top.h"
#include "component.h"
#include "rtl_emit/cli.h"
#include "simulator.h"
#include "soc_product_check.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

/// 发射模式的根组件名（**MUST 与 `check_products` 的 owner 基准逐字一致**）
constexpr const char *kEmitRootName = "top";

/// 发射模式的 design factory（与 `run_emit_cli` 的 `DesignFactory` 同型）
std::unique_ptr<ch::Component> make_emit_design(ch::core::context *) {
    return std::make_unique<axi4::example::AxiSocTop>(nullptr, kEmitRootName);
}

} // namespace

int main(int argc, char **argv) {
    using namespace ch;

    // ── 0) 模式判定：**位置无关**剥离两个模式标记 ────────────────────────────
    bool emit_mode = false;
    bool check_mode = false;
    std::vector<std::string> rest;
    rest.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--emit-rtl") {
            emit_mode = true;
            continue;
        }
        if (a == "--check-products") {
            check_mode = true;
            continue;
        }
        rest.push_back(a);
    }

    // ── 发射 / 自检模式 ────────────────────────────────────────────────────
    //   ⚠️ 两者都要 `--out <dir>`（缺 ⇒ exit 2，**不回落**库层默认 `rtl_out/`）
    if (emit_mode || check_mode) {
        std::string out_dir;
        bool has_out = false;
        for (std::size_t i = 0; i + 1 < rest.size(); ++i) {
            if (rest[i] == "--out") {
                out_dir = rest[i + 1];
                has_out = true;
                break;
            }
        }
        if (!has_out) {
            std::cerr << "FATAL: --emit-rtl / --check-products 缺 --out <dir>\n";
            return 2;
        }

        // ⚠️ **分支 A**（蓝图 §4.3.2）：SoC 发射入口**恒发层次化** ⇒ 未显式给就补上
        bool has_hier = false;
        for (const auto &a : rest) {
            if (a == "--hierarchical") {
                has_hier = true;
            }
        }
        if (!has_hier) {
            rest.emplace_back("--hierarchical");
        }

        const int rc = rtl_emit::run_emit_cli(rest, make_emit_design);
        if (rc != 0 || !check_mode) {
            return rc;
        }
        return axi4::example::soc_check::check_products(out_dir, out_dir + "/top.v");
    }

    // ── 默认模式 fail-closed：收到**任何**未知参数 ⇒ exit 2 ──────────────────
    if (!rest.empty()) {
        std::cerr << "FATAL: 未知参数（默认模式只接受无参）：" << rest[0] << "\n";
        return 2;
    }

    // ── 默认模式：激励 + 断言 ───────────────────────────────────────────────
    ch::ch_device<axi4::example::AxiSocTop> top_device;
    ch::Simulator sim(top_device.context());

    int checks = 0;
    int fails = 0;

    // 初始化
    sim.set_input_value(top_device.instance().io().start, false);
    sim.set_input_value(top_device.instance().io().write, false);
    sim.set_input_value(top_device.instance().io().rst_n, true);
    sim.set_input_value(top_device.instance().io().gpio_in, 0x5a_h);
    sim.set_input_value(top_device.instance().io().uart_rx, true);

    // ---- 检查 1：写 GPIO DATA_OUT（addr=0x00）⇒ gpio_out == 0xAB ----
    {
        const std::uint64_t expect = 0xAB;
        sim.set_input_value(top_device.instance().io().addr, 0_d);
        sim.set_input_value(top_device.instance().io().wdata, 0xAB_h);
        sim.set_input_value(top_device.instance().io().write, true);
        sim.set_input_value(top_device.instance().io().start, true);
        sim.tick();
        sim.set_input_value(top_device.instance().io().start, false);
        bool done = false;
        for (int i = 0; i < 10; ++i) {
            sim.tick();
            if (static_cast<std::uint64_t>(sim.get_port_value(top_device.instance().io().done))) {
                done = true;
                break;
            }
        }
        // master 从写状态回 idle 差一拍（响应握手后的**下一个**沿）
        sim.tick();

        const auto gpio_out =
            static_cast<std::uint64_t>(sim.get_port_value(top_device.instance().io().gpio_out));
        ++checks;
        if (!done || gpio_out != expect) {
            ++fails;
            std::cerr << "CHECK gpio_write FAIL (done=" << done << " gpio_out=0x" << std::hex
                      << gpio_out << " want 0x" << expect << std::dec << ")\n";
        }
    }

    // ---- 检查 2：读 GPIO DATA_IN（addr=0x04）⇒ rdata == 0x5A ----
    {
        const std::uint64_t expect = 0x5A;
        sim.set_input_value(top_device.instance().io().addr, 4_d);
        sim.set_input_value(top_device.instance().io().write, false);
        sim.set_input_value(top_device.instance().io().start, true);
        sim.tick();
        sim.set_input_value(top_device.instance().io().start, false);
        bool done = false;
        std::uint64_t rdata = 0;
        for (int i = 0; i < 10; ++i) {
            sim.tick();
            if (static_cast<std::uint64_t>(sim.get_port_value(top_device.instance().io().done))) {
                done = true;
                rdata =
                    static_cast<std::uint64_t>(sim.get_port_value(top_device.instance().io().rdata));
                break;
            }
        }
        ++checks;
        if (!done || rdata != expect) {
            ++fails;
            std::cerr << "CHECK gpio_read FAIL (done=" << done << " rdata=0x" << std::hex << rdata
                      << " want 0x" << expect << std::dec << ")\n";
        }
    }

    // ── 结论行（协议：十进制 · 单行 · 无 ANSI）──────────────────────────────
    std::cout << "RESULT: checks=" << checks << " fails=" << fails << "\n";
    return fails == 0 ? 0 : 1;
}
