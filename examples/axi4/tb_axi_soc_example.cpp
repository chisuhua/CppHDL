/**
 * AXI4-SoC Example —— **激励驱动 + 发射入口 + 产品自检**（`example-file-boundary-2026-11` · B6）
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
 * ⚠️ **激励是手写过渡态**（蓝图 §7 R13）：UART 8N1 帧解码超出 `.verif` 封闭原语集
 *    （ADR-074 D2）⇒ 扩展该原语集是**独立立项**。
 *
 * ⚠️ **checker 的值 + 规则暂时同处本文件**（2026-11-18 · 用户裁决选 B）：
 *    规则侧（`product_modules` / `tamper_hier` / `check_products` 的判定与编排）
 *    将由 **F2** 下沉 `include/rtl_emit/`；之后本文件只留值。
 */

#include "ch.hpp"
#include "axi_soc_example_top.h"
#include "component.h"
#include "device.h"
#include "hierarchy_equivalence.h"
#include "hierarchy_verilog.h"
#include "rtl_emit/cli.h"
#include "rtl_emit/manifest.h"
#include "simulator.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

/// 发射模式的根组件名（**MUST 与 `check_products` 的 owner 基准逐字一致**）
constexpr const char *kEmitRootName = "top";

/// 发射模式的 design factory（与 `run_emit_cli` 的 `DesignFactory` 同型）
std::unique_ptr<ch::Component> make_emit_design(ch::core::context *) {
    return std::make_unique<axi4::example::AxiSocTop>(nullptr, kEmitRootName);
}

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

/// 从产物里抽出 `module <name>` 的集合（**从产物派生** ⇒ 禁手写）
std::set<std::string> product_modules(const std::string &verilog_path) {
    std::set<std::string> mods;
    std::ifstream f(verilog_path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("module ", 0) != 0) continue;
        const auto sp = line.find_first_of(" \t", 7);
        if (sp == std::string::npos) continue;
        mods.insert(line.substr(7, sp - 7));
    }
    return mods;
}

/// 跑 `verilator --lint-only --top-module <top> <verilog>`（**argv 式** ⇒ 无 shell 拼串）
bool run_lint(const std::string &verilator, const std::string &top_module,
              const std::string &verilog_path) {
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
}

/// 找 verilator（照 `scripts/verify_examples_rtl_sim.sh:86-94` 的口径）
std::string find_verilator() {
    if (const char *env = std::getenv("VERILATOR"); env && *env) return env;
    // ① **编译期注入的源根**（与 CWD 无关）—— 修 2026-11-18 查实的缺陷：
    //    `build/verilator-install/bin/verilator` 是 **CWD 相对** ⇒ 从 `build/` 或
    //    `build/tests/` 跑时必然找不到 ⇒ CHECK 3 静默 SKIP（lint 从未被验）。
    {
        const std::string src_local =
            std::string(CPPHDL_SOURCE_DIR) + "/build/verilator-install/bin/verilator";
        if (std::filesystem::exists(src_local) && ::access(src_local.c_str(), X_OK) == 0) {
            return src_local;
        }
    }
    const std::string local = "build/verilator-install/bin/verilator";
    if (std::filesystem::exists(local) && ::access(local.c_str(), X_OK) == 0) {
        return local;
    }
    if (const char *path = std::getenv("PATH")) {
        std::string p(path);
        std::size_t pos = 0;
        while ((pos = p.find(':', pos)) != std::string::npos) {
            const std::string cand = p.substr(0, pos) + "/verilator";
            if (std::filesystem::exists(cand) && ::access(cand.c_str(), X_OK) == 0) {
                return cand;
            }
            pos += 1;
        }
    }
    return {};
}

// ── 双形态逐拍等价（J-4）的**设计专属**激励面 ──────────────────────────────

/// 顶层输入（受驱动）—— 顺序 MUST 与 `make_stimulus()` 每行的取值顺序**逐字一致**
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

