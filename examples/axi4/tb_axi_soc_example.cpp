/**
 * AXI4-SoC Example —— **激励驱动 + 发射入口**（`example-file-boundary-2026-11` · B6 + F2）
 *
 * ⚠️ **设计定义在 `axi_soc_example_top.h`**（`AxiSocTop` + `AxiLiteTestMaster` + **4 处
 *    `ch::ch_layer<T>`**）；本文件三个模式：
 *      · **默认** = 激励 + 断言 + `RESULT:` 协议行
 *      · **`--emit-rtl <dir>`** = 发射（**恒** `--hierarchical`）
 *      · **`--check-products`** = 发射 + 4 项产品级自检
 *
 * ⚠️ **层次化的真正决定因素 = 设计里的 `ch::ch_layer<T>` 用法**（2026-11-18 实测）：
 *    · 本设计有 **4 处** `ch_layer` ⇒ 层次化产物 = **5 module**
 *    · 无 `ch_layer` 的设计（如 `AxiLiteTop`）加 `--hierarchical` 产 **1 module**，
 *      且与扁平产物**逐字节相同** ⇒ 对它是 **no-op**
 *    ⇒ ⇒ 形态由**设计**决定，不由发射开关决定 ⇒ 可以统一加 `--hierarchical`。
 *
 * ⚠️ **验证语义的单一载体 = `.verif`**（ADR-074 D1 · R13 闭口）：
 *    `examples/axi4/tb_axi_soc_example.verif` ⇒ ⇒ 默认模式（IR 解释器）与生成的
 *    `tb_axi_soc_example.sv`（Verilog TB）**由同一份验证源驱动**。
 *    · UART 8N1 帧的**起点**由 `WAIT uart_tx low <max>` 锚定（ADR-074 §7.3 D3′ 受裁用法：
 *      等待的目标电平在入口处**尚未成立** ⇒ 等的是**跃迁沿**）
 *    · 起点锚定后各位中点 = **字面量偏移**（bit 周期 16 拍）⇒ ⇒ 不需要数组 / 变长偏移
 *      （那正是 ADR-074 §7.4 选项 E 被否决的机制）
 *
 * ⚠️ **本文件只留「值」**（蓝图 §4.3.1 · F2）：规则侧（`product_modules` ·
 *    `tamper_hier` · `check_products` 的判定与编排）**在库层**
 *    （`ch::rtl_emit` · `include/rtl_emit/product_check.h`）⇒ ⇒ 本文件**不得**
 *    再出现第二份判定/编排实现。
 *    · **值**（本文件）：期望 module 集 · 期望顶层端口数 · 激励面 · 观测面 ·
 *      变异目标 · lint 的 `--top-module` 名 · 最小变化观测量阈值
 *    · **规则**（库层）：4 项判据 + 编排 + 结论行 + 退出码
 */

#include "ch.hpp"
#include "axi_soc_example_top.h"
#include "component.h"
#include "device.h"
#include "hierarchy_equivalence.h"
#include "hierarchy_verilog.h"
#include "rtl_emit/cli.h"
#include "rtl_emit/product_check.h"
#include "rtl_emit/verif.h"
#include "simulator.h"

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

/// 验证源路径（相对仓根 · CMake 注入 `CPPHDL_SOURCE_DIR` ⇒ 三个面的 CWD 各异，
/// 相对路径必失败 ⇒ ⇒ 与 Lite 侧同款注入）。
constexpr const char *kVerifRelPath = "/examples/axi4/tb_axi_soc_example.verif";

