// src/rtl_emit/verif_run.cpp
//
// **IR 解释器** —— 执行后端之一（`rtl-verification-single-source` · ADR-074 D1/D6）。
//
// ⚠️ 端口解析链：DSL 标识符 → `ports[].cpp_member` → `port_table[].name` →
//    `context` 节点名 → `node_id` ⇒ ⇒ **只靠字符串名**（框架无反射）。
// ⚠️ 解析失败（未命中 / 多义）⇒ **fail-closed**（`ok=false` + 点名），MUST NOT 静默读 0。

#include "rtl_emit/verif.h"

#include "verif_internal.h"

#include "ast/ast_nodes.h"
#include "component.h"
#include "core/context.h"
#include "core/lnodeimpl.h"
#include "simulator.h"

#include <fstream>
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

    // ── `RESET` 不得指向框架复位端口（两端同源的 fail-closed）──────────────
    // ⚠️ **必须在解析之前**判 ⇒ ⇒ 否则「解释器接受、渲染器拒绝」⇒ ⇒ 两端结论分叉。
    {
        const std::string viol = verif_reset_port_violation(program, manifest);
        if (!viol.empty()) {
            res.error = viol;
            return res;
        }
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

    // ── 框架复位（与渲染式 TB 的 `write_tb_reset_pulse` **同终态**）──
    // ⚠️ 两后端 MUST 从**同一初始状态**起跑 ⇒ ⇒ 否则「语义只有一份」在**起点**就破裂。
    //   渲染式 TB（`write_tb_reset_pulse`）= `default_reset=1; repeat(4) @(posedge);
    //   default_reset=0; @(posedge);`，其寄存器复位分支写成
    //   `if (default_reset) reg <= <INIT>;` ⇒ ⇒ **复位终态 = 每个寄存器回到 INIT**。
    // ⚠️ **解释器侧等价的终态**（2026-11-21 · uart_rx 迁移实测）：
    //   `Simulator::reset()` 把**所有**节点清 0（`src/simulator.cpp`）⇒ ⇒ 对
    //   `INIT != 0` 的寄存器会起在**错误初态**（uart_rx 的 `idle`/`rx_d` INIT=true
    //   ⇒ 接收器起在「非空闲」⇒ 解码全错）。⇒ ⇒ **按 `regimpl::get_init_val()` 回填**。
    // ⚠️ **为什么不是「驱动 reset 节点 4 拍」**：`build_register` 给寄存器的
    //   `rst_val` 是 **nullptr**（`include/core/node_builder.h`）⇒ ⇒ 解释器的
    //   `instr_reg::eval` 在复位期间**只 return、不赋值**（`src/ast/instr_reg.cpp`）
    //   ⇒ ⇒ 驱动 reset 节点**不产生**「回到 INIT」的效果，只白白消耗 4 拍
    //   ⇒ ⇒ 真正的「INIT」语义在 `init_val` 上（渲染侧同源）⇒ ⇒ 回填它才对。
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
        // 寄存器回到 INIT（渲染侧 `if (default_reset) reg <= <INIT>` 的同一终态）
        for (const auto &node_ptr : ctx->get_nodes()) {
            if (!node_ptr || node_ptr->type() != ch::core::lnodetype::type_reg) {
                continue;
            }
            auto *reg = static_cast<ch::core::regimpl *>(node_ptr.get());
            auto *init = reg->get_init_val();
            if (!init || init->type() != ch::core::lnodetype::type_lit) {
                continue;
            }
            const auto &lit_val = static_cast<const ch::core::litimpl *>(init)->value();
            sim.set_value_by_node_id(node_ptr->id(), static_cast<std::uint64_t>(lit_val));
        }
        // ⚠️ **复位尾缝对齐**（ADR-076 实施期发现）：渲染式 TB 的复位脉冲末尾还有
        //   **一个** `@(posedge clk)`（**无 `#1`**）⇒ ⇒ 那一拍里复位已释放
        //   ⇒ ⇒ DUT 的寄存器**真的走了一拍** ⇒ ⇒ 解释器 MUST 补这一拍（否则相位差 1）。
        sim.tick();
    }

    // ── 端口位宽表（ADR-076 D4 的「width > 64 ⇒ fail-closed」）────────────
    //
    // ⚠️ **与渲染器同源**：只用 `manifest.port_table[]`（按产物端口名）与
    //   `manifest.ports[]` 里 `is_top_io` 的条目（按 C++ 成员名）⇒ ⇒ 与
    //   `src/rtl_emit/verif_render.cpp` 的 `PortTable` **同一份口径** ⇒ ⇒
    //   两端对「宽端口」给出**同一结论**。
    // ⚠️ 解析器（`Resolver`）还有一条「后代端口兜底」⇒ ⇒ 该口径下**查不到**的端口
    //   在渲染侧也会 fail-closed ⇒ ⇒ 解释器侧同样 fail-closed（不静默放行）。
    std::unordered_map<std::string, std::uint32_t> width_by_name;
    std::unordered_map<std::string, std::uint32_t> width_by_member;
    for (const auto &p : manifest.port_table) {
        width_by_name.emplace(p.name, p.width);
    }
    for (const auto &e : manifest.ports) {
        if (!e.is_top_io || e.cpp_member.empty()) {
            continue;
        }
        if (e.cpp_member == kOpaque || e.cpp_member == kOpaqueOut) {
            continue;
        }
        width_by_member.emplace(e.cpp_member, e.width);
    }
    auto width_of = [&](const std::string &ident, std::uint32_t &out) -> bool {
        auto it = width_by_member.find(ident);
        if (it != width_by_member.end()) {
            out = it->second;
            return true;
        }
        it = width_by_name.find(ident);
        if (it != width_by_name.end()) {
            out = it->second;
            return true;
        }
        return false;
    };
    // 被历史/序列判据引用的端口 MUST 位宽 ≤ 64（否则历史值会被截成低 64 位 ⇒ ⇒
    // 两端「大概都截断」⇒ ⇒ 「大概同构」正是 ADR-074 D8 要消灭的 ⇒ ⇒ 直接拒）
    auto check_width_or_fail = [&](const std::string &port, const std::string &where) -> bool {
        std::uint32_t w = 0;
        if (!width_of(port, w)) {
            res.error = where + "：端口 `" + port + "` 不在产品端口表 ⇒ 无法确定位宽 ⇒ "
                                     "fail-closed";
            return false;
        }
        if (w > 64) {
            res.error = where + "：端口 `" + port + "` 的位宽是 " + std::to_string(w) +
                         "（> 64）⇒ ⇒ 历史值会被截成低 64 位 ⇒ ⇒ fail-closed";
            return false;
        }
        return true;
    };

    // ── 历史序列（ADR-076 D2）──
    // ⚠️ key = **端口标识符字符串** ⇒ ⇒ 与渲染侧的历史数组**同一分桶口径**
    //   ⇒ ⇒ 「一个物理端口只用一个标识符」是**规范义务**（否则两条独立历史）。
    std::unordered_map<std::string, std::vector<std::uint64_t>> history;

    for (const auto &sc : program.scenarios) {
        for (const auto &op : sc.ops) {
            switch (op.kind) {
            case VerifOpKind::Record: {
                if (!check_width_or_fail(op.port, sc.name + " · RECORD " + op.port)) {
                    return res;
                }
                std::uint32_t id = 0;
                if (!resolve_or_fail(op.port, id, sc.name + " · RECORD " + op.port)) {
                    return res;
                }
                const auto v = sim.get_value_by_node_id(id);
                if (!v) {
                    res.error = sc.name + " · RECORD " + op.port + "：node_id 未命中";
                    return res;
                }
                history[op.port].push_back(*v);
                break;
            }
            case VerifOpKind::ExpectSeqSelf:
            case VerifOpKind::ExpectSeqStep:
            case VerifOpKind::ExpectSeqCount: {
                if (!check_width_or_fail(op.port, sc.name + " · " + op.port)) {
                    return res;
                }
                if (op.kind == VerifOpKind::ExpectSeqStep &&
                    !check_width_or_fail(op.port2, sc.name + " · " + op.port2)) {
                    return res;
                }
                const auto it = history.find(op.port);
                if (it == history.end()) {
                    res.error = sc.name + " · " + op.port + "：历史序列为空";
                    return res;
                }
                const std::vector<std::uint64_t> &h = it->second;
                const std::size_t L = h.size();
                const std::size_t n = op.seq_n;
                bool ok = true;
                std::string detail;
                if (op.kind == VerifOpKind::ExpectSeqSelf) {
                    const std::size_t lag = op.seq_lag;
                    for (std::size_t i = 0; i < n; ++i) {
                        const std::uint64_t a = h[L - n + i];
                        const std::uint64_t b = h[L - n - lag + i];
                        if (a != b) {
                            ok = false;
                            detail = "idx " + std::to_string(i) + " got " +
                                     std::to_string(a) + " want " + std::to_string(b);
                            break;
                        }
                    }
                } else if (op.kind == VerifOpKind::ExpectSeqStep) {
                    const auto it2 = history.find(op.port2);
                    if (it2 == history.end()) {
                        res.error = sc.name + " · " + op.port2 + "：历史序列为空";
                        return res;
                    }
                    const std::vector<std::uint64_t> &he = it2->second;
                    const std::size_t LE = he.size();
                    for (std::size_t i = 0; i < n; ++i) {
                        // ⚠️ **对齐**：V 的第 i 个相邻差 = 「倒数第 (n-i) 项」−「倒数第 (n+1-i) 项」
                        //   ⇒ 下标 h[L-n+i] − h[L-n-1+i]（**不是** h[L-n+i+1] ⇒ ⇒ 那会越界）
                        // ⚠️ 差值按 **mod 2^64** 回绕（与渲染侧 `logic [63:0]` 的减法同语义）
                        const std::uint64_t diff = h[L - n + i] - h[L - n - 1 + i];
                        // ⚠️ 对应的 E 样本 = **同一个窗口的同位项**（同样按「距末尾」对齐）
                        //   ⇒ 下标 he[LE-n-1+i]（与 diff 的「较早端」同拍 ⇒ ⇒ 该端的 E
                        //   才是**造成这次跃迁**的那个值）
                        const std::uint64_t want = he[LE - n - 1 + i];
                        if (diff != want) {
                            ok = false;
                            detail = "idx " + std::to_string(i) + " got " +
                                     std::to_string(diff) + " want " + std::to_string(want);
                            break;
                        }
                    }
                } else { // ExpectSeqCount
                    std::uint32_t cnt = 0;
                    for (std::size_t i = 0; i < n; ++i) {
                        if (h[L - n + i] == op.seq_value) {
                            ++cnt;
                        }
                    }
                    if (cnt != op.seq_count) {
                        ok = false;
                        detail = "count " + std::to_string(cnt) + " want " +
                                 std::to_string(op.seq_count);
                    }
                }
                ++res.checks;
                if (!ok) {
                    ++res.fails;
                    res.check_lines.push_back("CHECK " + op.check_id + " FAIL");
                    res.failure_lines.push_back("CHECK " + op.check_id + " FAIL (" +
                                                op.port + " " + detail + ")");
                } else {
                    res.check_lines.push_back("CHECK " + op.check_id + " PASS");
                }
                break;
            }
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
            default:
                // ⚠️ **fail-closed**（ADR-076 AC9）：遇未知 kind ⇒ **绝不**静默跳过
                //   ⇒ ⇒ 否则「扫描器认得、解释器不认得」会变成「断言被悄悄丢掉」。
                res.error = sc.name + "：未知的原语种类（解释器拒绝执行）";
                return res;
            }
        }
    }
    res.ok = true;
    return res;
}

