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
    /// 共同祖先 module 内的**中转线**（兄弟边 · design §7.2.3）
    std::map<std::string, std::vector<SynthesizedPort>> synth_wires;

    // ── module 树 + 端口表索引（合成**之前**要建的只读索引）
    std::map<std::string, const HierarchyModule *> module_by_key;
    for (const auto &m : analysis.modules) {
        module_by_key[m.key] = &m;
    }
    /// 子 module key → (父 module key, 父 module 内的实例名)
    std::map<std::string, std::pair<std::string, std::string>> parent_of;
    /// `"<parent>\x1f<inst>"` → 该实例（⇒ ⇒ 查**已有**连接，防双驱动）
    std::map<std::string, const HierarchyInstance *> inst_of;
    for (const auto &m : analysis.modules) {
        for (const auto &inst : m.instances) {
            parent_of[inst.module_key] = {m.key, inst.instance_name};
            inst_of[m.key + "\x1f" + inst.instance_name] = &inst;
        }
    }
    /// `anc` 是 `desc` 的**严格**祖先（沿 module 树向上走）
    auto is_strict_ancestor = [&parent_of](const std::string &anc, const std::string &desc) {
        for (std::string cur = desc;;) {
            const auto it = parent_of.find(cur);
            if (it == parent_of.end()) {
                return false;
            }
            if (it->second.first == anc) {
                return true;
            }
            cur = it->second.first;
        }
    };
    /// 该节点是否是 module 的端口（`is_input` 带出方向）
    auto module_port_of = [&module_by_key](const std::string &mk, const lnodeimpl *n,
                                           bool &is_input) -> bool {
        const auto it = module_by_key.find(mk);
        if (it == module_by_key.end()) {
            return false;
        }
        for (const auto &p : it->second->ports) {
            if (p.node == n) {
                is_input = p.is_input;
                return true;
            }
        }
        return false;
    };

    // ── **chain builder**（ADR-060 v1.12 §2.3 的 G0）──────────────────────
    //    冒泡（向上 · B3）与第三轴（向下 · 本批）**共用**这一条链：沿祖先链
    //    **每级一个同名端口**（名字 = 源节点的 ctx 级全局名 ⇒ 顶层与扁平产物逐字相同）。
    //    叶端点由**两种 leaf adapter** 决定（⇒ ⇒ 这正是「起点节点 + 方向」抽象不够用的地方）：
    //      · **port adapter**：叶端点本身是 module 的端口 ⇒ ⇒ 叶上**不**合成端口，直接接它；
    //      · **node adapter**：叶端点不是端口（reg / proxy）⇒ ⇒ 叶上合成端口 + **驱动重定向**。
    //    ⚠️ **幂等**：同 (module, 端口名) 重复请求 ⇒ ⇒ 复用；**撞名 / 方向或宽度不一致 ⇒ fail-closed**。
    auto add_pass_port = [&](const std::string &module_key, const std::string &name,
                             std::uint32_t width, bool is_input, const lnodeimpl *redirect,
                             std::string &err) -> bool {
        if (name.empty()) {
            err = "pass-through port with an empty name";
            return false;
        }
        // ① 撞**真实**端口名 ⇒ 失败（ADR-060 §2.3 的 fail-closed）
        const auto mod_it = module_by_key.find(module_key);
        if (mod_it != module_by_key.end()) {
            for (const auto &p : mod_it->second->ports) {
                if (p.name == name) {
                    err = "pass-through port '" + name + "' collides with an existing port of "
                          "module '" + module_key + "'";
                    return false;
                }
            }
        }
        // ② 幂等复用 / 冲突检测
        auto &ports = synth_by_child[module_key];
        for (auto &sp : ports) {
            if (sp.port != name) {
                continue;
            }
            if (sp.is_input != is_input || sp.width != width) {
                err = "pass-through port '" + name + "' on module '" + module_key +
                      "' already exists with a different direction/width";
                return false;
            }
            if (redirect != nullptr) {
                sp.redirect = redirect;
            }
            return true;
        }
        ports.push_back(SynthPortInfo{name, /*node=*/std::string(), width, is_input, redirect});
        taken_names.insert(name);
        return true;
    };
    /// 追加一条**合成连接**（幂等 + 双驱动 fail-closed）。
    /// ⚠️ 同一子端口出现两次映射 ⇒ Verilog `Duplicate pin connection` ⇒ ⇒ 必须在这里拦住；
    ///    与**已有**的分析层连接指向不同实参 ⇒ ⇒ 是**双驱动**（IR 里 `set_src` 是 last-wins
    ///    ⇒ ⇒ 语义只会丢掉一个驱动）⇒ ⇒ 响亮拒绝，不静默择一。
    auto add_conn = [&](const std::string &parent_mod, const std::string &inst_name,
                        const std::string &child_port, const std::string &arg, bool into_child,
                        std::string &err) -> bool {
        const std::string key = parent_mod + "\x1f" + inst_name;
        const auto iit = inst_of.find(key);
        if (iit != inst_of.end()) {
            for (const auto &c : iit->second->connections) {
                if (c.child_port == child_port && c.parent_side != arg) {
                    err = "child port '" + child_port + "' is already driven by '" +
                          c.parent_side + "' in module '" + parent_mod +
                          "' (the synthesized chain would drive it from '" + arg + "')";
                    return false;
                }
            }
        }
        auto &conns = synth_conns[key];
        for (auto &c : conns) {
            if (c.child_port != child_port) {
                continue;
            }
            if (c.parent_side != arg) {
                err = "child port '" + child_port + "' on instance '" + inst_name +
                      "' of module '" + parent_mod + "' is synthesized twice with different drivers";
                return false;
            }
            return true;   // 幂等
        }
        conns.push_back(HierarchyConnection{child_port, arg, into_child});
        return true;
    };
    /// 链走一遍：`path` = 驱动端 module **之下**的 module 链（末项 = 叶 module）。
    /// 返回 `ok == false` ⇒ ⇒ 该边**不可合成**（由调用方放进 `remaining`）。
    auto build_chain = [&](const std::vector<std::string> &path, const std::string &port_name,
                           std::uint32_t width, bool ports_are_input, bool leaf_is_port,
                           const std::string &leaf_port_name, const lnodeimpl *leaf_redirect,
                           const std::string &driver_arg, std::string &err) -> bool {
        if (path.empty()) {
            err = "empty pass-through chain";
            return false;
        }
        for (std::size_t i = 0; i < path.size(); ++i) {
            const std::string &child_mod = path[i];
            const auto pit = parent_of.find(child_mod);
            if (pit == parent_of.end()) {
                err = "module '" + child_mod + "' has no parent instance";
                return false;
            }
            const std::string &parent_mod = pit->second.first;
            const std::string &inst_name = pit->second.second;
            const bool is_leaf = (i + 1 == path.size());
            // ① 逐级同名端口（叶端点本身是端口时不合成）
            if (!is_leaf || !leaf_is_port) {
                if (!add_pass_port(child_mod, port_name, width, ports_are_input,
                                   is_leaf ? leaf_redirect : nullptr, err)) {
                    return false;
                }
            }
            // ② 该跳的实参：第一跳用驱动端 module 内的信号名，其后用上一级同名端口
            const std::string arg = (i == 0) ? driver_arg : port_name;
            const std::string child_port =
                (is_leaf && leaf_is_port) ? leaf_port_name : port_name;
            if (!add_conn(parent_mod, inst_name, child_port, arg, ports_are_input, err)) {
                return false;
            }
        }
        return true;
    };

    for (const auto &e : analysis.pending_implicit_edges) {
        const std::string desc =
            "'" + e.from_node + "'@'" + e.from_module + "' -> '" + e.to_node + "'@'" +
            e.to_module + "' (" + e.reason + ")";
        // ⚠️ **节点**是合成的必要条件；`child_instance_name` 只在**直接父子**边上有意义
        //    （非相邻边恒为空 ⇒ ⇒ MUST NOT 拿它当通用前置 ⇒ ⇒ 否则第三轴/兄弟边会被误判）
        const bool has_nodes = (e.from_ptr != nullptr && e.to_ptr != nullptr &&
                                names.count(e.from_ptr) != 0 &&
                                names.count(e.to_ptr) != 0);
        if (!has_nodes) {
            remaining.push_back(desc);
            continue;
        }
        // ⚠️ **方向由 module 树判定**（MUST NOT 只靠 `child_drives_parent` ——
        //    该标志只在**直接父子**关系下有意义，非相邻边恒为 false ⇒ ⇒
        //    无法区分「第三轴」与「兄弟边」）
        const bool driver_is_anc = is_strict_ancestor(e.from_module, e.to_module);
        const bool target_is_anc = is_strict_ancestor(e.to_module, e.from_module);
        const auto p_to = parent_of.find(e.to_module);
        const auto p_from = parent_of.find(e.from_module);
        const bool direct_row1 =
            driver_is_anc && p_to != parent_of.end() && p_to->second.first == e.from_module;
        const bool direct_row2 =
            target_is_anc && p_from != parent_of.end() && p_from->second.first == e.to_module;

        // ── **非直接父子**的三种形态（ADR-060 §2.1 第三轴 · §2.2 冒泡 · 兄弟边）
        if (!direct_row1 && !direct_row2) {
            // ⚠️ **W1**（ADR-060 §1.1）：合成端口宽度只能取**子侧** ⇒ 两端宽度不同 ⇒
            //    Verilog **静默**扩展/截断（产物关掉了 `WIDTHTRUNC`）⇒ ⇒ MUST 响亮拒绝
            const bool width_ok = (e.from_ptr->size() == e.to_ptr->size());
            const std::string width_note =
                width_ok ? std::string()
                         : (": width mismatch (" + std::to_string(e.from_ptr->size()) + " vs " +
                            std::to_string(e.to_ptr->size()) + ")");

            if (driver_is_anc) {
                // ── **第三轴**（driver 的 owner 在更上层）⇒ ⇒ 沿祖先链逐级同名 **input** 端口
                if (!width_ok) {
                    remaining.push_back(desc + width_note);
                    continue;
                }
                bool leaf_port_is_input = false;
                const bool leaf_is_port =
                    module_port_of(e.to_module, e.to_ptr, leaf_port_is_input);
                if (leaf_is_port && !leaf_port_is_input) {
                    // 叶端点是**输出**端口 ⇒ 父层驱动子 `ch_out` ⇒ 第八格 ⇒ 拒绝
                    remaining.push_back(desc +
                                        " (third-axis leaf is an output port; ADR-060 §2.1 forbids "
                                        "driving a child's ch_out)");
                    continue;
                }
                if (!leaf_is_port) {
                    // node adapter：只接受有**独立 driver 槽位**的节点（reg / proxy）
                    const auto ty = e.to_ptr->type();
                    if (ty != lnodetype::type_reg && ty != lnodetype::type_proxy) {
                        remaining.push_back(
                            desc + " (third-axis leaf is neither a port nor a redirectable "
                                   "reg/proxy node)");
                        continue;
                    }
                }
                // 路径 = 从被驱动端 module 向上走到驱动端 module 的**直接子**
                std::vector<std::string> path;
                for (std::string cur = e.to_module;;) {
                    path.push_back(cur);
                    const auto it = parent_of.find(cur);
                    if (it == parent_of.end() || it->second.first == e.from_module) {
                        break;
                    }
                    cur = it->second.first;
                }
                std::reverse(path.begin(), path.end());
                std::string err;
                if (!build_chain(path, names.at(e.from_ptr), e.from_ptr->size(),
                                 /*ports_are_input=*/true, leaf_is_port,
                                 leaf_is_port ? names.at(e.to_ptr) : std::string(),
                                 leaf_is_port ? nullptr : e.to_ptr, names.at(e.from_ptr), err)) {
                    remaining.push_back(desc + " (" + err + ")");
                }
                continue;
            }

            if (target_is_anc) {
                // ── **向上 pass-through**（子 → 父但非直接父子）⇒ ⇒ 与冒泡**同一套链**
                //    ⇒ ⇒ 属 **B3**（本批 fail-closed）
                remaining.push_back(desc +
                                    " (upward pass-through is part of bubbling synthesis, B3)");
                continue;
            }

            // ── **兄弟边**（design §7.2.3）：共同祖先 module 内一根**中转线**
            //    支持的形态（其余 ⇒ fail-closed；跨层的向上/向下链与 B3 同批）：
            //      · 两端是**同一个**父 module 的直接子（表亲边 ⇒ 需要两条链 ⇒ 本批不做）；
            //      · 驱动端是**子 module 的 output 端口**、被驱动端是**子 module 的 input 端口**；
            //      · 宽度一致。
            if (!width_ok) {
                remaining.push_back(desc + width_note);
                continue;
            }
            std::set<std::string> anc_of_driver;
            for (std::string cur = e.from_module;;) {
                anc_of_driver.insert(cur);
                const auto it = parent_of.find(cur);
                if (it == parent_of.end()) {
                    break;
                }
                cur = it->second.first;
            }
            std::string lca;
            for (std::string cur = e.to_module;;) {
                if (anc_of_driver.count(cur) != 0) {
                    lca = cur;
                    break;
                }
                const auto it = parent_of.find(cur);
                if (it == parent_of.end()) {
                    break;
                }
                cur = it->second.first;
            }
            bool drv_is_input = false;
            bool tgt_is_input = false;
            const bool drv_is_port = module_port_of(e.from_module, e.from_ptr, drv_is_input);
            const bool tgt_is_port = module_port_of(e.to_module, e.to_ptr, tgt_is_input);
            const bool both_direct_children =
                !lca.empty() && p_from != parent_of.end() && p_to != parent_of.end() &&
                p_from->second.first == lca && p_to->second.first == lca;
            if (!both_direct_children || !drv_is_port || drv_is_input || !tgt_is_port ||
                !tgt_is_input) {
                remaining.push_back(
                    desc + " (sibling edge requires two direct children of the same module, a "
                           "child output port on the driver side and a child input port on the "
                           "target side)");
                continue;
            }
            const std::string &wire = names.at(e.from_ptr);
            // 幂等：同一根线可能被多条边复用；与既有端口撞名 ⇒ 失败（否则重复声明）
            auto &wires = synth_wires[lca];
            bool wire_exists = false;
            for (const auto &w : wires) {
                if (w.name == wire) {
                    wire_exists = true;
                    break;
                }
            }
            const auto lca_mod = module_by_key.find(lca);
            bool clashes_with_port = false;
            if (lca_mod != module_by_key.end()) {
                for (const auto &pp : lca_mod->second->ports) {
                    if (pp.name == wire) {
                        clashes_with_port = true;
                        break;
                    }
                }
            }
            if (clashes_with_port) {
                remaining.push_back(desc + " (sibling wire '" + wire +
                                    "' collides with an existing port of module '" + lca + "')");
                continue;
            }
            if (!wire_exists) {
                wires.push_back(SynthesizedPort{wire, /*is_input=*/false, e.from_ptr->size()});
                taken_names.insert(wire);
            }
            {
                std::string err;
                if (!add_conn(lca, p_from->second.second, names.at(e.from_ptr), wire,
                              /*into_child=*/false, err) ||
                    !add_conn(lca, p_to->second.second, names.at(e.to_ptr), wire,
                              /*into_child=*/true, err)) {
                    remaining.push_back(desc + " (" + err + ")");
                    continue;
                }
            }
            continue;
        }

        // ⚠️ **两个方向的合成条件不同**：
        //    · 行 2（子 → 父）⇒ ⇒ 子侧 = **驱动端**所在 module · 父侧 = 被驱动端所在 module
        //    · 行 1（父 → 子）⇒ ⇒ 子侧 = **被驱动端**所在 module · 父侧 = 驱动端所在 module
        if (e.child_instance_name.empty()) {
            remaining.push_back(desc + " (direct parent/child edge without an instance name)");
            continue;
        }
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

    // module 类型名（`<module_type_name>` / `<module_type_name>_<序号>`；同类型多实例各发一份）
    //
    // ⚠️ **序号顺序 MUST 是「实例路径的字典序」**（ADR-060 **v1.12** §2.3 的 G1）：
    //    · 分组键 = **sanitized** `module_type_name`（同类型多实例 ⇒ 同组）
    //    · 组内排序键 = **`HierarchyModule.key` 的字节序**（`std::string` 的比较，
    //      **MUST NOT** 用「逐段比较路径」—— `Component` 的 name 不做字符集校验，
    //      含 `< '.'`（0x2E）的字符时两序会发散）
    //    · **编号前**按 `(sanitized base, 类型身份)` 判身份 ⇒ ⇒ 同组内出现**不同**的
    //      `module_type_name` ⇒ ⇒ 两个**不同类型**撞成同一个 base ⇒ ⇒ 响亮拒绝
    //      （现状只查**生成后**重名 ⇒ 会被静默编成 `x` / `x_1`）
    //    · ⚠️ **MUST NOT 重排 `analysis.modules` 的 post-order 发射序** —— 序号只影响**名字**
    std::map<std::string, std::vector<std::size_t>> type_groups;
    for (std::size_t i = 0; i < analysis.modules.size(); ++i) {
        type_groups[sanitize_verilog_name(analysis.modules[i].module_type_name)].push_back(i);
    }
    std::vector<std::string> module_names(analysis.modules.size());
    for (const auto &group : type_groups) {
        const std::string &base = group.first;
        const std::vector<std::size_t> &members = group.second;
        // ① 编号**之前**判身份（同 base 不同类型 ⇒ 响亮拒绝）
        const std::string &identity = analysis.modules[members.front()].module_type_name;
        for (const std::size_t idx : members) {
            if (analysis.modules[idx].module_type_name != identity) {
                result.error =
                    "hierarchical emission: modules '" + analysis.modules[members.front()].key +
                    "' and '" + analysis.modules[idx].key +
                    "' are different types but sanitize to the same module name base '" + base +
                    "' ('" + identity + "' vs '" + analysis.modules[idx].module_type_name +
                    "'); refusing to number them silently";
                return result;
            }
        }
        // ② 组内按 `key` 的**字节字典序**编号（确定性 · 与 ctx 节点 ID / 组件创建顺序无关）
        std::vector<std::size_t> order = members;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return analysis.modules[a].key < analysis.modules[b].key;
        });
        for (std::size_t k = 0; k < order.size(); ++k) {
            module_names[order[k]] = numbered_name(base, static_cast<int>(k));
        }
    }
    // 兜底：生成名 MUST 唯一（上面的分组已保证 ⇒ ⇒ 留着防后续改动破掉它）
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
    // ⚠️ `module_by_key` 已在**合成阶段之前**建好（见上）⇒ ⇒ 这里不再重复声明
    // module key → 时钟/复位需求（自底向上累积 ⇒ 需求**向上传导**）
    std::map<std::string, SeqNeeds> needs_of;
    for (std::size_t i = 0; i < analysis.modules.size(); ++i) {
        name_of_key[analysis.modules[i].key] = module_names[i];
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
        // ⚠️ **合成连接网**（兄弟边在共同祖先内的中转线 · design §7.2.3）
        {
            const auto it = synth_wires.find(m.key);
            if (it != synth_wires.end()) {
                view.synthesized_wires.insert(view.synthesized_wires.end(), it->second.begin(),
                                              it->second.end());
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
            // ⚠️ **合成连接**（行 2 的子 output → 父侧端点 · 第三轴的逐级同名端口 ·
            //    兄弟边的两端）MUST 在「未消费 output ⇒ 空连接」**之前**并入
            //    ⇒ ⇒ 否则同一端口会同时出现 `.p()` 与 `.p(x)` ⇒ Verilator 报
            //    `Duplicate pin connection`。
            {
                const auto sc = synth_conns.find(m.key + "\x1f" + inst.instance_name);
                if (sc != synth_conns.end()) {
                    conns.insert(conns.end(), sc->second.begin(), sc->second.end());
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