namespace {

// ── 值侧 ①：冻结的产物 module 期望集 ─────────────────────────────────────

/// ⚠️ **冻结的产物 module 期望集**（ADR-073 T1 · **手推** ⇒ 禁从成层声明处派生 ——
/// 后者会让「漏给某个 IP 加成层声明」同时改变期望 ⇒ 判据**恒绿**）。
/// ⇒ 与 `.rddf/checks/soc-rtl-expectations.md` 登记的集合 MUST 一致（T4/T6 核对）。
const std::vector<std::string> kExpectedModules = {
    "AxiLiteTestMaster", "AxiLiteMatrix2x2_32_32_", "AxiLiteGpio_8_",
    "AxiLiteUart_16_", "top",
};

/// ⚠️ **顶层端口数期望值** = 框架 2（clock/reset）+ 根自有 `__io` 11（7 in + 4 out）
/// ⇒ ADR-073 J-1 / AC3。
/// ⚠️ 2026-11-21：根自有 `__io` 由 12（8 in + 4 out）→ 11（7 in + 4 out）—— 删掉
///   零消费的 `timer_irq` 占位端口（台账 id=27 · ADR-074 §7.7 裁决 = 删）。
constexpr std::size_t kExpectedTopPorts = 13;

/// ⚠️ **最小变化观测量阈值**（ADR-073 J-4）—— MUST 有 ≥2 个随激励变化的观测量
/// ⇒ ⇒ **阈值是值**（⇒ 留 example；库层 MUST NOT 内置）。
constexpr std::size_t kMinVaryingObservations = 2;

// ── 规则侧的**平台胶水**：verilator 发现 + 执行 ───────────────────────────
//   ⚠️ 蓝图 §4.3.1：`find_verilator` 属「环境依赖 ⇒ 不下沉」；库层只判「退出码 == 0」。

/// 找 verilator（照 `scripts/verify_examples_rtl_sim.sh:86-94` 的口径）
std::string find_verilator() {
    if (const char *env = std::getenv("VERILATOR"); env && *env) return env;
    const auto usable = [](const std::string &p) {
        return !p.empty() && std::filesystem::exists(p);
    };
    // ① **编译期注入的源根**（与 CWD 无关）—— 修 2026-11-18 查实的缺陷：
    //    `build/verilator-install/bin/verilator` 是 **CWD 相对** ⇒ 从 `build/` 或
    //    `build/tests/` 跑时必然找不到 ⇒ CHECK 3 静默 SKIP（lint 从未被验）。
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

/// 跑 `verilator --lint-only --top-module <top> <verilog>`（**argv 式** ⇒ 无 shell 拼串）
bool run_lint(const std::string &verilator, const std::string &top_module,
              const std::string &verilog_path) {
#if defined(_WIN32)
    // ⚠️ verilator 后端在本仓是 **Linux-only**（见 `include/core/verilator_backend.h`）
    //   ⇒ ⇒ 无 POSIX 进程 API ⇒ ⇒ **按失败计**（fail-closed），不得静默跳过。
    (void)verilator;
    (void)top_module;
    (void)verilog_path;
    return false;
#else
    const pid_t pid = ::fork();
    if (pid == 0) {
        ::execl(verilator.c_str(), verilator.c_str(), "--lint-only", "--top-module",
                top_module.c_str(), verilog_path.c_str(), static_cast<char *>(nullptr));
        ::_exit(127); // execl 失败
    }
    if (pid < 0) {
        return false;
    }
    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) {
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

// ── 设计工厂（与 `run_emit_cli` 的 `DesignFactory` 同型）────────────────────

/// 发射 / 自检**共用**的根工厂 ⇒ ⇒ 根名**单一真相源**（设计头的常量）
std::unique_ptr<ch::Component> make_emit_design(ch::core::context *) {
    return std::make_unique<axi4::example::AxiSocTop>(nullptr, axi4::example::kSocRootName);
}

// ── 值侧 ②：双形态逐拍等价（J-4）的**设计专属**激励面 ─────────────────────

/// 顶层输入（受驱动）—— 顺序 MUST 与 `make_stimulus()` 每行的取值顺序**逐字一致**
///
/// ⚠️ **输入面 MUST 覆盖根的全部 `ch_in`**（7 个）—— 缺一个 ⇒ 该端口在等价性比对里
///   读到**未定义值** ⇒ ⇒ 「双形态逐拍等价」实际只覆盖了部分输入面。
///   （`uart_rx` 恒 1 = 空闲高）
/// ⚠️ 2026-11-21：根自有 `ch_in` 由 8 个 → 7 个（删掉零消费的 `timer_irq` 占位端口
///   · 台账 id=27 · ADR-074 §7.7 裁决 = 删）。
std::vector<ch::hier_eq::DrivenInput> make_inputs(axi4::example::AxiSocTop &root) {
    std::vector<ch::hier_eq::DrivenInput> in;
    // ⚠️ MUST 用 `set_input_value`（`<<=` 会把端口变成内部驱动 ⇒ 假分歧）
    in.push_back({"top.rst_n", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().rst_n, v & 1);
                  }});
    in.push_back({"top.start", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().start, v & 1);
                  }});
    in.push_back({"top.write", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().write, v & 1);
                  }});
    in.push_back({"top.addr", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().addr, v);
                  }});
    in.push_back({"top.wdata", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().wdata, v);
                  }});
    in.push_back({"top.gpio_in", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().gpio_in, v & 0xFF);
                  }});
    in.push_back({"top.uart_rx", [&root](ch::Simulator &sim, std::uint64_t v) {
                      sim.set_input_value(root.io().uart_rx, v & 1);
                  }});
    return in;
}

