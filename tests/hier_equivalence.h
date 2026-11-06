// tests/hier_equivalence.h
//
// **层次化 ≡ 扁平 逐拍等价 harness**（T3-005 · ADR-058 §2.2 判据②③④ +
// capability `rtl-hierarchical-output` 的 ADDED Requirement「层次化产物 MUST 与扁平产物等价」）。
//
// ⚠️ **P8 形态约束**（roadmap「P8」条 + A4 裁决）：本 harness MUST 写成「**可公共化**」形态
//   （内部实现与公共包装**分离**），但本 change **MUST NOT** 暴露公共 API ⇒ ⇒ 两层都在 `tests/`：
//   · `ch_test::hier::impl` —— **内部实现**（产物生成 · backend 装配 · 逐拍驱动）
//   · `ch_test::hier`      —— **薄包装**（将来公共化时**这一层**就是公共 API 的形状）
//
// ⚠️ **比较的是两份 Verilog**（扁平 vs 层次化）⇒ ⇒ **不是**「C++ 解释器 vs Verilog」
//   ⇒ ⇒ 双后端 parity 抓不到这一族缺陷（两后端由**同一份 C++** 生成 ⇒ 同构缺陷会被盖章）。
//
// ⚠️ **观测面由调用方手写**（ADR-058 §2.2 ②：`O` MUST 显式声明 · MUST 非空 · MUST 至少含一个
//    **随激励变化**的信号）⇒ ⇒ harness **不猜**观测面（猜 = 回到「全部输出」的空洞判据，
//    且会踩「子输出在层次化顶层不可观测」的盲区 ⇒ ADR-058 §4 风险 6）。
//
// ⚠️ **激励施加 MUST 用 `set_input_value(port, <纯整数>)`**：`<<=` 会把顶层端口变成
//    「有内部驱动」⇒ 该端口不再是顶层端口 ⇒ 两侧观测面不一致 ⇒ **假分歧**。

#pragma once

#include "ch.hpp"
#include "codegen_verilog.h"
#include "core/verilator_backend.h"
#include "core/verilator_port_set.h"
#include "hierarchy_verilog.h"
#include "simulator.h"

#include <cstdint>
#include <fstream>
#include <functional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace ch_test::hier::impl {

/// 一个**受驱动的顶层输入**（`set` MUST 走 `Simulator::set_input_value`）。
struct DrivenInput {
    std::string name;
    std::function<void(ch::Simulator &, std::uint64_t)> set;
};

/// 一个**被观测**的信号（观测集合 `O` 的元素）。
struct Observation {
    std::string name;
    std::function<std::uint64_t(ch::Simulator &)> read;
};

/// 逐拍观测轨迹：`trace[cycle][observation_index]`。
using Trace = std::vector<std::vector<std::uint64_t>>;

/// 一次「产物生成」的结果。
struct Products {
    bool ok = false;
    std::string error;
    std::string hier_path;                            ///< 层次化产物路径
    std::vector<ch::VerilatorPortSpec> ports;          ///< **由产物派生**的端口集合（ADR-058 §2.3 E1）
    std::vector<ch::SignalMapEntry> signal_map;        ///< **由产物派生**的映射表（ADR-058 §2.2）
};

/// 一次「逐拍比对」的结果。
struct Comparison {
    bool ok = false;                                   ///< 运行本身是否成功（false ⇒ `error` 有原因）
    bool equal = false;                                ///< 逐拍全等
    std::string error;                                 ///< 运行失败的原因（点名）
    std::string first_mismatch;                        ///< 空 = 全等
    std::size_t mismatch_count = 0;
    Trace trace;                                       ///< 扁平侧逐拍观测值
    std::string flat_product;                          ///< 扁平产物文本
    std::string hier_product;                          ///< 被仿真的层次化产物文本
};

