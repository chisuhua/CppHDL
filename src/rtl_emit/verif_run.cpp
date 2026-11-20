// src/rtl_emit/verif_run.cpp
//
// **IR 解释器** —— 执行后端之一（`rtl-verification-single-source` · ADR-074 D1/D6）。
//
// ⚠️ 端口解析链：DSL 标识符 → `ports[].cpp_member` → `port_table[].name` →
//    `context` 节点名 → `node_id` ⇒ ⇒ **只靠字符串名**（框架无反射）。
// ⚠️ 解析失败（未命中 / 多义）⇒ **fail-closed**（`ok=false` + 点名），MUST NOT 静默读 0。

#include "rtl_emit/verif.h"

#include "component.h"
#include "core/context.h"
#include "core/lnodeimpl.h"
#include "simulator.h"

#include <unordered_map>
#include <utility>
#include <vector>

namespace ch {
namespace rtl_emit {
namespace {

constexpr const char *kOpaque = "unnamed_input";
constexpr const char *kOpaqueOut = "unnamed_output";

struct Resolver {
    /// **(owner 路径, C++ 成员名)** → node_id（重名 ⇒ `UINT32_MAX` = 多义）
    std::unordered_map<std::string, std::uint32_t> by_owner_member;
    /// 根组件的 owner 路径（= manifest 里 `is_top_io` 条目的 owner）
    std::string root_owner;
    /// (owner 路径, 成员名) → 产物端口名
    std::unordered_map<std::string, std::string> port_name_of;
    /// 成员名 → 候选（owner 路径, node_id）列表（用于「成员名唯一」的兜底）
    std::vector<std::pair<std::string, std::uint32_t>> by_member;
    std::vector<std::string> errors;

    static std::string key(const std::string &owner, const std::string &member) {
        return owner + "\x1f" + member;
    }

    /// 节点的 **(owner 路径, 成员名)**（与 `manifest.cpp` 的 `cpp_member` 口径**逐字一致**）
    static bool owner_member(ch::core::lnodeimpl *node, std::string &owner,
                              std::string &member) {
        const std::string &nm = node->name();
        ch::Component *parent = node->get_parent();
        owner = (parent != nullptr) ? parent->name() : std::string();
        if (!owner.empty() && nm.rfind(owner + ".", 0) == 0) {
            member = nm.substr(owner.size() + 1);
        } else {
            member = nm;
        }
        return true;
    }

    bool build(ch::core::context *ctx, const RtlManifest &manifest) {
        if (!ctx) {
            errors.push_back("context 为空");
            return false;
        }
        // ① 根组件的 owner 路径 + 产物端口名映射（权威 = manifest 的 `is_top_io` 条目）
        for (const auto &p : manifest.ports) {
            if (!p.is_top_io) {
                continue;
            }
            if (p.cpp_member.empty() || p.cpp_member == kOpaque || p.cpp_member == kOpaqueOut) {
                errors.push_back("顶层端口 `" + p.name + "` 的 cpp_member 是占位符（端口未具名）");
                continue;
            }
            if (root_owner.empty()) {
                root_owner = p.owner;
            } else if (root_owner != p.owner) {
                errors.push_back("manifest 的顶层端口来自多个 owner（`" + root_owner + "` / `" +
                                 p.owner + "`）⇒ ⇒ 根不唯一");
            }
            const auto k = key(p.owner, p.cpp_member);
            auto it = port_name_of.find(k);
            if (it == port_name_of.end()) {
                port_name_of.emplace(k, p.name);
            } else if (it->second != p.name) {
                errors.push_back("(" + p.owner + ", " + p.cpp_member + ") 映射到多个端口名");
            }
        }
        if (root_owner.empty()) {
            errors.push_back("manifest 里没有 `is_top_io` 的顶层端口 ⇒ ⇒ 无法按名寻址");
            return false;
        }
        // ② ctx 节点 → (owner, member) → node_id
        for (const auto &node_ptr : ctx->get_nodes()) {
            auto *node = node_ptr.get();
            if (!node) {
                continue;
            }
            const auto ty = node->type();
            if (ty != ch::core::lnodetype::type_input &&
                ty != ch::core::lnodetype::type_output &&
                ty != ch::core::lnodetype::type_clock &&
                ty != ch::core::lnodetype::type_reset) {
                continue;
            }
            std::string owner;
            std::string member;
            owner_member(node, owner, member);
            by_member.emplace_back(member, node->id());
            const auto k = key(owner, member);
            auto it = by_owner_member.find(k);
            if (it == by_owner_member.end()) {
                by_owner_member.emplace(k, node->id());
            } else if (it->second != node->id()) {
                it->second = UINT32_MAX; // 重名 ⇒ 多义
            }
        }
        return errors.empty();
    }

