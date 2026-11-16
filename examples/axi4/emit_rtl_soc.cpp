// examples/axi4/emit_rtl_soc.cpp
//
// **SoC 层 RTL 发射入口**（ADR-073 D1/D2 · `soc-rtl-product-and-tb-2026-11` 的 **T2**）
//
// ⚠️ **为什么自己解析参数而不复用 `run_emit_cli`**：那是个**扁平**发射器
//   （`src/rtl_emit/cli.cpp:177-187` ⇒ `toVerilog(path, ctx, opts)`）⇒ ⇒ SoC 必须走
//   **层次化**入口 `toVerilogHierarchical(path, root, opts)` ⇒ ⇒ **根由显式传入**
//   （扁平入口拿不到 root —— 见 ADR-072 D1-a / v2.1 的「未被交付 + 推导不可靠」）。
//
// ⚠️ **为什么 `#include "axi_soc_example.cpp"` 而不是抽头文件**：照
//   `examples/axi4/emit_rtl.cpp:22-24` 的**既有先例** ⇒ ⇒ 复用**同一份** `AxiSocTop`
//   定义 ⇒ ⇒ example 源码**零结构改动**，且**不产生第二份定义**（⇒ 无双权威）。
//   ⚠️ `#define main` 只在本 TU 内屏蔽 example 的 `main` ⇒ 它的演示入口照常编译。
//
// ⚠️ **退出码（fail-closed）**：0 = 成功 · 1 = 发射/写盘失败 · 2 = 用法错误
//   ⇒ ⇒ **`result.ok == false` MUST 映射为非零**（否则护栏读到残留文件 ⇒ 假绿）。
//
// 用法：
//   build/examples/axi4/emit_rtl_axi_soc --out <dir> [--emit-hygiene]

#define main cpphdl_example_main_disabled
#include "axi_soc_example.cpp"
#undef main

#include "hierarchy_verilog.h"
#include "rtl_emit/manifest.h"

#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {

/// ⚠️ **冻结的产物 module 期望集**（ADR-073 T1 · **手推** ⇒ 禁从成层声明处派生 ——
/// 后者会让「漏给某个 IP 加成层声明」同时改变期望 ⇒ 判据**恒绿**）。
/// ⇒ 与 `.rddf/checks/soc-rtl-expectations.md` 登记的集合 MUST 一致（T4/T6 核对）。
const std::vector<std::string> kExpectedModules = {
    "AxiLiteTestMaster", "AxiLiteMatrix2x2_32_32_", "AxiLiteGpio_8_",
    "AxiLiteUart_16_", "top",
};

/// ⚠️ **顶层端口数期望值** = 框架 2（clock/reset）+ 根自有 `__io` 12（8 in + 4 out）
/// ⇒ ADR-073 J-1 / AC3。
constexpr std::size_t kExpectedTopPorts = 14;

struct Args {
    std::string out_dir = "rtl_out";
    bool emit_hygiene = false;
    /// 有诊断时仍返回 0（默认：诊断 ⇒ 返回 1）—— 与扁平 CLI `run_emit_cli` **同口径**
    bool allow_diagnostics = false;
    /// **产物级自检**（4 项逐条输出；任一失败 ⇒ 非零退出）
    bool check_products = false;
};

/// 返回 0 = ok · 1 = 用法错误
int parse_args(int argc, char **argv, Args &out) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--out") {
            if (i + 1 >= argc) {
                std::cerr << "::error::--out 缺参数\n";
                return 1;
            }
            out.out_dir = argv[++i];
        } else if (a == "--emit-hygiene") {
            out.emit_hygiene = true;
        } else if (a == "--allow-diagnostics") {
            out.allow_diagnostics = true;
        } else if (a == "--check-products") {
            out.check_products = true;
        } else if (a == "--help" || a == "-h") {
            std::cout << "用法: emit_rtl_axi_soc --out <dir> [--emit-hygiene]"
                         " [--allow-diagnostics] [--check-products]\n";
            return 1;
        } else {
            std::cerr << "::error::未知参数: " << a << "\n";
            return 1;
        }
    }
    return 0;
}