/// 建目录（单层；父目录 MUST 已存在 —— 调用方给 `mkdtemp` 出来的目录）。
inline bool ensure_dir(const std::string &path) {
    if (::mkdir(path.c_str(), 0755) == 0) {
        return true;
    }
    return ::access(path.c_str(), F_OK) == 0;
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

/// ① 生成层次化产物 + **由产物派生**端口集合。
///
/// ⚠️ 与「比对」**分成两步**是刻意的：调用方要能在比对之前**篡改**产物
///    （判别力自证：「故意破坏层次化产物 ⇒ 等价性判据 MUST 转红」）。
inline Products emit_products(ch::Component &root, const std::string &work_dir) {
    Products p;
    p.hier_path = work_dir + "/hier_src.v";
    auto emitted = ch::toVerilogHierarchical(p.hier_path, root);
    if (!emitted.ok) {
        p.error = "hierarchical emission failed: " + emitted.error;
        return p;
    }
    // ⚠️ 端口集合 MUST 由 **emitter 产物**派生（ADR-058 §2.3 E1 · 禁手工列举）
    p.ports = ch::backend_port_set(emitted, "top");
    if (p.ports.empty()) {
        p.error = "derived port set is empty";
        return p;
    }
    p.signal_map = emitted.signal_map;
    p.ok = true;
    return p;
}

/// ② 逐拍比对「扁平产物」与「层次化产物」。
///
/// @param hier_path 被仿真的层次化产物（**可指向被篡改的副本** ⇒ 判别力自证用）
/// @param ports     由**原始**产物派生的端口集合（篡改只动连线/方向 ⇒ 端口集合不变）
inline Comparison compare_products(ch::core::context *ctx, const std::string &work_dir,
                                   const std::string &hier_path,
                                   const std::vector<ch::VerilatorPortSpec> &ports,
                                   const std::vector<DrivenInput> &inputs,
                                   const std::vector<Observation> &observations,
                                   const std::vector<std::vector<std::uint64_t>> &stimulus) {
    Comparison out;
    if (!ctx) {
        out.error = "null context";
        return out;
    }
    const std::string flat_dir = work_dir + "/flat";
    const std::string hier_dir = work_dir + "/hier";
    if (!ensure_dir(flat_dir) || !ensure_dir(hier_dir)) {
        out.error = "cannot create work dirs under " + work_dir;
        return out;
    }

    // ── 扁平侧：走**既有** backend 路径（它自己从 ctx 生成 `top.v` 并编译）
    //    ⇒ 扁平产物 = 该 backend 真正仿真用的那份文件（不是另生成一份 ⇒ 杜绝「两份不一致」）
    ch::Simulator sim_flat(ctx);
    sim_flat.set_backend(std::make_unique<ch::VerilatorBackend>(flat_dir));
    {
        const auto *b = dynamic_cast<const ch::VerilatorBackend *>(sim_flat.backend());
        if (!b || b->port_access_snapshot().empty()) {
            out.error = "flat backend did not initialize (empty port access table)";
            return out;
        }
    }

    // ── 层次化侧：新入口（**致命**语义 ⇒ 失败即报错，不静默继续）
    {
        auto backend_hier = std::make_unique<ch::VerilatorBackend>(hier_dir);
        ch::data_map_t scratch;
        auto init = backend_hier->try_initialize_with_verilog(hier_path, ports, scratch);
        if (!init.has_value()) {
            out.error = "hierarchical backend init failed: " + init.error().message;
            return out;
        }
        ch::Simulator sim_hier(ctx);
        sim_hier.set_backend(std::move(backend_hier));  // ⇒ 只重绑 `data_map`（不重推端口表）

        // ── 逐拍驱动 + 比对（激励**逐拍**施加 ⇒ 寄存器通路才被真正走通）
        out.trace.reserve(stimulus.size());
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
            out.trace.push_back(std::move(row));
        }
    }

    out.flat_product = read_whole_file(flat_dir + "/top.v");
    out.hier_product = read_whole_file(hier_path);
    out.equal = out.first_mismatch.empty();
    out.ok = true;
    return out;
}

}   // namespace ch_test::hier::impl

// ─────────────────────────────────────────────────────────────────────────
// 薄包装层（将来公共化时**这一层**就是公共 API 的形状）
// ⚠️ 数据参数与返回值**全部 value-only**（不带 IR 指针 / 内部对象）
// ⚠️ 本 change **不**把它放进 `include/`（P8 独立立项）⇒ ⇒ 仍是 `tests/` 内部。
// ─────────────────────────────────────────────────────────────────────────
namespace ch_test::hier {

using Observation = impl::Observation;
using DrivenInput = impl::DrivenInput;
using Trace = impl::Trace;

/// ① 生成层次化产物 + 派生端口集合。
struct ProductSet {
    bool ok = false;
    std::string error;
    std::string hier_path;
    std::vector<ch::VerilatorPortSpec> ports;
    std::vector<ch::SignalMapEntry> signal_map;
};

inline ProductSet emit(ch::Component &root, const std::string &work_dir) {
    impl::Products p = impl::emit_products(root, work_dir);
    ProductSet out;
    out.ok = p.ok;
    out.error = p.error;
    out.hier_path = p.hier_path;
    out.ports = p.ports;
    out.signal_map = p.signal_map;
    return out;
}

/// ② 逐拍比对（返回 value-only 的结论）。
struct Result {
    bool ok = false;             ///< 运行本身成功
    bool equal = false;          ///< 逐拍全等
    std::string error;           ///< 运行失败原因
    std::string first_mismatch;  ///< 空 = 全等
    std::size_t mismatch_count = 0;
    Trace trace;
    std::string flat_product;
    std::string hier_product;
};

inline Result compare(ch::core::context *ctx, const std::string &work_dir,
                       const std::string &hier_path,
                       const std::vector<ch::VerilatorPortSpec> &ports,
                       const std::vector<DrivenInput> &inputs,
                       const std::vector<Observation> &observations,
                       const std::vector<std::vector<std::uint64_t>> &stimulus) {
    impl::Comparison c = impl::compare_products(ctx, work_dir, hier_path, ports, inputs,
                                                 observations, stimulus);
    Result out;
    out.ok = c.ok;
    out.equal = c.equal;
    out.error = c.error;
    out.first_mismatch = c.first_mismatch;
    out.mismatch_count = c.mismatch_count;
    out.trace = std::move(c.trace);
    out.flat_product = std::move(c.flat_product);
    out.hier_product = std::move(c.hier_product);
    return out;
}

}   // namespace ch_test::hier