/// 顶层观测面（**只能**取根的 `ch_out` ⇒ 子组件的 `ch_out` 在层次化顶层不可观测）
std::vector<ch::hier_eq::Observation> make_observations(axi4::example::AxiSocTop &root) {
    std::vector<ch::hier_eq::Observation> obs;
    obs.push_back(ch::hier_eq::make_observation(
        "top.rdata", root.io().rdata, [&root](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(root.io().rdata));
        }));
    obs.push_back(ch::hier_eq::make_observation(
        "top.gpio_out", root.io().gpio_out, [&root](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(root.io().gpio_out));
        }));
    // ⚠️ `uart_tx` 恒为 1（无帧 ⇒ 空闲高）⇒ ⇒ 它**不随激励变化** ⇒ 只作「同值对照」
    obs.push_back(ch::hier_eq::make_observation(
        "top.uart_tx", root.io().uart_tx, [&root](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(root.io().uart_tx));
        }));
    return obs;
}

/// ⚠️ 顺序 MUST 与 `make_inputs()` 一致：
///   rst_n · start · write · addr · wdata · gpio_in · uart_rx
std::vector<std::vector<std::uint64_t>> make_stimulus() {
    const std::uint64_t gpio_a = 0x5A;
    const std::uint64_t gpio_b = 0xA5;
    const std::uint64_t gpio_data_out = 0x000000A5;
    const std::uint64_t gpio_data_in = 0x00000004;
    const std::uint64_t idle = 0;
    const std::uint64_t uart_idle = 1;  // UART 空闲高（无帧）
    return {
        /* 0  */ {1, 1, 1, 0x00000000, gpio_data_out, gpio_a, uart_idle},  // 写事务：发起
        /* 1  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a, uart_idle},  // 保持 payload
        /* 2  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a, uart_idle},
        /* 3  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a, uart_idle},
        /* 4  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a, uart_idle},  // 等写完成 ⇒ gpio_out 变
        /* 5  */ {1, 1, 0, gpio_data_in, idle, gpio_a, uart_idle},         // 读事务 #1：发起
        /* 6  */ {1, 0, 0, gpio_data_in, idle, gpio_a, uart_idle},         // 保持 addr
        /* 7  */ {1, 0, 0, gpio_data_in, idle, gpio_a, uart_idle},
        /* 8  */ {1, 0, 0, gpio_data_in, idle, gpio_a, uart_idle},         // 等读完成 ⇒ rdata = 0x5A
        /* 9  */ {1, 0, 0, gpio_data_in, idle, gpio_b, uart_idle},         // 改 gpio_in ⇒ 让 rdata 能再变
        /* 10 */ {1, 1, 0, gpio_data_in, idle, gpio_b, uart_idle},         // 读事务 #2：发起
        /* 11 */ {1, 0, 0, gpio_data_in, idle, gpio_b, uart_idle},         // 保持 addr
        /* 12 */ {1, 0, 0, gpio_data_in, idle, gpio_b, uart_idle},
        /* 13 */ {1, 0, 0, gpio_data_in, idle, gpio_b, uart_idle},         // 等读完成 ⇒ rdata = 0xA5
    };
}

