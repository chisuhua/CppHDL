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

#include "hierarchy_equivalence.h"
#include "hierarchy_verilog.h"
#include "rtl_emit/manifest.h"

#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <iterator>
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

/// 顶层输入（受驱动）—— 顺序 MUST 与 `stimulus` 每行的取值顺序**逐字一致**
std::vector<ch::hier_eq::DrivenInput> make_inputs(AxiSocTop &root) {
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
    return in;
}

/// 顶层观测面（**只能**取根的 `ch_out` ⇒ 子组件的 `ch_out` 在层次化顶层不可观测）
std::vector<ch::hier_eq::Observation> make_observations(AxiSocTop &root) {
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

/// 逐拍激励（**≥10 拍**且**值不同** ⇒ ⇒ 否则 `compare` 判 `no-varying-signal`）
/// ⚠️ 顺序 MUST 与 `make_inputs` 的顺序一致：rst_n · start · write · addr · wdata · gpio_in
std::vector<std::vector<std::uint64_t>> make_stimulus() {
    const std::uint64_t gpio_a = 0x5A;
    const std::uint64_t gpio_b = 0xA5;
    const std::uint64_t gpio_data_out = 0x000000A5;
    const std::uint64_t gpio_data_in = 0x00000004;
    const std::uint64_t idle = 0;
    // ⚠️⚠️ **AXI payload MUST 保持稳定**（实测踩到）：master 的 `awaddr` / `wdata_axi`
    //   是**组合**跟随顶层 `addr` / `wdata` 的 ⇒ ⇒ 激励若在下一行把 `wdata` 清零
    //   ⇒ ⇒ **W 通道送出去的是 0 而不是 0xA5** ⇒ ⇒ `gpio_out` 恒 0（且**不报错**）。
    //   ⇒ ⇒ 事务期间 `addr` / `wdata` MUST 原样保持（这正是仓内 `tb_axi4_lite_example.sv`
    //   的 `send_aw` / `send_w` 分开送、且 `valid && !ready` 期间 payload 不变 的同型规则）。
    return {
        /* 0  */ {1, 1, 1, 0x00000000, gpio_data_out, gpio_a},  // 写事务：发起
        /* 1  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a},  // 保持 payload
        /* 2  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a},
        /* 3  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a},
        /* 4  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a},  // 等写完成 ⇒ gpio_out 变
        /* 5  */ {1, 1, 0, gpio_data_in, idle, gpio_a},           // 读事务 #1：发起
        /* 6  */ {1, 0, 0, gpio_data_in, idle, gpio_a},           // 保持 addr
        /* 7  */ {1, 0, 0, gpio_data_in, idle, gpio_a},
        /* 8  */ {1, 0, 0, gpio_data_in, idle, gpio_a},           // 等读完成 ⇒ rdata = 0x5A
        /* 9  */ {1, 0, 0, gpio_data_in, idle, gpio_b},           // 改 gpio_in ⇒ 让 rdata 能再变
        /* 10 */ {1, 1, 0, gpio_data_in, idle, gpio_b},           // 读事务 #2：发起
        /* 11 */ {1, 0, 0, gpio_data_in, idle, gpio_b},           // 保持 addr
        /* 12 */ {1, 0, 0, gpio_data_in, idle, gpio_b},
        /* 13 */ {1, 0, 0, gpio_data_in, idle, gpio_b},           // 等读完成 ⇒ rdata = 0xA5
    };
}


/// **变异自证**（ADR-073 V5）：把层次化产物里**驱动某个被观测端口**的 `assign`
/// **取反** ⇒ ⇒ 行为改变 ⇒ ⇒ `compare` MUST 判不等（⇒ 证明这条判据不是恒绿）。
///
/// ⚠️ **为什么不能直接取反顶层端口**：顶层端口名出现在**实例端口连接**的右侧
///   （`.child_port(top_port)`）⇒ ⇒ 把它取反 = **组合环** ⇒ ⇒ Verilator 直接报错
///   ⇒ ⇒ 那是「编译失败」而不是「行为差异」⇒ ⇒ 判别力更弱。
///   ⇒ ⇒ 必须取反**子 module 内驱动该端口的那条 assign**（`assign child_port = X;`
///   ⇒ ⇒ `assign child_port = ~X;`）⇒ ⇒ **真行为差异**。
///
/// ⚠️ **端口名与子 module 名全部由 `signal_map` 派生**（禁手写）⇒ ⇒ 与产物真相不脱钩。
/// @return 空串 = 篡改失败（⇒ 自证不成立 ⇒ 判据 MUST 判红）
std::string tamper_hier(const std::string &hier_path,
                        const std::vector<ch::SignalMapEntry> &signal_map,
                        const std::vector<ch::hier_eq::Observation> &observations,
                        const std::string &top_module) {
    // ① 被观测端口在**层次化顶层**的名字（**由 signal_map 派生** ⇒ 禁手写）
    std::string top_port;
    for (const auto &o : observations) {
        for (const auto &e : signal_map) {
            if (e.node_id == o.node_id && !e.hier_top_port_name.empty()) {
                top_port = e.hier_top_port_name;
                break;
            }
        }
        if (!top_port.empty()) break;
    }
    if (top_port.empty()) return {};
    std::string child_port;

    // ② 读产物 → 从**实例端口连接**反查子 module 的端口名
    //    （连接形如 `.child_port(top_port)` ⇒ ⇒ child_port = '.' 与 '(' 之间的标识符）
    std::ifstream in(hier_path);
    if (!in) return {};
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    const std::string conn = "(" + top_port + ")";
    const auto conn_pos = text.find(conn);
    if (conn_pos == std::string::npos) return {};
    const auto dot_pos = text.rfind('.', conn_pos);
    if (dot_pos == std::string::npos) return {};
    const auto name_begin = dot_pos + 1;
    if (name_begin >= conn_pos) return {};
    child_port = text.substr(name_begin, conn_pos - name_begin);
    if (child_port.empty()) return {};

    // ③ 找到 `assign <child_port> = <rhs>;` → 取反 rhs（**真行为差异**）
    const std::string key = "assign " + child_port + " = ";
    const auto pos = text.find(key);
    if (pos == std::string::npos) return {};
    const auto rhs_begin = pos + key.size();
    const auto rhs_end = text.find(';', rhs_begin);
    if (rhs_end == std::string::npos) return {};
    const std::string rhs = text.substr(rhs_begin, rhs_end - rhs_begin);
    if (rhs.empty() || rhs.find('(') != std::string::npos) return {};   // 只改**简单** rhs
    text.replace(rhs_begin, rhs.size(), "~(" + rhs + ")");

    const std::string out_path = hier_path + ".tampered";
    std::ofstream out(out_path);
    if (!out) return {};
    out << text;
    out.close();
    return out_path;
}

/// 产物级自检（4 项）—— 任一失败 ⇒ 返回非零
int check_products(AxiSocTop &root, ch::core::context *ctx,
                   const std::string &out_dir, const std::string &verilog_path,
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

    // ---- ④ 双形态逐拍等价（J-4）----
    // ⚠️ **A 侧固定是「扁平产物」**（`hier_eq::compare` 的语义）⇒ ⇒ 本判据 = 「扁平 ≡ 层次化」
    //   （层次化侧由 `emit` 按 `RespectDeclarations` 生成 ⇒ 4 个 IP 成层）。
    // ⚠️ `ports` **MUST 由 `emit` 派生**（禁手写 ⇒ 否则与 emitter 真相脱钩）。
    {
        const std::string eq_dir = out_dir + "/eq";
        std::error_code fec;
        std::filesystem::create_directories(eq_dir, fec);
        auto products = ch::hier_eq::emit(root, eq_dir,
                                          ch::Granularity::RespectDeclarations);
        if (!products.ok) {
            std::cout << "CHECK 4 双形态等价: FAIL (emit 失败: " << products.error << ")\n";
            ++failures;
        } else {
            const auto inputs = make_inputs(root);
            const auto observations = make_observations(root);
            const auto stimulus = make_stimulus();
            const auto res = ch::hier_eq::compare(ctx, eq_dir, products.hier_path,
                                                  products.ports, inputs, observations,
                                                  stimulus);
            // ⚠️ **变化量计数**（ADR-073 J-4：「MUST 有 ≥2 个随激励变化的观测量」）：
            //   `uart_tx` 无帧时恒 1 ⇒ ⇒ 它**不**贡献变化量 ⇒ ⇒ 只认 `rdata` + `gpio_out`。
            std::size_t varying = 0;
            if (!res.flat_trace.empty()) {
                for (std::size_t k = 0; k < observations.size(); ++k) {
                    bool varies = false;
                    for (std::size_t c = 1; c < res.flat_trace.size(); ++c) {
                        if (res.flat_trace[c][k] != res.flat_trace[0][k]) {
                            varies = true;
                            break;
                        }
                    }
                    if (varies) ++varying;
                }
            }
            // ⚠️ **变异自证**（ADR-073 V5）：篡改层次化产物 ⇒ ⇒ MUST 判不等
            //   ⇒ ⇒ 否则这条判据可能是**恒绿**（等价成立也可能只是「两边都没动」）。
            bool tamper_detected = false;
            std::string tamper_msg;
            if (res.ok && res.equal) {
                const std::string tampered =
                    tamper_hier(products.hier_path, products.signal_map, observations,
                                top_module);
                if (tampered.empty()) {
                    tamper_msg = "篡改失败（未找到可取反的观测端口连接）";
                } else {
                    const std::string tamper_dir = eq_dir + "/tampered";
                    std::error_code tec;
                    std::filesystem::create_directories(tamper_dir, tec);
                    const auto res_t = ch::hier_eq::compare(ctx, tamper_dir, tampered,
                                                            products.ports, inputs,
                                                            observations, stimulus);
                    tamper_detected = res_t.ok && !res_t.equal;
                    if (!res_t.ok) {
                        tamper_msg = "篡改后 compare 报错: " + res_t.error;
                    } else if (tamper_detected) {
                        tamper_msg = "篡改被抓住（first_mismatch=" + res_t.first_mismatch + ")";
                    } else {
                        tamper_msg = "篡改**未**被抓住 ⇒ ⇒ 判据恒绿";
                    }
                }
            } else {
                tamper_msg = "主比对未通过 ⇒ 未做自证";
            }
            const bool ok = res.ok && res.equal && (varying >= 2) && tamper_detected;
            std::cout << "CHECK 4 双形态等价: " << (ok ? "PASS" : "FAIL")
                      << " (ok=" << (res.ok ? 1 : 0) << " equal=" << (res.equal ? 1 : 0)
                      << " 变化量=" << varying << "/"
                      << observations.size() << " 自证=" << (tamper_detected ? "PASS" : "FAIL")
                      << (tamper_msg.empty() ? "" : " [" + tamper_msg + "]")
                      << (res.error.empty() ? "" : " err=" + res.error)
                      << ")\n";
            if (!ok) ++failures;
        }
    }

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
        return check_products(dev.instance(), dev.context(), args.out_dir, verilog_path,
                              emitted, manifest,
                              opts.top_module_name);
    }
    return 0;
}
