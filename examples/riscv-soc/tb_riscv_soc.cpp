/**
 * RV32I SoC 级产品 —— **激励驱动 + 发射入口**（ADR-077 D1 · change `riscv-soc-skeleton-2026-11`）
 *
 * ⚠️ **设计定义在 `src/rv32i_soc_top.h`**（`Rv32iSocTop` + 2 处 `ch::ch_layer<T>`）；
 *    本文件三个模式：
 *      · **默认** = 解释器路径 + 手推断言 + `RESULT:` 协议行
 *      · **`--emit-rtl <dir>`** = 发射（恒加 `--hierarchical`）
 *      · **`--check-products <dir>`** = 发射 + 4 项产品级自检
 *
 * ⚠️ **期望值 MUST 手推**（禁抄仿真输出）：程序与写回/存储结果**同源于**核级产品
 *    （`.rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc` 的「小程序」表）
 *    ⇒ ⇒ 本文件**不新造**任何期望 ⇒ 手推台账零重复。
 *
 * ⚠️ **SoC 级看不到核的 `dbg_*` / `obs_*`**（ADR-072 D1：后代 `ch_out` 不是顶层端口）
 *    ⇒ ⇒ 本文件的观测面 = **根自有**的两个计数（`store_count` / `writeback_count`）。
 *
 * ⚠️ **本文件只留「值」**（期望 module 集 / 端口数 / 激励面 / 观测面 / 阈值）；
 *    规则侧（4 项判据 + 编排）在库层 `ch::rtl_emit::check_products`。
 */

#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "hierarchy_equivalence.h"
#include "hierarchy_verilog.h"
#include "rtl_emit/cli.h"
#include "rtl_emit/product_check.h"
#include "rv32i_soc_top.h"
#include "simulator.h"

#include "cpu/riscv/rv32i_asm_encode.h"

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

using riscv::ADD;
using riscv::ADDI;
using riscv::JAL;
using riscv::NOP;
using riscv::SW;

// ---------------------------------------------------------------------------
// 7 字小程序（**手推** · 与核级产品**同源** ⇒ 期望值零新造）
//
// | 字 | 指令 | 语义 |
// |---|---|---|
// | 0 | `addi x1, x0, 42` | x1 = 42 |
// | 1 | `nop` | 余量 |
// | 2 | `addi x2, x0, 11` | x2 = 11 |
// | 3 | `nop` | 余量 |
// | 4 | `add x3, x1, x2` | x3 = **53** |
// | 5 | `sw x3, 0(x0)` | [字节地址 0] = 53 |
// | 6 | `jal x0, 0` | 自环（`rd = x0` ⇒ **不写回**） |
//
// ⇒ ⇒ `store_count == 1` · `writeback_count == 3`（**两值不同** ⇒ 事件源接反必红）
// ---------------------------------------------------------------------------

constexpr std::size_t kProgWords = 7;

/// 程序末尾之后 MUST 填 NOP（`ch_mem` 初值 0 的 opcode = 未知 ⇒ 流水线冻结）。
constexpr std::size_t kFillWords = 64;

/// 观测拍数（与 `.sv` TB 的 `repeat` 一致）。
constexpr std::size_t kCycles = 60;

/// 尾段起点：此后计数 MUST 不再增长（`jal` 自环已稳定）。
constexpr std::size_t kTailStart = 40;

/// **手算**编码常量（独立于库编码器的交叉校验证据）。
constexpr uint32_t kHandEncoded[kProgWords] = {
    0x02A00093u, // 0  addi x1, x0, 42
    0x00000013u, // 1  nop
    0x00B00113u, // 2  addi x2, x0, 11
    0x00000013u, // 3  nop
    0x002081B3u, // 4  add  x3, x1, x2
    0x00302023u, // 5  sw   x3, 0(x0)
    0x0000006Fu, // 6  jal  x0, 0
};

/// 程序字（取自库编码器 ⇒ 全仓唯一一份编码器）。
constexpr uint32_t kProgram[kProgWords] = {
    ADDI(1, 0, 42), NOP, ADDI(2, 0, 11), NOP, ADD(3, 1, 2), SW(3, 0, 0), JAL(0, 0),
};

// 手算常量 × 库编码器 的交叉校验（编译期 ⇒ 恒生效 ⇒ 零运行期成本）。
static_assert(kProgram[0] == kHandEncoded[0], "字 0 编码与手算常量不符");
static_assert(kProgram[1] == kHandEncoded[1], "字 1 编码与手算常量不符");
static_assert(kProgram[2] == kHandEncoded[2], "字 2 编码与手算常量不符");
static_assert(kProgram[3] == kHandEncoded[3], "字 3 编码与手算常量不符");
static_assert(kProgram[4] == kHandEncoded[4], "字 4 编码与手算常量不符");
static_assert(kProgram[5] == kHandEncoded[5], "字 5 编码与手算常量不符");
static_assert(kProgram[6] == kHandEncoded[6], "字 6 编码与手算常量不符");