/// 把值侧装进库层的判据输入（**值** ⇒ 留本文件；**规则** ⇒ 在库层）
ch::rtl_emit::ProductCheckSpec make_spec(const std::string &out_dir) {
    ch::rtl_emit::ProductCheckSpec spec;
    // ⚠️ 产物路径 = CLI 的**默认落点**（`include/rtl_emit/cli.h` 的用法行：`--out <dir>`
    //   ⇒ `<dir>/top.v`）⇒ ⇒ 与发射出来的文件**逐字同一路径**。
    spec.verilog_path = out_dir + "/top.v";
    spec.work_dir = out_dir;
    spec.expected_modules = kExpectedModules;
    spec.expected_top_ports = kExpectedTopPorts;
    spec.min_varying_observations = kMinVaryingObservations;
    // ⚠️ `--top-module` 名 = **产物**的顶层 module 名（`HierarchyEmitOptions` 的默认）
    //   ⇒ ⇒ **不等于**设计根名 ⇒ ⇒ 取库层默认值（**不写字面量**）。
    spec.lint_top_module = ch::HierarchyEmitOptions{}.top_module_name;
    spec.lint = [](const std::string &verilog_path) {
        const std::string verilator = find_verilator();
        if (verilator.empty()) {
            return false; // ⚠️ fail-closed：找不到 linter ⇒ 按失败计（不得静默跳过）
        }
        return run_lint(verilator, ch::HierarchyEmitOptions{}.top_module_name, verilog_path);
    };
    return spec;
}

/// 值侧的三个激励面回调（**产品专属** ⇒ 留本文件）
ch::rtl_emit::ProductCheckHooks make_hooks() {
    ch::rtl_emit::ProductCheckHooks hooks;
    // ⚠️ `static_cast` 安全：库层用**本文件的工厂**建根 ⇒ 类型必为 `AxiSocTop`。
    hooks.make_inputs = [](ch::Component &root) {
        return make_inputs(static_cast<axi4::example::AxiSocTop &>(root));
    };
    hooks.make_observations = [](ch::Component &root) {
        return make_observations(static_cast<axi4::example::AxiSocTop &>(root));
    };
    hooks.make_stimulus = []() { return make_stimulus(); };
    return hooks;
}

} // namespace

int main(int argc, char **argv) {
    using namespace ch;

    // ── 0) 模式判定：**位置无关**剥离两个模式标记 ────────────────────────────
    //   ⚠️ 剥离与 `--out` 校验**走库层共用实现**（`rtl_emit/cli.h`）⇒ ⇒ 与 Lite 侧
    //   不会各写一份而漂移（2026-11-19：Lite 侧曾漏 `--out` 取值校验 ⇒ fail-open）。
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

    // ── 发射 / 自检模式 ────────────────────────────────────────────────────
    if (emit_mode || check_mode) {
        std::string out_dir;
        std::string error;
        if (!rtl_emit::require_out_dir(rest, &out_dir, &error)) {
            std::cerr << "FATAL: --emit-rtl / --check-products " << error << "\n";
            return 2;
        }

        // ⚠️ **统一加 `--hierarchical`**：对**有 `ch_layer` 的设计**产层次化（本设计 5 module），
        //    对**无 `ch_layer` 的设计**是 no-op（产物逐字节不变）⇒ 不必 per-design 区分。
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
        // ⚠️ **规则侧在库层**（`ch::rtl_emit::check_products`）⇒ ⇒ 本文件**不编排**判据。
        return rtl_emit::check_products(make_spec(out_dir), make_hooks(), make_emit_design);
    }

    // ── 默认模式 fail-closed：收到**任何**未知参数 ⇒ exit 2 ──────────────────
    if (!rest.empty()) {
        std::cerr << "FATAL: 未知参数（默认模式只接受无参）：" << rest[0] << "\n";
        return 2;
    }

    // ── 默认模式：**由 `.verif` 驱动**（ADR-074 D1 · R13 闭口）──────────────
    //   ⚠️ 同一份 `.verif` 也生成 `tb_axi_soc_example.sv` ⇒ ⇒ 两侧**同一语义载体**
    //   （此前是 C++ 侧 2 条 + 手写 `.sv` 14 条，连写入值都不同）。
    //   ⚠️ helper **不打印** `RESULT:` 行（结论行是**值**侧 ⇒ 由本 example 打印）。
    ch::ch_device<axi4::example::AxiSocTop> top_device;
    const std::string verif_path = std::string(CPPHDL_SOURCE_DIR) + kVerifRelPath;
    const auto r = rtl_emit::run_verif_file(verif_path, top_device.instance(),
                                            top_device.context());
    for (const auto &line : r.run.failure_lines) {
        std::cerr << line << "\n";
    }
    if (!r.ok) {
        std::cerr << "FATAL: " << r.error << "\n";
        return 1;
    }
    std::cout << "RESULT: checks=" << r.run.checks << " fails=" << r.run.fails << "\n";
    return r.run.fails == 0 ? 0 : 1;
}