/// 从产物里抽出 `module <name>` 的集合（**从产物派生** ⇒ 禁手写）
std::set<std::string> product_modules(const std::string &verilog_path) {
    std::set<std::string> mods;
    std::ifstream f(verilog_path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("module ", 0) != 0) continue;
        // ⚠️ **搜索从 7 开始**（`"module "` 之后的第一个分隔符）⇒ ⇒ 否则会命中
        //   "module" 与名字之间的空格 ⇒ 取到空/越界的名字
        const auto sp = line.find_first_of(" \t", 7);
        if (sp == std::string::npos) continue;
        mods.insert(line.substr(7, sp - 7));
    }
    return mods;
}

/// 找 verilator（照 `scripts/verify_examples_rtl_sim.sh:77-84` 的口径）
std::string find_verilator() {
    if (const char *env = std::getenv("VERILATOR"); env && *env) return env;
    const std::string local = "build/verilator-install/bin/verilator";
    if (std::filesystem::exists(local) && ::access(local.c_str(), X_OK) == 0) {
        return local;
    }
    if (const char *path = std::getenv("PATH")) {
        std::string p(path);
        std::size_t pos = 0;
        while ((pos = p.find(':', pos)) != std::string::npos) {
            const std::string cand = p.substr(0, pos) + "/verilator";
            if (std::filesystem::exists(cand) &&
                ::access(cand.c_str(), X_OK) == 0) {
                return cand;
            }
            pos += 1;
        }
    }
    return {};
}

