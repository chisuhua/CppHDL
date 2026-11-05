// src/hierarchy_verilog.cpp
//
// 层次化 RTL 发射（见 include/hierarchy_verilog.h）。
//
// 分两步，**顺序不可颠倒**：
//   ① **校验**（分析层 + 本文件的额外检查）⇒ 任何失败都在**碰 I/O 之前**返回错误；
//   ② **发射**（每个 module 一次 `ModuleEmitter`）⇒ 全部成功后才 rename 落盘。
//
// ⚠️ 只读 IR：发射期 MUST NOT 改 `lnodeimpl`（ADR-060 §2.1）——
//    改 IR 会让 `toVerilog()` 的既有产物与仿真行为一起变。

#include "hierarchy_verilog.h"

#include "codegen_naming.h"
#include "codegen_verilog.h"
#include "ast/mem_port_impl.h"
#include "hierarchy_analysis.h"
#include "logger.h"

#include <cstdio>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <string>
#include <vector>

namespace ch {
namespace {

using ch::core::lnodeimpl;
using ch::core::lnodetype;

bool is_sync_mem_read(const lnodeimpl *n) {
    if (n->type() != lnodetype::type_mem_read_port) {
        return false;
    }
    const auto *p = static_cast<const ch::core::mem_read_port_impl *>(n);
    return p->port_type() == ch::core::mem_port_type::sync_read;
}

bool is_mem_write(const lnodeimpl *n) {
    return n->type() == lnodetype::type_mem_write_port;
}

/// 该 module 是否需要 clock / reset 端口（**两者分开判** —— ADR-060 §2.1 v1.4）：
///   clock ⇒ 含 `ch_reg` ∨ mem 写端口 ∨ **同步**读端口；
///   reset ⇒ 含 `ch_reg` ∨ **同步**读端口（mem 写端口的 `always_ff` **没有** reset 分支）。
struct SeqNeeds {
    bool clock = false;
    bool reset = false;
};

SeqNeeds seq_needs(const HierarchyModule &m) {
    SeqNeeds s;
    for (const auto &b : m.body) {
        const auto ty = b.node->type();
        if (ty == lnodetype::type_reg) {
            s.clock = true;
            s.reset = true;
        } else if (is_mem_write(b.node)) {
            s.clock = true;
        } else if (is_sync_mem_read(b.node)) {
            s.clock = true;
            s.reset = true;
        }
    }
    return s;
}

/// 名称后缀：`<type>` / `<type>_1` / `<type>_2` …（同类型多实例**各发一份**，ADR-062 §2.6）
std::string numbered_name(const std::string &base, int seq) {
    if (seq == 0) {
        return base;
    }
    return base + "_" + std::to_string(seq);
}

std::string emit_instance_statement(const HierarchyInstance &inst,
                                    const std::string &child_module_name,
                                    const std::vector<HierarchyConnection> &conns) {
    std::ostringstream os;
    os << "    " << child_module_name << " " << inst.instance_name << " (";
    for (std::size_t i = 0; i < conns.size(); ++i) {
        const auto &c = conns[i];
        os << (i == 0 ? "\n" : ",\n");
        os << "        ." << c.child_port << "(" << c.parent_side << ")";
    }
    os << "\n    );";
    return os.str();
}

}   // namespace

HierarchyEmitResult toVerilogHierarchical(const std::string &out_path, Component &root,
                                          const HierarchyEmitOptions &options) {
    HierarchyEmitResult result;

    // ④ 静态析构期兜底：不抛、不写盘，用**返回码**呈现失败
    if (ch::detail::in_static_destruction()) {
        result.error = "hierarchical emission refused during static destruction";
        return result;
    }

    // ── ① 校验（**全部**在第一次 I/O 之前；本段不接受任何「写了一半」的状态）
    HierarchyAnalysis analysis;
    try {
        // ⚠️ `Record` ⇒ 「需要隐式端口合成」的边**不抛** ⇒ ⇒ 交给下面的 collect
        //    （`Reject` 模式仍是分析层的**默认**行为 ⇒ ⇒ T3-② 的 fail-closed 契约不变）
        analysis = analyze_hierarchy(root, ImplicitPortHandling::Record);
    } catch (const std::exception &e) {
        // ⚠️ 这里是**转换**成可观测错误，不是吞掉（既有的 `toVerilog` 才是吞掉）
        result.error = e.what();
        return result;
    }
    result.warnings = analysis.warnings;

    if (analysis.modules.empty()) {
        result.error = "hierarchical emission: analysis produced no modules";
        return result;
    }

    // ── 命名表（**提前**建：合成端口的 `assign` 与父侧实参都要用它）
    //    （纯读 ⇒ ⇒ 无副作用 ⇒ ⇒ 提前不影响任何既有行为）
    const auto names = build_verilog_name_table(root.context()->get_eval_list());

    // ── **行 2（子 → 父）的隐式端口合成**（ADR-060 §2.1 行 2）────────────
    //    子侧端点**不是**子端口 ⇒ ⇒ 在**子侧**合成 **output** 端口 `__x_<n>`
    //    ⇒ ⇒ 子体内 `assign __x_<n> = <子侧节点>;`
    //    ⇒ ⇒ 父侧实例端口映射 `. __x_<n>(<父侧端点>)`
    struct SynthPortInfo {
        std::string port;
        std::string node;        ///< output 端口：子侧节点名（体内 `assign` 的 RHS）
        std::uint32_t width = 0;
        bool is_input = false;   ///< `true` = 行 1（input 端口 · 值由父层提供）
        /// 行 1：被重定向的节点（**寄存器**）⇒ ⇒ 发射期改由合成端口驱动
        const lnodeimpl *redirect = nullptr;
    };
    /// 子 module key → 它要发的合成端口（⇒ ⇒ 端口声明 + 体内 assign）
    std::map<std::string, std::vector<SynthPortInfo>> synth_by_child;
    /// (父 module key, 子实例名) → 追加到该实例的连接
    std::map<std::string, std::vector<HierarchyConnection>> synth_conns;
    /// 子 module key → 端口序号（确定性 ⇒ ⇒ 同名不同义）
    std::map<std::string, int> synth_seq;
    /// 子 module key → (子侧节点 → 端口名)（**去重**：同一节点只合成一个端口）
    std::map<std::string, std::map<const lnodeimpl *, std::string>> synth_port_of;
    /// 已被用户节点占用的名字（⚠️ 合成名 MUST NOT 与之撞名 —— 否则**重复声明**；
    /// 默认 warning 集下只是响亮失败，但属**可预防**缺陷 · ADR-060 v1.9 §2.2）
    std::set<std::string> taken_names;
    for (const auto &[nptr, nname] : names) {
        (void)nptr;
        taken_names.insert(nname);
    }
    /// 生成一个**未占用**的合成名：`<prefix><seq>`；冲突则顺延 `_N`
    auto make_synth_name = [&taken_names](const std::string &prefix, int &seq) {
        std::string candidate;
        do {
            candidate = prefix + std::to_string(seq++);
        } while (taken_names.count(candidate) != 0);
        taken_names.insert(candidate);
        return candidate;
    };
    /// **不可合成**的合成类问题（⇒ ⇒ 交给下面的 collect ⇒ ⇒ 按策略处置）
    std::vector<std::string> remaining;

    for (const auto &e : analysis.pending_implicit_edges) {
        const std::string desc =
            "'" + e.from_node + "'@'" + e.from_module + "' -> '" + e.to_node + "'@'" +
            e.to_module + "' (" + e.reason + ")";
        const bool has_nodes = (e.from_ptr != nullptr && e.to_ptr != nullptr &&
                                !e.child_instance_name.empty() &&
                                names.count(e.from_ptr) != 0 &&
                                names.count(e.to_ptr) != 0);
        if (!has_nodes) {
            remaining.push_back(desc);
            continue;
        }
        // ⚠️ **两个方向的合成条件不同**：
        //    · 行 2（子 → 父）⇒ ⇒ 子侧 = **驱动端**所在 module · 父侧 = 被驱动端所在 module
        //    · 行 1（父 → 子）⇒ ⇒ 子侧 = **被驱动端**所在 module · 父侧 = 驱动端所在 module
        if (e.child_drives_parent) {
            // ── 行 2：子侧合成 **output** 端口 `__x_<n>`（父层取它的值）──
            const std::string &child_key = e.from_module;
            const std::string &parent_key = e.to_module;
            auto &port_map = synth_port_of[child_key];
            auto it = port_map.find(e.from_ptr);
            if (it == port_map.end()) {
                const std::string port =
                    make_synth_name("__x_", synth_seq[child_key]);
                port_map[e.from_ptr] = port;
                synth_by_child[child_key].push_back(SynthPortInfo{
                    port, names.at(e.from_ptr), e.from_ptr->size(),
                    /*is_input=*/false, /*redirect=*/nullptr});
                it = port_map.find(e.from_ptr);
            }
            synth_conns[parent_key + "\x1f" + e.child_instance_name].push_back(
                HierarchyConnection{it->second, names.at(e.to_ptr), /*into_child=*/false});
        } else {
            // ── 行 1：子侧合成 **input** 端口 `__if_<n>`（父层驱动它）──
            //    ⚠️ 本批**只**覆盖「子侧端点是**寄存器**」+「父侧驱动节点在父 module 内」
            //    ⇒ ⇒ 其余（proxy/op 源 · 第三轴）⇒ ⇒ 仍 fail-closed（**B2b-3**）
            // ⚠️ **可合成的子侧端点 = 有独立 driver 槽位的节点**（ADR-060 v1.9 §2.1 行 1）：
            //    ① `type_reg`（`next` 槽位 · `print_reg` 重定向 · B2b-2）
            //    ② `type_proxy`（bundle 字段 · `print_proxy` 重定向 · 本批）
            //    ⚠️ **MUST NOT** 收 `type_output`（第八格：父驱动子 `ch_out` ⇒ 拒绝）
            //    ⚠️ **MUST NOT** 收 `type_op` / 其它（第七格：改写运算操作数 ⇒ 拒绝）
            const auto child_type = e.to_ptr->type();
            const bool child_is_redirectable =
                (child_type == lnodetype::type_reg ||
                 child_type == lnodetype::type_proxy);
            const auto drv_mod = analysis.module_of_node.find(e.from_ptr);
            const bool driver_in_parent =
                drv_mod != analysis.module_of_node.end() && drv_mod->second == e.from_module;
            if (!child_is_redirectable || !driver_in_parent) {
                remaining.push_back(desc);
                continue;
            }
            const std::string &child_key = e.to_module;    // 子侧 = 被驱动端所在 module
            const std::string &parent_key = e.from_module;  // 父侧 = 驱动端所在 module
            auto &port_map = synth_port_of[child_key];
            auto it = port_map.find(e.to_ptr);
            if (it == port_map.end()) {
                const std::string port =
                    make_synth_name("__if_", synth_seq[child_key]);
                port_map[e.to_ptr] = port;
                synth_by_child[child_key].push_back(SynthPortInfo{
                    port, /*node=*/std::string(), e.to_ptr->size(),
                    /*is_input=*/true, /*redirect=*/e.to_ptr});
                it = port_map.find(e.to_ptr);
            }
            synth_conns[parent_key + "\x1f" + e.child_instance_name].push_back(
                HierarchyConnection{it->second, names.at(e.from_ptr), /*into_child=*/true});
        }
    }

    // ── 合成类诊断的 **collect**（ADR-060 §2.4.1）────────────────────────
    //    ① 分析层记录的「需要隐式端口合成」的边
    //    ② 发射器自己的检查：子 → 父 的**父侧端点** MUST 是父 module 的 output 端口
    //       （否则父层需要一根「连接网」⇒ ⇒ 同样是隐式端口合成的义务）
    //    ⇒ ⇒ **全部**收集 ⇒ ⇒ 再按 `PortPolicy` 决定诊断广度（不是「首错即返」）
    {
        std::vector<std::string> synth_diags = remaining;
        for (const auto &m : analysis.modules) {
            for (const auto &inst : m.instances) {
                for (const auto &conn : inst.connections) {
                    if (conn.into_child) {
                        continue;
                    }
                    bool is_parent_output_port = false;
                    for (const auto &pp : m.ports) {
                        if (pp.name == conn.parent_side && !pp.is_input) {
                            is_parent_output_port = true;
                            break;
                        }
                    }
                    if (!is_parent_output_port) {
                        synth_diags.push_back(
                            "child module '" + inst.module_key + "' drives '" +
                            conn.parent_side + "' in module '" + m.key +
                            "', which is not an output port of that module");
                    }
                }
            }
        }
        if (!synth_diags.empty()) {
            // ⚠️ **中间态**：隐式端口**合成**属 B2b ⇒ ⇒ 本批一律 fail-closed
            //    ⇒ ⇒ 差异**只在诊断广度**（`Reject` 只报第一条 · 其余报全部）
            const bool all = (options.port_policy != PortPolicy::Reject);
            std::ostringstream msg;
            msg << "hierarchical emission: ";
            if (all) {
                msg << synth_diags.size() << " cross-module edge(s) require implicit-port "
                    << "synthesis (ADR-060 §2.1), which is not implemented yet:";
                for (std::size_t i = 0; i < synth_diags.size(); ++i) {
                    msg << "\n  [" << (i + 1) << "] " << synth_diags[i];
                }
            } else {
                msg << "a cross-module edge requires implicit-port synthesis (ADR-060 "
                    << "§2.1), which is not implemented yet:\n  [1] " << synth_diags.front();
            }
            result.error = msg.str();
            if (all) {
                // `SynthesizeWarn` ⇒ 每一条一条 `CHWARN`（诊断 MUST 逐条点名）
                for (const auto &d : synth_diags) {
                    CHWARN("hierarchical emission: implicit-port synthesis needed for %s",
                           d.c_str());
                }
            }
            return result;
        }
    }

    // module 类型名（`<module_type_name>_<序号>`；同类型多实例各发一份）
    std::map<std::string, int> type_seq;
    std::vector<std::string> module_names;
    module_names.reserve(analysis.modules.size());
    for (const auto &m : analysis.modules) {
        const std::string base = sanitize_verilog_name(m.module_type_name);
        module_names.push_back(numbered_name(base, type_seq[base]++));
    }
    // 同名不同类型 ⇒ 响亮拒绝（MUST NOT 静默合并）
    for (std::size_t i = 0; i < module_names.size(); ++i) {
        for (std::size_t j = i + 1; j < module_names.size(); ++j) {
            if (module_names[i] == module_names[j]) {
                result.error = "hierarchical emission: two different modules share the "
                               "generated name '" + module_names[i] + "'";
                return result;
            }
        }
    }

    // module key → 生成名（实例化语句用）
    std::map<std::string, std::string> name_of_key;
    // module key → 该 module 的端口表（实例化时要知道子 module 有哪些 output 端口）
    std::map<std::string, const HierarchyModule *> module_by_key;
    // module key → 时钟/复位需求（自底向上累积 ⇒ 需求**向上传导**）
    std::map<std::string, SeqNeeds> needs_of;
    for (std::size_t i = 0; i < analysis.modules.size(); ++i) {
        name_of_key[analysis.modules[i].key] = module_names[i];
        module_by_key[analysis.modules[i].key] = &analysis.modules[i];
    }

    // 根 module 用 options.top_module_name（与扁平产物同名 ⇒ 便于 A/B 比对）
    for (std::size_t i = 0; i < analysis.modules.size(); ++i) {
        if (analysis.modules[i].is_root) {
            module_names[i] = options.top_module_name;
            name_of_key[analysis.modules[i].key] = options.top_module_name;
        }
    }

    // ── ② 发射（纯内存 ⇒ 任何异常都不会留下半套文件）
    std::ostringstream all;

    for (std::size_t mi = 0; mi < analysis.modules.size(); ++mi) {
        const auto &m = analysis.modules[mi];

        ModuleView view;
        view.module_name = module_names[mi];
        view.names = names;

        // 端口：clock/reset 先（分模块判），再其余（分析层已排好序）
        // ⚠️ **时钟需求向上传导**（边界情形 #22）：子 module 要 clock ⇒ 父 module 也要
        //    （否则父层没法把 clock 传下去 ⇒ 实例化时缺 pin）
        SeqNeeds needs = m.is_root ? SeqNeeds{true, true} : seq_needs(m);
        for (const auto &inst : m.instances) {
            const auto it = needs_of.find(inst.module_key);
            if (it == needs_of.end()) {
                continue;
            }
            needs.clock = needs.clock || it->second.clock;
            needs.reset = needs.reset || it->second.reset;
        }
        needs_of[m.key] = needs;
        auto push_clock_reset = [&](lnodetype ty) {
            for (const auto &kv : names) {
                if (kv.first->type() != ty) {
                    continue;
                }
                view.ports.push_back(const_cast<lnodeimpl *>(kv.first));
                if (ty == lnodetype::type_clock) {
                    view.clock_name = kv.second;
                } else {
                    view.reset_name = kv.second;
                }
                return true;
            }
            return false;
        };
        const bool has_clock = needs.clock && push_clock_reset(lnodetype::type_clock);
        const bool has_reset = needs.reset && push_clock_reset(lnodetype::type_reset);
        if (needs.clock && !has_clock) {
            result.error = "hierarchical emission: module '" + m.key +
                           "' needs a clock port but the context has no clock node";
            return result;
        }
        if (needs.reset && !has_reset) {
            result.error = "hierarchical emission: module '" + m.key +
                           "' needs a reset port but the context has no reset node";
            return result;
        }
        // ⚠️ 根 module 的 clock/reset 已由分析层放进端口表 ⇒ 跳过重复
        for (const auto &p : m.ports) {
            auto *node = const_cast<lnodeimpl *>(p.node);
            if (!m.is_root && (node->type() == lnodetype::type_clock ||
                               node->type() == lnodetype::type_reset)) {
                continue;
            }
            if (m.is_root && (node->type() == lnodetype::type_clock ||
                              node->type() == lnodetype::type_reset)) {
                continue;   // 已在上面 push
            }
            view.ports.push_back(node);
        }

        // 端口排序：clock → reset → input → output
        // ⚠️ **根 module 必须与扁平产物逐字逐序相同**（扁平 `print_header` 的顺序就是
        //    clock → reset → 无 driver 的 input → 全部 ch_out）⇒ 这里用同一套分组。
        std::stable_sort(view.ports.begin(), view.ports.end(),
                         [](const lnodeimpl *a, const lnodeimpl *b) {
                             auto rank = [](const lnodeimpl *n) {
                                 switch (n->type()) {
                                 case lnodetype::type_clock:
                                     return 0;
                                 case lnodetype::type_reset:
                                     return 1;
                                 case lnodetype::type_input:
                                     return 2;
                                 case lnodetype::type_output:
                                     return 3;
                                 default:
                                     return 4;
                                 }
                             };
                             return rank(a) < rank(b);
                         });

        // 体内节点：body + 端口节点（保证声明 / 逻辑都被发到）
        view.nodes = m.body.empty() ? std::vector<lnodeimpl *>{}
                                    : std::vector<lnodeimpl *>{};
        for (const auto &b : m.body) {
            view.nodes.push_back(const_cast<lnodeimpl *>(b.node));
        }
        for (auto *p : view.ports) {
            view.nodes.push_back(p);
        }
        // ⚠️ 遍历顺序 MUST 与扁平一致（= ctx 拓扑序）⇒ 声明与 assign 的顺序逐字相同
        {
            std::unordered_map<const lnodeimpl *, std::size_t> rank;
            const auto eval_order = root.context()->get_eval_list();
            for (std::size_t i = 0; i < eval_order.size(); ++i) {
                rank[eval_order[i]] = i;
            }
            std::stable_sort(view.nodes.begin(), view.nodes.end(),
                             [&rank](const lnodeimpl *a, const lnodeimpl *b) {
                                 auto ia = rank.find(a);
                                 auto ib = rank.find(b);
                                 const std::size_t ra =
                                     ia == rank.end() ? 0 : ia->second;
                                 const std::size_t rb =
                                     ib == rank.end() ? 0 : ib->second;
                                 return ra < rb;
                             });
        }

        // ── 跨 module 的驱动边 ⇒ 本 module **不发**那条 assign（值由实例端口映射提供）
        {
            std::unordered_set<const lnodeimpl *> suppress;
            for (auto *n : view.nodes) {
                if (n->num_srcs() == 0) {
                    continue;
                }
                auto *drv = n->src(0);
                const auto it_self = analysis.module_of_node.find(n);
                const auto it_drv = analysis.module_of_node.find(drv);
                if (it_self != analysis.module_of_node.end() &&
                    it_drv != analysis.module_of_node.end() &&
                    it_self->second != it_drv->second) {
                    suppress.insert(n);
                }
            }
            view.suppress_driver_assign = std::move(suppress);
        }

        // ⚠️ 「子 → 父的父侧端点 MUST 是父 module 的 output 端口」这条检查
        //    **已上移**到发射前的 collect 预检（⇒ ⇒ 全部合成类问题**一起**报告，
        //    而不是「首错即返」；⇒ ⇒ 也避免两份判定漂移）。

        // ── **合成端口**（隐式端口 · ADR-060 §2.1 行 2）────────────────
        //    端口声明走 `synthesized_ports`（**不是 IR 节点** ⇒ ⇒ 单列）
        //    体内的 `assign __x_<n> = <子侧节点>;` 走 `body_epilogue`
        {
            const auto it = synth_by_child.find(m.key);
            if (it != synth_by_child.end()) {
                for (const auto &sp : it->second) {
                    view.synthesized_ports.push_back(
                        SynthesizedPort{sp.port, sp.is_input, sp.width});
                    if (sp.is_input) {
                        // ⚠️ input 端口：值由**父层**提供 ⇒ ⇒ 本 module 内**不**发 assign
                        //    ⇒ ⇒ 改由**驱动重定向**把被驱动的节点接到该端口
                        //    （⇒ ⇒ 发射期虚拟重定向 ⇒ ⇒ MUST NOT 写回 IR）
                        if (sp.redirect != nullptr) {
                            view.port_drives_node[sp.redirect] = sp.port;
                        }
                    } else {
                        view.body_epilogue.push_back("    assign " + sp.port + " = " +
                                                      sp.node + ";");
                    }
                }
            }
        }

        // 实例化语句
        for (const auto &inst : m.instances) {
            const auto it = name_of_key.find(inst.module_key);
            if (it == name_of_key.end()) {
                result.error = "hierarchical emission: instance '" + inst.instance_name +
                               "' points at unknown module '" + inst.module_key + "'";
                return result;
            }
            // ⚠️ 子 module 若有 clock/reset 端口 ⇒ 实例化 MUST 连上（否则 PINMISSING）
            auto conns = inst.connections;
            const auto child_needs = needs_of.find(inst.module_key);
            if (child_needs != needs_of.end()) {
                if (child_needs->second.clock) {
                    conns.push_back(HierarchyConnection{"default_clock",
                                                        view.clock_name, true});
                }
                if (child_needs->second.reset) {
                    conns.push_back(HierarchyConnection{"default_reset",
                                                        view.reset_name, true});
                }
            }
            // ⚠️ **未被子层消费的 output 端口 MUST 以空连接出现**（`.port()`）
            //    （ADR-061 决策② ②：成层组件的 `ch_out` **即使无人消费**也是端口
            //      ⇒ ⇒ 父层若不接 ⇒ ⇒ Verilator 报 `PINMISSING` ⇒ ⇒ lint 失败）
            {
                const auto child_mod = module_by_key.find(inst.module_key);
                if (child_mod != module_by_key.end()) {
                    for (const auto &cp : child_mod->second->ports) {
                        if (cp.is_input) {
                            continue;   // 子 input：未驱动者 MUST 冒泡（B3）⇒ ⇒ 这里不接
                        }
                        bool already = false;
                        for (const auto &c : conns) {
                            if (c.child_port == cp.name) {
                                already = true;
                                break;
                            }
                        }
                        if (!already) {
                            conns.push_back(HierarchyConnection{cp.name, "", false});
                        }
                    }
                }
            }
            // ⚠️ **合成端口的连接**（行 2：子 output → 父侧端点）
            {
                const auto sc = synth_conns.find(m.key + "\x1f" + inst.instance_name);
                if (sc != synth_conns.end()) {
                    conns.insert(conns.end(), sc->second.begin(), sc->second.end());
                }
            }
            view.body_epilogue.push_back(
                emit_instance_statement(inst, it->second, conns));
        }

        std::ostringstream one;
        ModuleEmitter emitter(root.context(), view);
        emitter.print(one);
        if (!all.str().empty()) {
            all << "\n";
        }
        all << "// ---- module: " << m.key << " (" << view.module_name << ") ----\n";
        all << one.str();
    }

    const std::string text = all.str();

    // ── ③ 落盘：临时文件 → 成功后 rename（失败不留半套产物）
    const std::string tmp_path = out_path + ".tmp";
    {
        std::ofstream out(tmp_path, std::ios::binary);
        if (!out.good()) {
            result.error = "hierarchical emission: cannot open '" + tmp_path + "'";
            return result;
        }
        out << text;
        out.flush();
        if (!out.good()) {
            out.close();
            std::remove(tmp_path.c_str());
            result.error = "hierarchical emission: write failed for '" + tmp_path + "'";
            return result;
        }
    }
    if (std::rename(tmp_path.c_str(), out_path.c_str()) != 0) {
        std::remove(tmp_path.c_str());
        result.error = "hierarchical emission: rename '" + tmp_path + "' -> '" + out_path +
                       "' failed";
        return result;
    }

    result.ok = true;
    result.files.push_back(out_path);
    return result;
}

}   // namespace ch