/// **手推**计数期望值（ISA 语义：3 条写回指令 · 1 条 store 指令）。
constexpr uint64_t kExpectedStoreCount = 1;
constexpr uint64_t kExpectedWritebackCount = 3;

/// 判据计数器（失败进 stderr · 结论行进 stdout）。
class Checker {
public:
    void check(bool ok, const std::string &what) {
        ++checks_;
        if (!ok) {
            ++fails_;
            std::cerr << "CHECK FAIL: " << what << "\n";
        }
    }
    std::size_t checks() const { return checks_; }
    std::size_t fails() const { return fails_; }

private:
    std::size_t checks_ = 0;
    std::size_t fails_ = 0;
};

// ── 值侧 ①：冻结的产物 module 期望集（ADR-077 V2 · **手推**）──────────────
//
// ⚠️ 禁从 `ch_layer` 调用点派生 —— 否则「漏一个成层声明」会**同时**改变期望
//    ⇒ ⇒ 判据恒绿。
// ⚠️ 与 `.rddf/checks/soc-rtl-expectations.md#riscv-soc` 登记的集合 MUST 一致。
const std::vector<std::string> kExpectedModules = {
    "top",
    "Rv32iCoreComplex",
    "SocTraceCounter",
    "IfStage",
    "IdStage",
    "ExStage",
    "MemStage",
    "WbStage",
    "HazardUnit",
    "ForwardingMux_32_",
};

/// 顶层端口数 = 框架 2 + 根自有 6（ADR-077 V1）。
constexpr std::size_t kExpectedTopPorts = 8;

/// 最小变化观测量数（签批 spec `rtl-soc-output` 的硬要求 ⇒ **不是**可调参数）。
constexpr std::size_t kMinVaryingObservations = 2;

// ── 平台胶水：verilator 发现 + 执行（照 `tb_axi_soc_example.cpp`）───────────
std::string find_verilator() {
    if (const char *env = std::getenv("VERILATOR"); env && *env) return env;
    const auto usable = [](const std::string &p) {
        return !p.empty() && std::filesystem::exists(p);
    };
    const std::string src_local =
        std::string(CPPHDL_SOURCE_DIR) + "/build/verilator-install/bin/verilator";
    if (usable(src_local)) return src_local;
    const std::string local = "build/verilator-install/bin/verilator";
    if (usable(local)) return local;
    if (const char *path = std::getenv("PATH")) {
        std::string p(path);
        std::size_t pos = 0;
        while ((pos = p.find(':', pos)) != std::string::npos) {
            const std::string cand = p.substr(0, pos) + "/verilator";
            if (usable(cand)) return cand;
            pos += 1;
        }
    }
    return {};
}

bool run_lint(const std::string &verilator, const std::string &top_module,
              const std::string &verilog_path) {
#if defined(_WIN32)
    (void)verilator;
    (void)top_module;
    (void)verilog_path;
    return false; // fail-closed：无 POSIX 进程 API ⇒ 按失败计
#else
    const pid_t pid = ::fork();
    if (pid == 0) {
        ::execl(verilator.c_str(), verilator.c_str(), "--lint-only", "--top-module",
                top_module.c_str(), verilog_path.c_str(), static_cast<char *>(nullptr));
        ::_exit(127);
    }
    if (pid < 0) return false;
    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

std::unique_ptr<ch::Component> make_emit_design(ch::core::context *) {
    return std::make_unique<riscv_soc::Rv32iSocTop>(nullptr, riscv_soc::kSocRootName);
}

// ── 值侧 ②：双形态逐拍等价的激励面 / 观测面 ───────────────────────────────
//
// ⚠️ 输入面 MUST 覆盖根的**全部** 4 个 `ch_in`（缺一个 ⇒ 该端口在比对里读到未定义值）。
std::vector<ch::hier_eq::DrivenInput> make_inputs(riscv_soc::Rv32iSocTop &root) {
    std::vector<ch::hier_eq::DrivenInput> in;
    // ⚠️ MUST 用 `set_input_value`（`<<=` 会把端口变成内部驱动 ⇒ 假分歧）
    in.push_back({"top.ext_rst", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().ext_rst, v & 1);
                  }});
    in.push_back({"top.prog_we", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().prog_we, v & 1);
                  }});
    in.push_back({"top.prog_addr", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().prog_addr, v & 0xFFFFFFFFu);
                  }});
    in.push_back({"top.prog_data", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().prog_data, v & 0xFFFFFFFFu);
                  }});
    return in;
}

