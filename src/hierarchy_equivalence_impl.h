// src/hierarchy_equivalence_impl.h
//
// **层次化 ≡ 扁平 逐拍等价性 helper 的内部实现**（P8 · 公共 API 的实现细节）。
//
// ⚠️ **为什么是 `src/` 下的内部头**（★ **不是** `include/`）：
//   · 公共 API 面只应暴露**承诺的最小面**（capability
//     `hierarchical-equivalence-public-helper` 的 Requirement ①）⇒ ⇒ 内部实现
//     的符号（`emit_products` / `compare_products` / `ensure_dir` / …）MUST NOT
//     出现在 `include/`。
//   · ★ 仓内同款先例 —— `src/hierarchy_chain.h` · `src/hierarchy_port_check.h` ·
//     `src/hierarchy_port_set_check.h` · `src/hierarchy_signal_map_check.h`。
//   · ★ **可移植性**：公共头**不**拖进 `core/verilator_backend.h`（★ 它在
//     `__cpp_lib_expected` 缺失时**显式 `#error`** ⇒ ⇒ 会破 macOS/Windows 的
//     `#include` 契约）⇒ ⇒ **只在这里**用它。
//
// ⚠️ **本文件的内容来自 `tests/hier_equivalence.h` 的 `ch_test::hier::impl`**
//   （T3-005 的实现）⇒ ⇒ **迁移**（非重写）⇒ ⇒ ⇒ 行为等价由「既有 9 用例 /
//   414 断言仍全绿」+「既有 3 个产物逐字节不变」承担。
//
// ⚠️ **本文件新增的能力**（★ **不是**既有行为 ⇒ ⇒ 新增工作量）：
//   · **fail-closed** 校验（空输入 / 无随激励变化的信号 / >64bit 端口 / 无顶层输出 /
//     verilator 不可用 / 观测信号在顶层不可见）—— 既有 `compare_products`
//     **不校验观测集** ⇒ ⇒ 空/恒定观测集 ⇒ `equal=true` ⇒ ⇒ **恒绿**
//     ⇒ ⇒ 那正是本 helper 要消除的坑。

#pragma once

#include "hierarchy_equivalence.h"

