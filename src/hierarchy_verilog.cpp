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
        analysis = analyze_hierarchy(root);
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
    const auto names = build_verilog_name_table(root.context()->get_eval_list());

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

        // ── 子 → 父：父侧端点 MUST 是父 module 的 **output 端口**
        //    （否则父层需要一根「连接网」或隐式端口 ⇒ 属 T3-④ 的隐式端口合成）
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
                    result.error =
                        "hierarchical emission: child module '" + inst.module_key +
                        "' drives '" + conn.parent_side + "' in module '" + m.key +
                        "', which is not an output port of that module. This needs an "
                        "implicit port (ADR-060 §2.1) or a parent-side net; implicit "
                        "port synthesis is not implemented yet.";
                    return result;
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