/// 顶层观测面（**只能**取根的 `ch_out` —— 后代 `ch_out` 在层次化顶层不可观测）。
std::vector<ch::hier_eq::Observation> make_observations(riscv_soc::Rv32iSocTop &root) {
    std::vector<ch::hier_eq::Observation> obs;
    obs.push_back(ch::hier_eq::make_observation(
        "top.store_count", root.io().store_count, [&root](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(root.io().store_count));
        }));
    obs.push_back(ch::hier_eq::make_observation(
        "top.writeback_count", root.io().writeback_count,
        [&root](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(root.io().writeback_count));
        }));
    return obs;
}

/// 激励序列：装载 7 字 → 填 NOP → 释放复位若干拍（合计 ≥10 拍且各拍值不同）。
///
/// ⚠️ 顺序 MUST 与 `make_inputs()` 一致：ext_rst · prog_we · prog_addr · prog_data
std::vector<std::vector<std::uint64_t>> make_stimulus() {
    std::vector<std::vector<std::uint64_t>> stim;
    // 装载期：ext_rst = 1（核被复位 ⇒ 计数器不被装载动作污染）
    for (std::size_t i = 0; i < kProgWords; ++i) {
        stim.push_back({1, 1, static_cast<std::uint64_t>(i),
                        static_cast<std::uint64_t>(kProgram[i])});
    }
    // 填充 NOP（地址继续递增 ⇒ 激励值持续变化）
    for (std::size_t i = kProgWords; i < 16; ++i) {
        stim.push_back({1, 1, static_cast<std::uint64_t>(i), static_cast<std::uint64_t>(NOP)});
    }
    // 释放复位 + 跑够拍数（计数在此段变化）
    for (std::size_t i = 0; i < 24; ++i) {
        stim.push_back({0, 0, 0, 0});
    }
    return stim;
}

ch::rtl_emit::ProductCheckSpec make_spec(const std::string &out_dir) {
    ch::rtl_emit::ProductCheckSpec spec;
    spec.verilog_path = out_dir + "/top.v";
    spec.work_dir = out_dir;
    spec.expected_modules = kExpectedModules;
    spec.expected_top_ports = kExpectedTopPorts;
    spec.min_varying_observations = kMinVaryingObservations;
    spec.lint_top_module = ch::HierarchyEmitOptions{}.top_module_name;
    spec.lint = [](const std::string &verilog_path) {
        const std::string verilator = find_verilator();
        if (verilator.empty()) return false; // fail-closed：找不到 linter ⇒ 按失败计
        return run_lint(verilator, ch::HierarchyEmitOptions{}.top_module_name, verilog_path);
    };
    return spec;
}

ch::rtl_emit::ProductCheckHooks make_hooks() {
    ch::rtl_emit::ProductCheckHooks hooks;
    hooks.make_inputs = [](ch::Component &root) {
        return make_inputs(static_cast<riscv_soc::Rv32iSocTop &>(root));
    };
    hooks.make_observations = [](ch::Component &root) {
        return make_observations(static_cast<riscv_soc::Rv32iSocTop &>(root));
    };
    hooks.make_stimulus = []() { return make_stimulus(); };
    return hooks;
}

// ---------------------------------------------------------------------------
// 默认模式：解释器路径 + 手推断言（A1–A6）
// ---------------------------------------------------------------------------