VerifRunFileResult run_verif_file(const std::string &verif_path, Component &root,
                                  core::context *ctx, const VerifRunOptions &options) {
    VerifRunFileResult out;
    std::ifstream in(verif_path);
    if (!in) {
        out.error = "打不开验证源：" + verif_path;
        return out;
    }
    VerifProgram prog;
    VerifParseError perr;
    if (!parse_verif(in, prog, perr)) {
        out.error = "验证源解析失败（" + verif_path + ":" + std::to_string(perr.line) +
                    "）：" + perr.message;
        return out;
    }
    const auto manifest = build_rtl_manifest(root, ctx);
    if (!manifest.diagnostics.empty()) {
        out.error = "manifest 诊断：" + manifest.diagnostics.front().detail;
        return out;
    }
    ch::Simulator sim(ctx);
    out.run = run_verif(prog, sim, ctx, manifest, options);
    if (!out.run.ok) {
        out.error = "执行失败：" + out.run.error;
        return out;
    }
    // 条数自洽：运行期条数 MUST == 静态条数（防「0 条断言被当绿」）
    const std::size_t expected = verif_check_count(prog);
    if (out.run.checks != expected) {
        out.error = "条数不等 —— 运行期 " + std::to_string(out.run.checks) + " ≠ 静态 " +
                    std::to_string(expected);
        return out;
    }
    out.ok = true;
    return out;
}

} // namespace rtl_emit
} // namespace ch