/// 逐拍激励（**≥10 拍**且**值不同** ⇒ ⇒ 否则 `compare` 判 `no-varying-signal`）
/// ⚠️ 顺序 MUST 与 `make_inputs()` 一致：rst_n · start · write · addr · wdata · gpio_in
/// ⚠️⚠️ **AXI payload MUST 在事务期间保持稳定**：master 的 `awaddr` / `wdata_axi` 是
///   **组合**跟随顶层 `addr` / `wdata` 的 ⇒ ⇒ 激励若在下一行把 `wdata` 清零
///   ⇒ ⇒ W 通道送出去的是 0 而不是 0xA5 ⇒ ⇒ `gpio_out` 恒 0（且**不报错**）。
std::vector<std::vector<std::uint64_t>> make_stimulus() {
    const std::uint64_t gpio_a = 0x5A;
    const std::uint64_t gpio_b = 0xA5;
    const std::uint64_t gpio_data_out = 0x000000A5;
    const std::uint64_t gpio_data_in = 0x00000004;
    const std::uint64_t idle = 0;
    return {
        /* 0  */ {1, 1, 1, 0x00000000, gpio_data_out, gpio_a},  // 写事务：发起
        /* 1  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a},  // 保持 payload
        /* 2  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a},
        /* 3  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a},
        /* 4  */ {1, 0, 0, 0x00000000, gpio_data_out, gpio_a},  // 等写完成 ⇒ gpio_out 变
        /* 5  */ {1, 1, 0, gpio_data_in, idle, gpio_a},         // 读事务 #1：发起
        /* 6  */ {1, 0, 0, gpio_data_in, idle, gpio_a},         // 保持 addr
        /* 7  */ {1, 0, 0, gpio_data_in, idle, gpio_a},
        /* 8  */ {1, 0, 0, gpio_data_in, idle, gpio_a},         // 等读完成 ⇒ rdata = 0x5A
        /* 9  */ {1, 0, 0, gpio_data_in, idle, gpio_b},         // 改 gpio_in ⇒ 让 rdata 能再变
        /* 10 */ {1, 1, 0, gpio_data_in, idle, gpio_b},         // 读事务 #2：发起
        /* 11 */ {1, 0, 0, gpio_data_in, idle, gpio_b},         // 保持 addr
        /* 12 */ {1, 0, 0, gpio_data_in, idle, gpio_b},
        /* 13 */ {1, 0, 0, gpio_data_in, idle, gpio_b},         // 等读完成 ⇒ rdata = 0xA5
    };
}

/// **变异自证**（ADR-073 V5）：把层次化产物里**驱动某个被观测端口**的 `assign`
/// **取反** ⇒ ⇒ 行为改变 ⇒ ⇒ `compare` MUST 判不等（⇒ 证明这条判据不是恒绿）。
///
/// ⚠️ **为什么不能直接取反顶层端口**：顶层端口名出现在**实例端口连接**的右侧
///   （`.child_port(top_port)`）⇒ ⇒ 把它取反 = **组合环** ⇒ ⇒ Verilator 直接报错
///   ⇒ ⇒ 那是「编译失败」而不是「行为差异」⇒ ⇒ 判别力更弱。
/// @return 空串 = 篡改失败（⇒ 自证不成立 ⇒ 判据 MUST 判红）
std::string tamper_hier(const std::string &hier_path,
                        const std::vector<ch::SignalMapEntry> &signal_map,
                        const std::vector<ch::hier_eq::Observation> &observations,
                        const std::string &top_module) {
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
    const std::string child_port = text.substr(name_begin, conn_pos - name_begin);
    if (child_port.empty()) return {};
    (void)top_module;

    const std::string key = "assign " + child_port + " = ";
    const auto pos = text.find(key);
    if (pos == std::string::npos) return {};
    const auto rhs_begin = pos + key.size();
    const auto rhs_end = text.find(';', rhs_begin);
    if (rhs_end == std::string::npos) return {};
    const std::string rhs = text.substr(rhs_begin, rhs_end - rhs_begin);
    if (rhs.empty() || rhs.find('(') != std::string::npos) return {};
    text.replace(rhs_begin, rhs.size(), "~(" + rhs + ")");

    const std::string out_path = hier_path + ".tampered";
    std::ofstream out(out_path);
    if (!out) return {};
    out << text;
    return out_path;
}