int run_verification() {
    Checker ck;

    auto dev = std::make_unique<ch::ch_device<riscv_soc::Rv32iSocTop>>();
    Simulator sim(dev->context());
    auto &io = dev->instance().io();

    // ── A1：复位/初始状态两计数为 0 ──
    ck.check(sim.get_port_value(io.store_count) == 0, "A1 store_count 初始 == 0");
    ck.check(sim.get_port_value(io.writeback_count) == 0, "A1 writeback_count 初始 == 0");

    // ── 装载：装载期 ext_rst = 1（核保持复位 ⇒ 装载动作不产生写回/store）──
    sim.set_input_value(io.ext_rst, 1u);
    sim.set_input_value(io.prog_we, 1u);
    bool load_phase_clean = true;
    for (std::size_t i = 0; i < kProgWords; ++i) {
        sim.set_input_value(io.prog_addr, static_cast<uint64_t>(i));
        sim.set_input_value(io.prog_data, kProgram[i]);
        sim.tick(1);
        if (sim.get_port_value(io.store_count) != 0 ||
            sim.get_port_value(io.writeback_count) != 0) {
            load_phase_clean = false;
        }
    }
    for (std::size_t i = kProgWords; i < kFillWords; ++i) {
        sim.set_input_value(io.prog_addr, static_cast<uint64_t>(i));
        sim.set_input_value(io.prog_data, NOP);
        sim.tick(1);
        if (sim.get_port_value(io.store_count) != 0 ||
            sim.get_port_value(io.writeback_count) != 0) {
            load_phase_clean = false;
        }
    }
    sim.set_input_value(io.prog_we, 0u);
    sim.set_input_value(io.prog_addr, 0u);
    sim.set_input_value(io.prog_data, 0u);

    // ── A2：装载期两计数保持 0（`ext_rst` 接反 ⇒ 装载被当指令执行 ⇒ 必红）──
    ck.check(load_phase_clean, "A2 装载期两计数保持 0");

    // ── 释放复位 ──
    sim.set_input_value(io.ext_rst, 0u);

    // ── 逐拍观测（A3–A6）──
    bool monotonic = true;
    uint64_t last_store = 0;
    uint64_t last_wb = 0;
    uint64_t tail_store = 0;
    uint64_t tail_wb = 0;
    bool tail_sampled = false;
    uint64_t final_store = 0;
    uint64_t final_wb = 0;

    for (std::size_t c = 0; c < kCycles; ++c) {
        sim.tick(1);
        const uint64_t sc = static_cast<uint64_t>(sim.get_port_value(io.store_count));
        const uint64_t wc = static_cast<uint64_t>(sim.get_port_value(io.writeback_count));
        // A5：单调不减
        if (sc < last_store || wc < last_wb) {
            monotonic = false;
        }
        last_store = sc;
        last_wb = wc;
        // A6：尾段采样
        if (!tail_sampled && c + 1 >= kTailStart) {
            tail_store = sc;
            tail_wb = wc;
            tail_sampled = true;
        }
        final_store = sc;
        final_wb = wc;
    }

    // ── A3 / A4：手推终值 ──
    ck.check(final_store == kExpectedStoreCount,
             "A3 store_count == 1（1 条 sw · 手推）");
    ck.check(final_wb == kExpectedWritebackCount,
             "A4 writeback_count == 3（x1/x2/x3 · 手推）");

    // ── A5：单调不减 ──
    ck.check(monotonic, "A5 两计数单调不减");

    // ── A6：尾段不再增长（jal 自环已稳定）──
    ck.check(tail_sampled && final_store == tail_store && final_wb == tail_wb,
             "A6 尾段计数不再增长");

    std::cout << "RESULT: checks=" << ck.checks() << " fails=" << ck.fails() << "\n";
    return ck.fails() == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char **argv) {
    using namespace ch;

    // ── 模式判定：位置无关剥离两个模式标记（库层共用实现）──
    std::vector<std::string> rest;
    rest.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        rest.emplace_back(argv[i]);
    }
    std::vector<std::string> mode_flags;
    rest = rtl_emit::strip_mode_flags(rest, {"--emit-rtl", "--check-products"}, &mode_flags);
    const bool emit_mode =
        std::find(mode_flags.begin(), mode_flags.end(), "--emit-rtl") != mode_flags.end();
    const bool check_mode =
        std::find(mode_flags.begin(), mode_flags.end(), "--check-products") != mode_flags.end();

    // ── 发射 / 自检模式 ──
    if (emit_mode || check_mode) {
        std::string out_dir;
        std::string error;
        if (!rtl_emit::require_out_dir(rest, &out_dir, &error)) {
            std::cerr << "FATAL: --emit-rtl / --check-products " << error << "\n";
            return 2;
        }
        // ⚠️ 统一加 `--hierarchical`：本设计有 2 处 `ch_layer` ⇒ 产层次化产物。
        bool has_hier = false;
        for (const auto &a : rest) {
            if (a == "--hierarchical") has_hier = true;
        }
        if (!has_hier) rest.emplace_back("--hierarchical");

        const int rc = rtl_emit::run_emit_cli(rest, make_emit_design);
        if (rc != 0 || !check_mode) return rc;
        return rtl_emit::check_products(make_spec(out_dir), make_hooks(), make_emit_design);
    }

    // ── 默认模式 fail-closed：收到任何未知参数 ⇒ exit 2 ──
    if (!rest.empty()) {
        std::cerr << "FATAL: 未知参数（默认模式只接受无参）：" << rest[0] << "\n";
        return 2;
    }

    return run_verification();
}
