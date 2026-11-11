// src/rtl_emit/tb_stub.cpp
//
// **测试台骨架生成**的实现（`examples-rtl-sim-coverage` E2）—— 见头文件的口径。

#include "rtl_emit/tb_stub.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <vector>

namespace ch {
namespace rtl_emit {
namespace {

/// 端口名右侧补齐（注释块对齐用）
std::string pad(const std::string &s, std::size_t width) {
    if (s.size() >= width) {
        return s;
    }
    return s + std::string(width - s.size(), ' ');
}

/// 声明一个端口信号（输入 tie-off 的初始化在 ⑤ 之前用 `initial` 不做 —— 直接用
/// `logic ... = 0;` 的 NSDMI 形态，语义更直白且**不**依赖过程块顺序）。
void emit_signal(std::ostream &out, const RtlPortEntry &p) {
    out << "    logic ";
    if (p.width > 1) {
        out << "[" << (p.width - 1) << ":0] ";
    }
    out << p.name;
    if (p.dir == "input" && p.role != "clock" && p.role != "reset") {
        out << " = 0"; // tie-off：**不含**语义，只是让骨架能编译且可跑
    }
    out << ";\n";
}

/// ⚠️ **该条目是否是产物的顶层端口**（E2 · 实测踩坑）。
///
/// **为什么需要过滤**：manifest 的 `ports[]` 是**全部** `type_input`/`type_output`，
/// 而**产物**的顶层端口表按 F3 的契约是
/// 「clock → reset → **无 driver 的 input** → **全部 `ch_out`**」
/// ⇒ ⇒ 子组件的**有 driver 的 input**在扁平化后是**内部 wire**（不是端口）
/// ⇒ ⇒ 盲目按 manifest 连线会得到
/// `Instance attempts to connect to 'top_leaf_data_in', but it is a variable`（实测）。
///
/// 判据（**只用 manifest 现有字段**，与 F3 的契约对齐）：
/// · `role ∈ {clock, reset}` ⇒ 顶层；
/// · `dir == "output"` ⇒ 顶层（**全部** `ch_out`，含 inline 后代冒泡上来的）；
/// · `is_top_io`（根组件自有 IO）⇒ 顶层；
/// · 其余（**子组件的 input**）⇒ **不是**顶层端口 ⇒ **不连**。
bool is_top_level_port(const RtlPortEntry &p) {
    if (p.role == "clock" || p.role == "reset") {
        return true;
    }
    if (p.dir == "output") {
        return true;
    }
    return p.is_top_io;
}

} // namespace

void write_tb_stub(std::ostream &out, const RtlManifest &manifest,
                   const TbStubOptions &options) {
    const std::string tb_module = options.tb_module.empty()
                                      ? ("tb_" + options.dut_module + "_stub")
                                      : options.tb_module;
    const std::string hint = options.example_hint.empty()
                                 ? std::string("<example>")
                                 : options.example_hint;

    // ── ① 文件头（**无时间戳** ⇒ ⇒ 可重复生成、可 diff）────────────────
    out << "// " << tb_module << ".sv\n";
    out << "//\n";
    out << "// **测试台骨架** —— 由 `cpphdl-emit-rtl --emit-tb-stub` 生成。\n";
    out << "// 本文件**只有**端口连线 + 时钟/复位 + VCD 开关；激励与断言 MUST 由人补齐。\n";
    out << "//\n";
    out << "// ⚠️ 补完的 TB MUST 在文件头登记**期望值出处**\n";
    out << "//    （协议规范 / 手推推导 / C++ 侧断言位置）—— 仓内纪律：期望值 MUST 手推。\n";
    out << "// ⚠️ 补完的 TB MUST 结尾打印 `RESULT: checks=<N> fails=0`\n";
    out << "//    （运行护栏 `scripts/verify_examples_rtl_sim.sh` 据此判红绿）。\n";
    out << "\n";
    out << "`timescale 1ns/1ps\n";
    out << "\n";
    out << "module " << tb_module << ";\n";
    out << "\n";

    // ── ② 端口清单注释块（来自 manifest）────────────────────────────
    out << "    // ── 端口清单（manifest · rtl-emit-manifest-v1）────────────────\n";
    out << "    // " << pad("name", 46) << pad("dir", 8) << pad("width", 7)
        << pad("role", 8) << pad("is_top_io", 11) << "owner\n";
    std::size_t name_w = 4;
    for (const auto &p : manifest.ports) {
        if (is_top_level_port(p)) {
            name_w = std::max(name_w, p.name.size());
        }
    }
    name_w = std::min<std::size_t>(name_w + 2, 46);
    for (const auto &p : manifest.ports) {
        if (!is_top_level_port(p)) {
            out << "    // (内部 wire，非顶层端口) " << p.name << " · owner="
                << (p.owner.empty() ? "<ctx>" : p.owner) << "\n";
            continue;
        }
        out << "    // " << pad(p.name, name_w) << pad(p.dir, 8)
            << pad(std::to_string(p.width), 7) << pad(p.role, 8)
            << pad(p.is_top_io ? "yes" : "no", 11)
            << (p.owner.empty() ? "<ctx>" : p.owner) << "\n";
    }
    out << "\n";

    // ── ③ 端口信号声明 ───────────────────────────────────────────────
    out << "    // ── 端口信号（输入已 tie-off 到 0）──────────────────────────────\n";
    for (const auto &p : manifest.ports) {
        if (is_top_level_port(p)) {
            emit_signal(out, p);
        }
    }
    out << "\n";

    // ── ④ DUT 实例化（按 manifest 顺序）──────────────────────────────
    out << "    // ── DUT ──────────────────────────────────────────────────────\n";
    out << "    " << options.dut_module << " dut (\n";
    {
        std::vector<const RtlPortEntry *> top_ports;
        for (const auto &p : manifest.ports) {
            if (is_top_level_port(p)) {
                top_ports.push_back(&p);
            }
        }
        for (std::size_t i = 0; i < top_ports.size(); ++i) {
            const auto &p = *top_ports[i];
            out << "        ." << p.name << "(" << p.name << ")"
                << (i + 1 < top_ports.size() ? "," : "") << "\n";
        }
    }
    out << "    );\n";
    out << "\n";

    // ── ⑤ 时钟 / 复位驱动骨架 ────────────────────────────────────────
    const std::string clock_port =
        [&]() -> std::string {
        for (const auto &p : manifest.ports) {
            if (p.role == "clock") {
                return p.name;
            }
        }
        return {};
    }();
    const std::string reset_port =
        [&]() -> std::string {
        for (const auto &p : manifest.ports) {
            if (p.role == "reset") {
                return p.name;
            }
        }
        return {};
    }();

    out << "    // ── 时钟 / 复位 ────────────────────────────────────────────────\n";
    out << "    integer checks = 0;\n";
    out << "    integer fails = 0;\n";
    if (!clock_port.empty()) {
        out << "    initial " << clock_port << " = 0;\n";
        out << "    always #" << options.clock_half_period_ns << " " << clock_port
            << " = ~" << clock_port << ";\n";
    } else {
        out << "    // ⚠️ 清单里**没有** `role == clock` 的端口 ⇒ ⇒ 本骨架无法驱动时钟。\n";
        out << "    //    若被测设计确实无时钟（纯组合）⇒ ⇒ 删掉本段即可。\n";
    }
    out << "\n";

    // ── ⑥ 主过程：复位 + **留空**的激励/断言 ─────────────────────────
    out << "    initial begin\n";
    if (!reset_port.empty()) {
        out << "        // 复位脉冲（" << options.reset_cycles << " 拍）\n";
        out << "        " << reset_port << " = 1;\n";
        out << "        repeat (" << options.reset_cycles << ") @(posedge "
            << (clock_port.empty() ? reset_port : clock_port) << ");\n";
        out << "        " << reset_port << " = 0;\n";
        out << "        @(posedge " << (clock_port.empty() ? reset_port : clock_port)
            << ");\n";
    }
    out << "\n";
    out << "        // ── TODO（" << hint << "）：激励 ────────────────────────────\n";
    out << "        //   在这里逐拍驱动输入并推进时钟。\n";
    out << "        //   ⚠️ 不得只写 `$finish` —— 那是「TB 只编译不跑」的历史事故形态。\n";
    out << "\n";
    out << "        // ── TODO（" << hint << "）：断言 ────────────────────────────\n";
    out << "        //   `checks = checks + 1; if (<条件>) fails = fails + 1;`\n";
    out << "        //   ⚠️ 期望值 MUST 有**外部出处**（协议规范 / 手推）—— 禁抄仿真输出。\n";
    out << "        //   ⚠️ 每个输出 MUST 至少有一条 X 检查：\n";
    out << "        //      `checks = checks + 1; if ($isunknown(<输出>)) fails = fails + 1;`\n";
    out << "        //      （未驱动 / 半驱动输出在默认 Verilog 语义下是 X ⇒ 值断言可能\n";
    out << "        //       恰好通过，只有 X 检查能抓到「信号根本没接上」。）\n";
    out << "\n";
    // ⚠️ 注释措辞注意：**不**在注释里写 "verilator" 这个词，也**不**让注释行
    //   以 `*` / `/` 结尾 —— 仿真的 pragma 扫描会把 `/*verilator ... */` 当指令
    //   ⇒ ⇒ 实测：`Verilator 的 ...` 被当成 `Unknown verilator comment` ⇒ 编译失败。
    out << "        // 收尾约定：运行护栏按这一行判红绿（不是退出码：$finish 恒返回 0）。\n";
    out << "        $display(\"RESULT: checks=%0d fails=%0d\", checks, fails);\n";
    out << "        if (fails != 0) $fatal(1, \"TB failed\");\n";
    out << "        $finish;\n";
    out << "    end\n";
    out << "\n";

    // ── ⑦ VCD 开关 ───────────────────────────────────────────────────
    out << "`ifdef DUMP_VCD\n";
    out << "    initial begin\n";
    out << "        $dumpfile(\"" << tb_module << ".vcd\");\n";
    out << "        $dumpvars(0, " << tb_module << ");\n";
    out << "    end\n";
    out << "`endif\n";
    out << "\n";
    out << "endmodule\n";
}

} // namespace rtl_emit
} // namespace ch