/// 产物级自检（4 项）—— 任一失败 ⇒ 返回非零
int check_products(const std::string &verilog_path,
                   const ch::HierarchyEmitResult &emitted,
                   const ch::rtl_emit::RtlManifest &manifest,
                   const std::string &top_module) {
    int failures = 0;

    // ---- ① 产物 module 集 == 冻结期望集 ----
    const auto mods = product_modules(verilog_path);
    const std::set<std::string> expected(kExpectedModules.begin(),
                                         kExpectedModules.end());
    const bool modules_ok = (mods == expected);
    std::cout << "CHECK 1 module 集: " << (modules_ok ? "PASS" : "FAIL")
              << " (期望 " << expected.size() << " 个 · 实际 " << mods.size()
              << " 个)\n";
    if (!modules_ok) {
        ++failures;
        for (const auto &m : mods) {
            if (!expected.count(m)) std::cerr << "  ::error::多出 module: " << m << "\n";
        }
        for (const auto &m : expected) {
            if (!mods.count(m)) std::cerr << "  ::error::缺 module: " << m << "\n";
        }
    }

    // ---- ② 顶层端口数 + 零后代冒泡 ----
    // ⚠️ **判据 = 「零后代冒泡」，不是「零 Bubbled」**（ADR-073 AC1/AC3 的字面已订正 ——
    //   根自有的顶层输入本就被标为 `Bubbled`，见 feedback-20261116-001）：
    //   顶层端口里的 `Bubbled` **MUST 全部是根自有的顶层输入**（owner == 根）
    //   ⇒ ⇒ 「owner != 根的未驱动端口」MUST == 0（那才是后代悬空被冒泡）。
    std::size_t top_ports = 0;
    for (const auto &p : emitted.port_provenance) {
        if (p.module == top_module) ++top_ports;
    }
    std::size_t descendant_undriven = 0;
    for (const auto &e : manifest.ports) {
        if (e.driven) continue;
        if (e.owner != top_module && !e.owner.empty()) ++descendant_undriven;
    }
    const bool ports_ok = (top_ports == kExpectedTopPorts) && (descendant_undriven == 0);
    std::cout << "CHECK 2 顶层端口: " << (ports_ok ? "PASS" : "FAIL")
              << " (端口 " << top_ports << "/" << kExpectedTopPorts
              << " · 后代未驱动 " << descendant_undriven << ")\n";
    if (!ports_ok) ++failures;

    // ---- ③ lint 干净（verilator --lint-only --top-module top）----
    const std::string verilator = find_verilator();
    if (verilator.empty()) {
        std::cout << "CHECK 3 lint: SKIP (verilator 不可用 · **未验证**)\n";
    } else {
        const std::string cmd = verilator + " --lint-only --top-module " + top_module +
                                " " + verilog_path + " > /dev/null 2>&1";
        const int rc = std::system(cmd.c_str());
        const bool lint_ok = (rc == 0);
        std::cout << "CHECK 3 lint: " << (lint_ok ? "PASS" : "FAIL")
                  << " (verilator --lint-only --top-module " << top_module << ")\n";
        if (!lint_ok) ++failures;
    }

    // ---- ④ 双形态逐拍等价 —— 由 **T7** 落地（`ch::hier_eq::emit` + `compare`）----
    std::cout << "CHECK 4 双形态等价: TODO (T7)\n";

    std::cout << (failures == 0 ? "PRODUCTS-CHECK: PASS\n"
                                : "PRODUCTS-CHECK: FAIL (" + std::to_string(failures) +
                                       " 项)\n");
    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char **argv) {
    Args args;
    if (parse_args(argc, argv, args) != 0) {
        return 2;
    }

    ch::ch_device<AxiSocTop> dev;

    ch::HierarchyEmitOptions opts;
    opts.emit_hygiene = args.emit_hygiene;

    std::error_code ec;
    std::filesystem::create_directories(args.out_dir, ec);
    if (ec) {
        std::cerr << "::error::无法创建输出目录 " << args.out_dir << ": " << ec.message() << "\n";
        return 1;
    }

    const std::string verilog_path = args.out_dir + "/top.v";
    const auto emitted = ch::toVerilogHierarchical(verilog_path, dev.instance(), opts);

    // ⚠️ **fail-closed**：发射失败 ⇒ 非零退出 ⇒ **不留残留文件**给护栏误读
    if (!emitted.ok) {
        std::cerr << "::error::发射失败: " << emitted.error << "\n";
        return 1;
    }

    // manifest（机器可读的端口清单）
    const auto manifest =
        ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    {
        std::ofstream mf(args.out_dir + "/manifest.json");
        if (!mf) {
            std::cerr << "::error::无法写 manifest: " << args.out_dir + "/manifest.json\n";
            return 1;
        }
        ch::rtl_emit::write_rtl_manifest_json(manifest, mf);
    }

    // ⚠️ **fail-closed（同扁平 CLI `src/rtl_emit/cli.cpp:217-224` 的口径）**：
    //   manifest 的**诊断**（悬空输入 / 无归属 / 不可观测）⇒ 退出码 1
    //   ⇒ ⇒ 绝不静默通过（--allow-diagnostics 可显式放行）。
    // ⚠️ **emitter 的 `warnings` 不是诊断** —— 「synthesized cross-module」
    //   （ADR-060 §2.4.1 的**父侧落点**语义，ADR-070 D1）是层次化发射的**常态**
    //   ⇒ ⇒ 只作信息打印，**不**影响退出码。
    for (const auto &w : emitted.warnings) {
        std::cerr << "[warn] " << w << "\n";
    }
    if (!manifest.diagnostics.empty() && !args.allow_diagnostics) {
        for (const auto &d : manifest.diagnostics) {
            std::cerr << "::error::诊断[" << d.kind << "] " << d.port << " @ "
                      << d.owner << ": " << d.detail << "\n";
        }
        std::cerr << "::error::存在 " << manifest.diagnostics.size()
                  << " 条诊断 ⇒ 退出码 1（fail-closed；--allow-diagnostics 可放行）\n";
        return 1;
    }

    std::cout << "OK: 产物 " << emitted.files.size() << " 个 · manifest 端口 "
              << manifest.ports.size() << " 条 · 诊断 " << manifest.diagnostics.size()
              << " 条 · 顶层 module " << opts.top_module_name << "\n";

    if (args.check_products) {
        return check_products(verilog_path, emitted, manifest,
                              opts.top_module_name);
    }
    return 0;
}