    bool resolve(const std::string &ident, std::uint32_t &out, std::string &err) const {
        // ① **根组件自有**的端口（owner == 根 + 成员名 == ident）—— 产品端口面
        {
            const auto k = key(root_owner, ident);
            auto it = by_owner_member.find(k);
            if (it != by_owner_member.end()) {
                if (it->second == UINT32_MAX) {
                    err = "端口 `" + ident + "` 在根组件上**重名**（多义）";
                    return false;
                }
                out = it->second;
                return true;
            }
        }
        // ② **产物端口名**（`top_<member>`）反查（根 owner + 成员名）
        for (const auto &[k, pname] : port_name_of) {
            if (pname != ident) {
                continue;
            }
            const auto sep = k.find('\x1f');
            const std::string owner = k.substr(0, sep);
            const std::string member = k.substr(sep + 1);
            auto it = by_owner_member.find(key(owner, member));
            if (it != by_owner_member.end() && it->second != UINT32_MAX) {
                out = it->second;
                return true;
            }
        }
        // ③ **成员名唯一**时的兜底（不限 owner）—— 后代端口（成层/内联）
        std::vector<std::uint32_t> hits;
        for (const auto &[member, id] : by_member) {
            if (member == ident) {
                hits.push_back(id);
            }
        }
        if (hits.size() == 1) {
            out = hits.front();
            return true;
        }
        if (hits.size() > 1) {
            err = "端口 `" + ident + "` 匹配到 " + std::to_string(hits.size()) +
                  " 个节点（多义）";
            return false;
        }
        err = "端口 `" + ident + "` 未命中（未具名 / 拼写错误 / 不是顶层端口）";
        return false;
    }
};

} // namespace

VerifRunResult run_verif(const VerifProgram &program, ch::Simulator &sim,
                         ch::core::context *ctx, const RtlManifest &manifest,
                         const VerifRunOptions &options) {
    VerifRunResult res;
    Resolver r;
    if (!r.build(ctx, manifest)) {
        res.error = r.errors.empty() ? "端口解析表构建失败" : r.errors.front();
        return res;
    }

    auto resolve_or_fail = [&](const std::string &port, std::uint32_t &id,
                               const std::string &where) {
        std::string err;
        if (!r.resolve(port, id, err)) {
            res.error = where + "：" + err;
            return false;
        }
        return true;
    };

    // ── 框架复位（与渲染式 TB 的 `write_tb_reset_pulse` 同构）──
    // ⚠️ 两后端 MUST 从**同一初始状态**起跑 ⇒ ⇒ 否则「语义只有一份」在**起点**就破裂。
    //   渲染式 TB 的输入在 0 时刻被 NSDMI 置 0 ⇒ 解释器侧要显式做同样的事。
    if (options.apply_frame_reset) {
        for (const auto &p : manifest.port_table) {
            if (p.dir != "input" || p.role != "data") {
                continue;
            }
            std::uint32_t id = 0;
            std::string err;
            if (r.resolve(p.name, id, err)) {
                sim.set_value_by_node_id(id, 0);
            }
        }
        sim.reset();
        for (const auto &p : manifest.port_table) {
            if (p.dir != "input" || p.role != "data") {
                continue;
            }
            std::uint32_t id = 0;
            std::string err;
            if (r.resolve(p.name, id, err)) {
                sim.set_value_by_node_id(id, 0);
            }
        }
    }

    for (const auto &sc : program.scenarios) {
        for (const auto &op : sc.ops) {
            switch (op.kind) {
            case VerifOpKind::Tick:
                for (std::uint32_t i = 0; i < op.count; ++i) {
                    sim.tick();
                }
                break;
            case VerifOpKind::Settle:
                sim.eval_combinational();
                break;
            case VerifOpKind::Set: {
                std::uint32_t id = 0;
                if (!resolve_or_fail(op.port, id, sc.name + " · SET " + op.port)) {
                    return res;
                }
                if (!sim.set_value_by_node_id(id, op.value)) {
                    res.error = sc.name + " · SET " + op.port + "：node_id 未命中";
                    return res;
                }
                break;
            }
            case VerifOpKind::Reset: {
                std::uint32_t id = 0;
                if (!resolve_or_fail(op.port, id, sc.name + " · RESET " + op.port)) {
                    return res;
                }
                sim.set_value_by_node_id(id, op.value);
                for (std::uint32_t i = 0; i < op.count; ++i) {
                    sim.tick();
                }
                sim.set_value_by_node_id(id, op.value ? 0u : 1u);
                break;
            }
            case VerifOpKind::Wait: {
                std::uint32_t id = 0;
                if (!resolve_or_fail(op.port, id, sc.name + " · WAIT " + op.port)) {
                    return res;
                }
                bool seen = false;
                for (std::uint32_t i = 0; i < op.count; ++i) {
                    sim.tick();
                    const auto v = sim.get_value_by_node_id(id);
                    if (!v) {
                        res.error = sc.name + " · WAIT " + op.port + "：node_id 未命中";
                        return res;
                    }
                    // 目标电平判定 = **零值测试**（ADR-074 §7.2 D2′：`high` ≡ 非零 ≡ 缺省
                    // ⇒ **不是**「N 位全 1」⇒ 不需要端口位宽）
                    const bool hit = (op.level == WaitLevel::Low) ? (*v == 0u) : (*v != 0u);
                    if (hit) {
                        seen = true;
                        break;
                    }
                }
                ++res.checks;
                if (!seen) {
                    ++res.fails;
                    res.check_lines.push_back("CHECK " + op.check_id + " FAIL");
                    res.failure_lines.push_back("CHECK " + op.check_id + " FAIL (timeout " +
                                                op.port + " max=" +
                                                std::to_string(op.count) + ")");
                } else {
                    res.check_lines.push_back("CHECK " + op.check_id + " PASS");
                }
                break;
            }
            case VerifOpKind::Expect: {
                std::uint32_t id = 0;
                if (!resolve_or_fail(op.port, id, sc.name + " · EXPECT " + op.port)) {
                    return res;
                }
                const auto v = sim.get_value_by_node_id(id);
                if (!v) {
                    res.error = sc.name + " · EXPECT " + op.port + "：node_id 未命中";
                    return res;
                }
                ++res.checks;
                if (*v != op.value) {
                    ++res.fails;
                    res.check_lines.push_back("CHECK " + op.check_id + " FAIL");
                    res.failure_lines.push_back("CHECK " + op.check_id + " FAIL (" + op.port +
                                                " got " + std::to_string(*v) + " want " +
                                                std::to_string(op.value) + ")");
                } else {
                    res.check_lines.push_back("CHECK " + op.check_id + " PASS");
                }
                break;
            }
            case VerifOpKind::ExpectStable: {
                std::uint32_t id = 0;
                if (!resolve_or_fail(op.port, id, sc.name + " · EXPECT_STABLE " + op.port)) {
                    return res;
                }
                const auto held = sim.get_value_by_node_id(id);
                if (!held) {
                    res.error = sc.name + " · EXPECT_STABLE " + op.port + "：node_id 未命中";
                    return res;
                }
                for (std::uint32_t i = 0; i < op.count; ++i) {
                    sim.tick();
                    const auto v = sim.get_value_by_node_id(id);
                    ++res.checks;
                    if (!v || *v != *held) {
                        ++res.fails;
                        res.check_lines.push_back("CHECK " + op.check_id + " FAIL");
                        res.failure_lines.push_back("CHECK " + op.check_id + " FAIL (unstable " +
                                                    op.port + ")");
                    } else {
                        res.check_lines.push_back("CHECK " + op.check_id + " PASS");
                    }
                }
                break;
            }
            case VerifOpKind::Observe: {
                std::uint32_t id = 0;
                if (!resolve_or_fail(op.port, id, sc.name + " · OBSERVE " + op.port)) {
                    return res;
                }
                if (options.verbose) {
                    const auto v = sim.get_value_by_node_id(id);
                    res.check_lines.push_back("OBSERVE " + sc.name + " " + op.port + " = " +
                                              (v ? std::to_string(*v) : std::string("<miss>")));
                }
                break;
            }
            }
        }
    }
    res.ok = true;
    return res;
}

} // namespace rtl_emit
} // namespace ch