/// 产物级自检（4 项）—— 任一失败 ⇒ 返回非零
int check_products(const std::string &out_dir, const std::string &verilog_path) {
    int failures = 0;

    // ⚠️ 设计在这里**重建一次**：CHECK 2（端口面）与 CHECK 4（双形态等价）都要它
    //   ⇒ ⇒ 端口面的唯一权威 = `build_rtl_manifest` 的 `port_table[]`
    //   （与产物端口表**同源** ⇒ 不需要反解产物）
    ch::ch_device<axi4::example::AxiSocTop> dev;
    auto &root = dev.instance();
    const auto manifest = ch::rtl_emit::build_rtl_manifest(root, dev.context());

    // ---- ① 产物 module 集 == 冻结期望集 ----
    const auto mods = product_modules(verilog_path);
    const std::set<std::string> expected(kExpectedModules.begin(), kExpectedModules.end());
    const bool modules_ok = (mods == expected);
    std::cout << "CHECK 1 module 集: " << (modules_ok ? "PASS" : "FAIL") << " (期望 "
              << expected.size() << " 个 · 实际 " << mods.size() << " 个)\n";
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
    // ⚠️ 端口面的唯一权威 = `manifest.port_table[]`（与产物端口表**同源**）
    const std::size_t top_ports = manifest.port_table.size();
    std::size_t descendant_undriven = 0;
    for (const auto &e : manifest.ports) {
        if (e.driven) continue;
        if (e.owner != "top" && !e.owner.empty()) ++descendant_undriven;
    }
    const bool ports_ok = (top_ports == kExpectedTopPorts) && (descendant_undriven == 0);
    std::cout << "CHECK 2 顶层端口: " << (ports_ok ? "PASS" : "FAIL") << " (端口 "
              << top_ports << "/" << kExpectedTopPorts << " · 后代未驱动 "
              << descendant_undriven << ")\n";
    if (!ports_ok) ++failures;

    // ---- ③ lint 干净（verilator --lint-only --top-module top）----
    const std::string verilator = find_verilator();
    if (verilator.empty()) {
        // ⚠️ 2026-11-18：SKIP 改为 **FAIL**（原 SKIP 让「找不到 verilator」变成常态，
        //   lint 静默不验）。`find_verilator` 已改用 `CPPHDL_SOURCE_DIR` 搜索根 ⇒
        //   构建过 verilator 的环境总能找到 ⇒ 找不到就是真异常。
        std::cout << "CHECK 3 lint: FAIL (verilator 不可用 ⇒ **未验证**，按失败计)\n";
        ++failures;
    } else {
        // ⚠️ **argv 式**（`fork` + `execl`）—— 不用 `std::system` 拼串：
        //   ① 路径含空格不再断裂；② 消掉 shell 注入面
        const bool lint_ok = run_lint(verilator, "top", verilog_path);
        std::cout << "CHECK 3 lint: " << (lint_ok ? "PASS" : "FAIL")
                  << " (verilator --lint-only --top-module top)\n";
        if (!lint_ok) ++failures;
    }

    // ---- ④ 双形态逐拍等价（J-4）----
    // ⚠️ **A 侧固定是「扁平产物」**（`hier_eq::compare` 的语义）⇒ ⇒ 本判据 = 「扁平 ≡ 层次化」
    {
        const std::string eq_dir = out_dir + "/eq";
        std::error_code fec;
        std::filesystem::create_directories(eq_dir, fec);
        auto products = ch::hier_eq::emit(root, eq_dir, ch::Granularity::RespectDeclarations);
        if (!products.ok) {
            std::cout << "CHECK 4 双形态等价: FAIL (emit 失败: " << products.error << ")\n";
            ++failures;
        } else {
            const auto inputs = make_inputs(root);
            const auto observations = make_observations(root);
            const auto stimulus = make_stimulus();
            const auto res =
                ch::hier_eq::compare(dev.context(), eq_dir, products.hier_path,
                                     products.ports, inputs, observations, stimulus);
            // ⚠️ **变化量计数**（ADR-073 J-4：MUST 有 ≥2 个随激励变化的观测量）
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
            bool tamper_detected = false;
            std::string tamper_msg;
            if (res.ok && res.equal) {
                const std::string tampered =
                    tamper_hier(products.hier_path, products.signal_map, observations, "top");
                if (tampered.empty()) {
                    tamper_msg = "篡改失败（未找到可取反的观测端口连接）";
                } else {
                    const std::string tamper_dir = eq_dir + "/tampered";
                    std::error_code tec;
                    std::filesystem::create_directories(tamper_dir, tec);
                    const auto res_t = ch::hier_eq::compare(
                        dev.context(), tamper_dir, tampered, products.ports, inputs,
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
                      << " 变化量=" << varying << "/" << observations.size()
                      << " 自证=" << (tamper_detected ? "PASS" : "FAIL")
                      << (tamper_msg.empty() ? "" : " [" + tamper_msg + "]")
                      << (res.error.empty() ? "" : " err=" + res.error) << ")\n";
            if (!ok) ++failures;
        }
    }

    std::cout << (failures == 0 ? "PRODUCTS-CHECK: PASS\n"
                                : "PRODUCTS-CHECK: FAIL (" + std::to_string(failures) + " 项)\n");
    return failures == 0 ? 0 : 1;
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
    if (emit_mode || check_mode) {
        std::string out_dir;
        bool has_out = false;
        for (std::size_t i = 0; i + 1 < rest.size(); ++i) {
            if (rest[i] != "--out") {
                continue;
            }
            const std::string &v = rest[i + 1];
            // ⚠️ **取值形态校验**（2026-11-18 查实：`--out --emit-hygiene` 会把
            //    `--emit-hygiene` 当目录名 ⇒ 静默建成 `--emit-hygiene/`）。
            if (v.empty() || v.front() == '-') {
                std::cerr << "FATAL: --out 的取值不能以 '-' 开头（疑似把 flag 当目录名）：" << v
                          << "\n";
                return 2;
            }
            out_dir = v;
            has_out = true;
            break;
        }
        if (!has_out) {
            std::cerr << "FATAL: --emit-rtl / --check-products 缺 --out <dir>\n";
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
        return check_products(out_dir, out_dir + "/top.v");
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

    std::cout << "RESULT: checks=" << checks << " fails=" << fails << "\n";
    return fails == 0 ? 0 : 1;
}