#include "core/verilator_backend.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace ch::detail {
namespace hier_eq_impl {

/// **失败原因的稳定前缀**（★ 公共契约 ⇒ ⇒ 可机械分类 ⇒ ⇒ 见公共头的清单）。
/// ⚠️ **MUST NOT** 改这些字符串（⇒ ⇒ 已发布的 helper 依赖它们做分类）。
namespace err {
inline constexpr const char *kEmptyObservations = "empty-observations";
inline constexpr const char *kEmptyStimulus = "empty-stimulus";
inline constexpr const char *kNoVaryingSignal = "no-varying-signal";
inline constexpr const char *kEmitFailed = "emit-failed";
inline constexpr const char *kPortSetEmpty = "port-set-empty";
inline constexpr const char *kPortWidth64Exceeded = "port-width-64-exceeded";
inline constexpr const char *kNoTopLevelOutput = "no-top-level-output";
inline constexpr const char *kBackendInitFailed = "backend-init-failed";
inline constexpr const char *kVerilatorUnavailable = "verilator-unavailable";
inline constexpr const char *kObservationNotVisible = "observation-not-visible";
}  // namespace err

/// Verilator 的**正确**获取方式（★ 仓内已知陷阱：`apt install verilator` 是**错的**
/// ⇒ ⇒ 错误串 MUST NOT 建议它）。
inline constexpr const char *kVerilatorBuildHint =
    "verilator is required; build it with: cmake --build build --target verilator";

/// 建目录（单层；父目录 MUST 已存在 —— 调用方给 `mkdtemp` 出来的目录）。
inline bool ensure_dir(const std::string &path) {
    if (::mkdir(path.c_str(), 0755) == 0) {
        return true;
    }
    return ::access(path.c_str(), F_OK) == 0;
}

/// 外部工具是否可用（★ 与 `tests/external_tool_helpers.h` 的 `ch_test::tool_available`
/// **同款**判定 ⇒ ⇒ 但**不能**直接 include 它 ⇒ ⇒ 那是 `tests/` 内部头 ⇒ ⇒
/// `src/` 够不到 ⇒ ⇒ ⇒ 这里**自持一份** ⇒ ⇒ ⇒ 两者 MUST 保持同口径）。
inline bool tool_available(const char *cmd) {
    const std::string which_cmd = std::string("command -v ") + cmd + " >/dev/null 2>&1";
    return std::system(which_cmd.c_str()) == 0;
}

/// 读整个文件（不存在 ⇒ 空串）。
inline std::string read_whole_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

/// 带**稳定前缀**的错误串（★ 公共契约 ⇒ ⇒ 前缀 MUST NOT 变）。
inline std::string prefixed(const char *prefix, const std::string &detail) {
    std::string out(prefix);
    if (!detail.empty()) {
        out += ": ";
        out += detail;
    }
    return out;
}

/// 一次「产物生成」的内部结果。
struct Products {
    bool ok = false;
    std::string error;
    std::string hier_path;
    std::vector<ch::VerilatorPortSpec> ports;
    std::vector<ch::SignalMapEntry> signal_map;
};

/// 一次「逐拍比对」的内部结果。
struct Comparison {
    bool ok = false;
    bool equal = false;
    std::string error;
    std::string first_mismatch;
    std::size_t mismatch_count = 0;
    std::vector<std::vector<std::uint64_t>> flat_trace;
    std::string flat_product;
    std::string hier_product;
};

/// ① 生成层次化产物 + **由产物派生**端口集合 + 映射表。
///
/// ⚠️ 与「比对」**分成两步**是刻意的：调用方要能在比对之前**篡改**产物
///    （判别力自证：「故意破坏层次化产物 ⇒ 等价性判据 MUST 转红」）。
inline Products emit_products(ch::Component &root, const std::string &work_dir,
                              ch::Granularity granularity) {
    Products p;
    p.hier_path = work_dir + "/hier_src.v";
    ch::Component::GranularityScope scope(granularity);
    auto emitted = ch::toVerilogHierarchical(p.hier_path, root);
    if (!emitted.ok) {
        p.error = prefixed(err::kEmitFailed, "hierarchical emission failed: " + emitted.error);
        return p;
    }
    // ⚠️ 端口集合 MUST 由 **emitter 产物**派生（禁手工列举）
    p.ports = ch::backend_port_set(emitted, "top");
    if (p.ports.empty()) {
        p.error = prefixed(err::kPortSetEmpty, "derived port set is empty");
        return p;
    }
    // ⚠️ **>64bit 端口**：Verilator 的硬顶（`include/core/verilator_backend.h`）
    //    ⇒ ⇒ MUST 在**这里**就 fail-closed 并**点名端口 + 位宽**
    //    ⇒ ⇒ 否则错误会延后到 backend 初始化 ⇒ ⇒ 信息里没有端口名。
    for (const auto &port : p.ports) {
        if (port.bitwidth > 64) {
            p.error = prefixed(err::kPortWidth64Exceeded,
                               "port '" + port.port_name + "' is " +
                                   std::to_string(port.bitwidth) +
                                   " bits; the Verilator backend supports at most 64");
            return p;
        }
    }
    // ⚠️ **顶层无 output** ⇒ ⇒ 观测面**无从构造** ⇒ ⇒ MUST 点名成因
    //    （★ 否则用户拿到的是「读到的都是 0」⇒ ⇒ 恒绿假绿）。
    bool has_output = false;
    for (const auto &port : p.ports) {
        if (!port.is_input) {
            has_output = true;
            break;
        }
    }
    if (!has_output) {
        p.error = prefixed(err::kNoTopLevelOutput,
                           "the design exposes no top-level output port; an observation "
                           "surface cannot be constructed");
        return p;
    }
    p.signal_map = emitted.signal_map;
    p.ok = true;
    return p;
}

/// **被观测侧的对账**（★ capability `rtl-hierarchical-output` 的 Requirement ④/⑤
/// 的「被观测侧」一半 ⇒ ⇒ MUST 有机械载体）。
///
/// ⚠️ **为什么用 `ports` 而不是 `signal_map`**：`backend_port_set` 只取**根 module**
///    的端口（`include/hierarchy_verilog.h:136-139`）⇒ ⇒ 「某个 `node_id` 在端口集里」
///    ⇔ 「它在层次化顶层可见」⇒ ⇒ ⇒ 与映射表的 `hier_top_port_name` 非空**同口径**
///    ⇒ ⇒ ⇒ 而 `compare` 的参数里**只有** `ports`（★ 调用方 MAY 传被篡改产物的
///    路径 ⇒ ⇒ 故 MUST 用**端口集**而不是映射表来判可见性）。
///
/// ⚠️ **盲区侧**（「不在观测面内的 `ch_out` MUST 出现在映射表里 ∧
///    `hier_top_port_name` 为空」）**不在这里** ⇒ ⇒ 那需要枚举 DUT 的**全部**
///    `ch_out` ⇒ ⇒ 那是 **DUT 的知识** ⇒ ⇒ helper 拿不到 ⇒ ⇒ 仓内判据承担。
inline bool check_observed_visible(const std::vector<ch::VerilatorPortSpec> &ports,
                                   const std::vector<ch::hier_eq::Observation> &observations,
                                   std::string &detail) {
    for (const auto &obs : observations) {
        bool found = false;
        for (const auto &port : ports) {
            if (port.node_id == obs.node_id) {
                found = true;
                break;
            }
        }
        if (!found) {
            detail = "observed signal '" + obs.name +
                     "' is not a top-level port of the hierarchical product; a child "
                     "module's output is not observable at the hierarchical top level";
            return false;
        }
    }
    return true;
}

/// ② 逐拍比对「扁平产物」与「层次化产物」。
inline Comparison compare_products(ch::core::context *ctx, const std::string &work_dir,
                                   const std::string &hier_path,
                                   const std::vector<ch::VerilatorPortSpec> &ports,
                                   const std::vector<ch::hier_eq::DrivenInput> &inputs,
                                   const std::vector<ch::hier_eq::Observation> &observations,
                                   const std::vector<std::vector<std::uint64_t>> &stimulus) {
    Comparison out;
    if (ctx == nullptr) {
        out.error = prefixed(err::kBackendInitFailed, "null context");
        return out;
    }

    // ⚠️ **fail-closed · 空输入**（★ 既有实现**不**查 ⇒ ⇒ 空 ⇒ ⇒ 恒绿）
    if (observations.empty()) {
        out.error = prefixed(err::kEmptyObservations,
                             "the observation surface is empty; a cycle-by-cycle comparison "
                             "would be vacuously equal");
        return out;
    }
    if (stimulus.empty()) {
        out.error = prefixed(err::kEmptyStimulus,
                             "the stimulus is empty; a cycle-by-cycle comparison would be "
                             "vacuously equal");
        return out;
    }
    // ⚠️ **fail-closed · 观测信号在顶层不可见**（★ 选了子 module 的输出 ⇒ ⇒
    //    读不到 ⇒ ⇒ 「恒 0」⇒ ⇒ 假绿）
    {
        std::string detail;
        if (!check_observed_visible(ports, observations, detail)) {
            out.error = prefixed(err::kObservationNotVisible, detail);
            return out;
        }
    }

    const std::string flat_dir = work_dir + "/flat";
    const std::string hier_dir = work_dir + "/hier";
    if (!ensure_dir(flat_dir) || !ensure_dir(hier_dir)) {
        out.error = prefixed(err::kBackendInitFailed,
                             "cannot create work dirs under " + work_dir);
        return out;
    }

    // ⚠️ **fail-closed · verilator 不可用**（★ 仓内已知陷阱：正确的获取方式是
    //    `cmake --build build --target verilator` ⇒ ⇒ **MUST NOT** 建议 apt）。
    if (!tool_available("verilator")) {
        out.error = prefixed(err::kVerilatorUnavailable, kVerilatorBuildHint);
        return out;
    }

    // ── 扁平侧：走**既有** backend 路径（它自己从 ctx 生成 `top.v` 并编译）
    //    ⇒ 扁平产物 = 该 backend 真正仿真用的那份文件（不是另生成一份 ⇒ 杜绝「两份不一致」）
    ch::Simulator sim_flat(ctx);
    sim_flat.set_backend(std::make_unique<ch::VerilatorBackend>(flat_dir));
    {
        const auto *b = dynamic_cast<const ch::VerilatorBackend *>(sim_flat.backend());
        if (!b || b->port_access_snapshot().empty()) {
            out.error = prefixed(err::kBackendInitFailed,
                                 "flat backend did not initialize (empty port access table)");
            return out;
        }
    }

    // ── 层次化侧：新入口（**致命**语义 ⇒ 失败即报错，不静默继续）
    {
        auto backend_hier = std::make_unique<ch::VerilatorBackend>(hier_dir);
        ch::data_map_t scratch;
        auto init = backend_hier->try_initialize_with_verilog(hier_path, ports, scratch);
        if (!init.has_value()) {
            out.error = prefixed(err::kBackendInitFailed,
                                 "hierarchical backend init failed: " + init.error().message);
            return out;
        }
        ch::Simulator sim_hier(ctx);
        sim_hier.set_backend(std::move(backend_hier));  // ⇒ 只重绑 `data_map`（不重推端口表）

        // ── 逐拍驱动 + 比对（激励**逐拍**施加 ⇒ 寄存器通路才被真正走通）
        out.flat_trace.reserve(stimulus.size());
        for (std::size_t cycle = 0; cycle < stimulus.size(); ++cycle) {
            const auto &values = stimulus[cycle];
            for (std::size_t i = 0; i < inputs.size() && i < values.size(); ++i) {
                inputs[i].set(sim_flat, values[i]);
                inputs[i].set(sim_hier, values[i]);
            }
            sim_flat.tick();
            sim_hier.tick();

            std::vector<std::uint64_t> row;
            row.reserve(observations.size());
            for (const auto &obs : observations) {
                const std::uint64_t vf = obs.read(sim_flat);
                const std::uint64_t vh = obs.read(sim_hier);
                row.push_back(vf);
                if (vf != vh) {
                    ++out.mismatch_count;
                    if (out.first_mismatch.empty()) {
                        out.first_mismatch = "cycle " + std::to_string(cycle) +
                                             ", observation '" + obs.name + "': flat=0x" +
                                             std::to_string(vf) + " hier=0x" + std::to_string(vh);
                    }
                }
            }
            out.flat_trace.push_back(std::move(row));
        }
    }

    out.flat_product = read_whole_file(flat_dir + "/top.v");
    out.hier_product = read_whole_file(hier_path);
    out.equal = out.first_mismatch.empty();

    // ⚠️ **fail-closed · 「逐拍全等」但观测集合**无任何信号随激励变化**
    //    ⇒ ⇒ 此刻「全等」是**空洞**的（★ 观测值全是常量 ⇒ ⇒ 任何恒定产物都能
    //    通过 ⇒ ⇒ 判据无判别力）⇒ ⇒ MUST 拒绝。
    //    ⚠️ **只在「本就要宣告全等」时判**：若已经**出现分歧** ⇒ ⇒ 判据**显然**有
    //    判别力 ⇒ ⇒ 此时的错误信息 MUST 是「分歧」而不是「空洞」⇒ ⇒ 顺序不可颠倒。
    //    ⚠️ **单拍激励无法判**（★ 只有一组取值 ⇒ ⇒ 恒定）⇒ ⇒ 只在 ≥2 拍时判。
    if (out.equal && stimulus.size() >= 2) {
        bool any_varies = false;
        for (std::size_t o = 0; o < observations.size() && !any_varies; ++o) {
            std::set<std::uint64_t> values;
            for (const auto &row : out.flat_trace) {
                if (o < row.size()) {
                    values.insert(row[o]);
                }
            }
            if (values.size() >= 2) {
                any_varies = true;
            }
        }
        if (!any_varies) {
            out.ok = false;
            out.equal = false;
            out.error = prefixed(err::kNoVaryingSignal,
                                 "no observed signal takes more than one value across the "
                                 "stimulus; a cycle-by-cycle equality would hold for any "
                                 "constant product");
            return out;
        }
    }

    out.ok = true;
    return out;
}

}  // namespace hier_eq_impl
}  // namespace ch::detail
