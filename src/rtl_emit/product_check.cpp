// src/rtl_emit/product_check.cpp
//
// **产物级自检的产品无关规则**（`example-file-boundary-2026-11` **F2** · 蓝图 §4.3.1）。
//
// ⚠️ 本文件是**从 example 侧搬下来的规则**（原 `examples/axi4/emit_rtl_soc.cpp` 的
//   checker 部分 ⇒ ⇒ B6 内联进 `tb_axi_soc_example.cpp` ⇒ ⇒ 本批再搬到库层）。
//   ⇒ ⇒ 搬的是**规则**（判定逻辑 + 编排）⇒ ⇒ **值**（期望集 / 端口数 / 激励面 /
//!    观测面 / 阈值 / `--top-module` 名）**留在 example**。
//
// ⚠️ **行为逐字保持**（AC18 兜底）：4 项判据的顺序 · `failures` 累加 · `eq/` 产出 ·
//!   `CHECK 1..4` 与 `PRODUCTS-CHECK` 结论行 · 退出码 0/1 ⇒ ⇒ 与搬移前**逐字一致**。
//   ⇒ ⇒ **唯一的行为差异**（**有意为之** · 2026-11-18）：CHECK 3 在「linter 不可用」
//   时由 **SKIP（不计失败）** 改为 **FAIL（计失败）** ⇒ ⇒ 理由 = SKIP 让「找不到
//   linter」成为常态 ⇒ ⇒ lint **从未被验**却显示绿灯（fail-open）。
//   ⇒ ⇒ 该差异在 change 的 F2 任务行与本仓 CHANGELOG 里**已登记**。

#include "rtl_emit/product_check.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace ch {
namespace rtl_emit {

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

    std::ifstream f(hier_path);
    if (!f) return {};
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // 找 `assign <child_port> = <rhs>;` 段：child_port = top_port 的**末段**
    // （顶层端口名形如 `top_<member>` ⇒ 子端口名是其末段）
    // ⚠️ 搜的是 **`<port>(` 的左括号处**（不是端口名本身）⇒ ⇒ 否则会落在
    //   端口**声明**处 ⇒ ⇒ 取到的 child_port 是垃圾 ⇒ ⇒ 找不到 assign ⇒ 自证失败。
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

int check_products(const ProductCheckSpec &spec, const ProductCheckHooks &hooks,
                   const DesignFactory &factory) {
    // ⚠️ **本函数只返回 0/1**（AC18：「退出码语义（0/1）不变」）⇒ ⇒ 用法错误
    //   （工厂为空 / 回调缺失）**按失败计**（fail-closed）⇒ ⇒ 不得返回 2。
    if (!factory) {
        std::cerr << "::error::设计工厂为空（fail-closed · 按失败计）\n";
        return 1;
    }
    if (!hooks.make_inputs || !hooks.make_observations || !hooks.make_stimulus) {
        std::cerr << "::error::激励面回调缺失（fail-closed · 按失败计）\n";
        return 1;
    }

    int failures = 0;

    // ── 重建设计（CHECK 2 的端口面与 CHECK 4 的比对都要它）────────────────
    //   ⚠️ 构造路径**与发射路径同型**（ctx + ctx_swap + factory + build）
    //   ⇒ ⇒ 被检查的设计 == 被发射的设计（不像 `ch_device` 那样多一层构造帧）
    auto ctx = std::make_unique<core::context>("rtl_emit_product_check");
    std::unique_ptr<Component> root;
    {
        core::ctx_swap swap(ctx.get());
        {
            // ⚠️ **截断构造作用域帧**（与 `ch_device` **同型** · `include/device.h`）：
            //   「基类构造先于派生类成员构造」⇒ ⇒ 根的 `__io` 端口在**派生类 ctor** 里造
            //   ⇒ ⇒ 帧必须活到「派生类成员构造完毕」才截断（`ch_device` 就是这么做的）
            //   ⇒ ⇒ 本层与 `ch_device` 保持**同一条构造路径**（⇒ ⇒ 端口归属一致）。
            Component::ConstructionFrame frame;
            root = factory(ctx.get());
        }
        if (!root) {
            std::cerr << "::error::设计注册返回空（fail-closed · 按失败计）\n";
            return 1;
        }
        root->build(ctx.get());
    }
    // ⚠️ owner 基准**由根组件派生**（不是字面量）⇒ ⇒ 根名可配 ⇒ ⇒ 不会假红
    const std::string root_name = root->name();
    const auto manifest = build_rtl_manifest(*root, ctx.get());

    // ---- ① 产物 module 集 == 冻结期望集 ----
    const auto mods = product_modules(spec.verilog_path);
    const std::set<std::string> expected(spec.expected_modules.begin(),
                                         spec.expected_modules.end());
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
        if (e.owner != root_name && !e.owner.empty()) ++descendant_undriven;
    }
    const bool ports_ok = (top_ports == spec.expected_top_ports) && (descendant_undriven == 0);
    std::cout << "CHECK 2 顶层端口: " << (ports_ok ? "PASS" : "FAIL") << " (端口 "
              << top_ports << "/" << spec.expected_top_ports << " · 后代未驱动 "
              << descendant_undriven << ")\n";
    if (!ports_ok) ++failures;

    // ---- ③ lint 干净（调用方执行 linter；缺失 ⇒ 按失败计）----
    if (!spec.lint) {
        std::cout << "CHECK 3 lint: FAIL (linter 不可用 ⇒ **未验证**，按失败计)\n";
        ++failures;
    } else {
        const bool lint_ok = spec.lint(spec.verilog_path);
        std::cout << "CHECK 3 lint: " << (lint_ok ? "PASS" : "FAIL") << " (verilator --lint-only"
                  << (spec.lint_top_module.empty() ? "" : " --top-module " + spec.lint_top_module)
                  << ")\n";
        if (!lint_ok) ++failures;
    }

    // ---- ④ 双形态逐拍等价（+ 变异自证）----
    // ⚠️ **A 侧固定是「扁平产物」**（`hier_eq::compare` 的语义）⇒ ⇒ 本判据 = 「扁平 ≡ 层次化」
    {
        const std::string eq_dir = spec.work_dir + "/eq";
        std::error_code fec;
        std::filesystem::create_directories(eq_dir, fec);
        auto products = ch::hier_eq::emit(*root, eq_dir, ch::Granularity::RespectDeclarations);
        if (!products.ok) {
            std::cout << "CHECK 4 双形态等价: FAIL (emit 失败: " << products.error << ")\n";
            ++failures;
        } else {
            const auto inputs = hooks.make_inputs(*root);
            const auto observations = hooks.make_observations(*root);
            const auto stimulus = hooks.make_stimulus();
            const auto res =
                ch::hier_eq::compare(ctx.get(), eq_dir, products.hier_path,
                                     products.ports, inputs, observations, stimulus);
            // ⚠️ **变化量计数**（ADR-073 J-4：MUST 有 ≥N 个随激励变化的观测量）
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
                    tamper_hier(products.hier_path, products.signal_map, observations, root_name);
                if (tampered.empty()) {
                    tamper_msg = "篡改失败（未找到可取反的观测端口连接）";
                } else {
                    const std::string tamper_dir = eq_dir + "/tampered";
                    std::error_code tec;
                    std::filesystem::create_directories(tamper_dir, tec);
                    const auto res_t = ch::hier_eq::compare(
                        ctx.get(), tamper_dir, tampered, products.ports, inputs,
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
            const bool ok = res.ok && res.equal && (varying >= spec.min_varying_observations) &&
                            tamper_detected;
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

}   // namespace rtl_emit
}   // namespace ch
