// src/hierarchy_analysis.cpp
//
// 层次化 RTL 输出的**分析层**实现（见 include/hierarchy_analysis.h 的契约）。
//
// ⚠️ **只读**：本文件 MUST NOT 调用任何 `set_src` / `set_next` / `add_src`
//    （ADR-060 §2.1「图重写 MUST NOT 真的改 IR」）。
// ⚠️ **fail-closed**：需要「隐式端口合成」的跨 module 边在本阶段**没有实现**
//    （那是 T3-④ 的义务）⇒ ⇒ MUST 抛异常，MUST NOT 静默产出不完整的端口表
//    （否则 golden 会把分析层的缺陷**合法化**）。

#include "hierarchy_analysis.h"

#include "codegen_naming.h"
#include "core/context.h"
#include "core/lnodeimpl.h"
#include "logger.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ch {
namespace {

using ch::core::lnodeimpl;
using ch::core::lnodetype;

constexpr const char* kImplicitPortUnsupported =
    "hierarchical analysis: this cross-module edge needs an IMPLICIT PORT "
    "(ADR-060 §2.1), which is not implemented until T3-④. "
    "Analysis refuses to emit a partial port table.";

bool is_mem_node(const lnodeimpl* n) {
    const auto ty = n->type();
    return ty == lnodetype::type_mem || ty == lnodetype::type_mem_read_port ||
           ty == lnodetype::type_mem_write_port;
}

bool is_clock_or_reset(const lnodeimpl* n) {
    return n->type() == lnodetype::type_clock || n->type() == lnodetype::type_reset;
}

struct Tree {
    Component* root = nullptr;
    std::unordered_map<const Component*, Component*> parent;
    std::unordered_map<const Component*, bool> layered;
    std::vector<Component*> postorder;   // 叶子在前、根在最后
};

void collect(Component* c, Tree& t) {
    t.layered[c] = c->is_layer();
    for (const auto& child : c->children()) {
        if (!child) {
            continue;
        }
        auto it = t.parent.find(child.get());
        if (it != t.parent.end()) {
            CHFATAL_EXCEPTION(
                "hierarchical analysis: component '%s' appears twice in the "
                "component tree (duplicate add_child?)",
                child->name().c_str());
        }
        t.parent[child.get()] = c;
        collect(child.get(), t);
    }
    t.postorder.push_back(c);
}

/// 最近的**成层**祖先（含自己）⇒ inline 折叠（ADR-061 决策②）。
Component* nearest_layer(Component* c, const Tree& t) {
    while (c != nullptr) {
        auto it = t.layered.find(c);
        if (it != t.layered.end() && it->second) {
            return c;
        }
        auto pit = t.parent.find(c);
        c = (pit == t.parent.end()) ? nullptr : pit->second;
    }
    return nullptr;
}

}   // namespace

HierarchyAnalysis analyze_hierarchy(Component& root) {
    auto* ctx = root.context();
    CHREQUIRE(ctx != nullptr,
              "hierarchical analysis: root component has no context (not built?)");
    if (ctx == nullptr) {
        CHFATAL_EXCEPTION("hierarchical analysis: root '%s' has no context",
                          root.name().c_str());
    }

    Tree tree;
    tree.root = &root;
    collect(&root, tree);

    // ── 命名：**复用扁平发射器的全局表**（ADR-061 决策④）
    //    ⇒ ⇒ 顶层端口名与扁平产物**逐字相同**，且模块内端口名唯一
    //    （`ch_in` 的默认名都是 `unnamed_input`，不全局去重就无法发射）。
    const auto names = ch::build_verilog_name_table(ctx->get_eval_list());
    auto vname = [&names](const lnodeimpl* n) -> std::string {
        auto it = names.find(n);
        return it == names.end() ? std::string("<unnamed>") : it->second;
    };

    HierarchyAnalysis out;

    // ⚠️ **参数名 MUST NOT 叫 `msg`**：`CHWARN` / `CHERROR` 等宏会展开成
    //    `std::string msg = printf_format(fmt, __VA_ARGS__);`
    //    ⇒ ⇒ 实参里的 `msg` 会**绑定到宏内部那个尚未初始化的 `msg`**
    //    ⇒ ⇒ 读到悬垂指针（实测：SIGSEGV 或打印乱码）。这是**框架级陷阱**，
    //    已记入本 change 的 design.md（宏内部变量名应改名 ⇒ 独立立项）。
    auto warn = [&out](std::string text) {
        CHWARN("%s", text.c_str());
        out.warnings.push_back(std::move(text));
    };

    // ── 前置校验 ①：节点归属的组件 MUST 在 root 的子树里（D9 / ADR-061 决策⑥）
    //    ⚠️ 用**向下走 `children()`** 得到的集合判，MUST NOT 用 parent 链
    //       （成员子组件的 parent_ 在宿主 build 前是空的）
    for (const auto& owned : ctx->get_nodes()) {
        auto* owner = owned->get_parent();
        if (owner == nullptr) {
            continue;
        }
        if (tree.layered.find(owner) == tree.layered.end()) {
            CHFATAL_EXCEPTION(
                "hierarchical analysis: node '%s' belongs to component '%s' which is "
                "NOT registered in the component tree. Assemble it with "
                "top.add_child(std::move(unit)) (or ch_module / ch_device) before "
                "build(ctx).",
                owned->name().c_str(), owner->name().c_str());
        }
    }

    // ── 前置校验 ②：已登记但**从未 build** 的组件 ⇒ 有节点、无端口 ⇒ 拒绝
    //    ⚠️ 判据 MUST 是 `!is_built()`（`build()` 先置 built_ 再 create_ports）
    for (auto* c : tree.postorder) {
        if (!c->is_built()) {
            CHFATAL_EXCEPTION(
                "hierarchical analysis: component '%s' is registered in the component "
                "tree but was never build()-ed. It has nodes but NO PORTS, so it must "
                "not become a module.",
                c->name().c_str());
        }
    }

    // ── 前置校验 ③：**成层**组件自己驱动自己的 `ch_in` ⇒ 非法（ADR-062 决策①）
    //    （子 module 内对 input 赋值 = 非法 Verilog；父层又引用不到体内 driver）
    for (auto* c : tree.postorder) {
        if (!tree.layered[c]) {
            continue;
        }
        for (const auto& owned : ctx->get_nodes()) {
            if (owned->get_parent() != c || owned->type() != lnodetype::type_input ||
                owned->num_srcs() == 0) {
                continue;
            }
            auto* drv = owned->src(0);
            if (drv != nullptr && nearest_layer(drv->get_parent(), tree) == c) {
                CHFATAL_EXCEPTION(
                    "hierarchical analysis: layered component '%s' drives its own "
                    "ch_in '%s' from inside the same module. A sub-module must not "
                    "assign its own input (illegal Verilog); drive it from the parent "
                    "via the instance port instead.",
                    c->name().c_str(), owned->name().c_str());
            }
        }
    }

    // ── 节点 → 折叠后的 module 归属 ──
    std::unordered_map<const lnodeimpl*, Component*> mod_of;
    for (const auto& owned : ctx->get_nodes()) {
        auto* owner = owned->get_parent();
        mod_of[owned.get()] = (owner == nullptr) ? &root : nearest_layer(owner, tree);
    }

    auto module_key = [](const Component* c) { return c->name(); };

    // ── 模块清单（拓扑序：post-order 里成层的 + 根）
    std::unordered_map<const Component*, std::size_t> module_index;
    for (auto* c : tree.postorder) {
        if (!tree.layered[c] && c != &root) {
            continue;
        }
        HierarchyModule m;
        m.key = module_key(c);
        m.module_type_name = c->module_type_name();
        m.is_root = (c == &root);
        module_index[c] = out.modules.size();
        out.modules.push_back(std::move(m));
    }

    // ── 端口表 + 体内节点
    //    ⚠️ 遍历顺序 = ctx 节点创建序（确定性）
    auto find_port = [&out](std::size_t mi, const lnodeimpl* n, bool& is_input)
        -> HierarchyPort* {
        for (auto& p : out.modules[mi].ports) {
            if (p.node == n) {
                is_input = p.is_input;
                return &p;
            }
        }
        return nullptr;
    };

    for (const auto& owned : ctx->get_nodes()) {
        lnodeimpl* n = owned.get();
        auto* owner = n->get_parent();
        Component* mod_c = mod_of[n];
        if (mod_c == nullptr) {
            continue;   // 不可达（root 一定成层）
        }
        const auto mi = module_index.at(mod_c);
        auto& mod = out.modules[mi];

        const auto ty = n->type();
        if (ty == lnodetype::type_clock || ty == lnodetype::type_reset) {
            if (owner == nullptr) {
                // context 级 clock/reset ⇒ 顶层 module 的 input 端口（ADR-058 §2.1）
                // ⚠️ MUST NOT 告警（每个含寄存器的设计都有它们）
                mod.ports.push_back(HierarchyPort{vname(n), /*is_input=*/true, n->size(),
                                                  HierarchyPortOrigin::ClockReset, n});
            } else {
                mod.body.push_back(HierarchyBodyNode{n, vname(n)});
            }
        } else if (ty == lnodetype::type_input) {
            const bool layered_owner = (owner != nullptr) && tree.layered.at(owner);
            if (layered_owner) {
                // 成层组件的 `ch_in` ⇒ **本 module 的 input 端口**
                // （⚠️ emitter MUST 忽略 driver() —— ADR-062 决策①）
                mod.ports.push_back(HierarchyPort{vname(n), /*is_input=*/true, n->size(),
                                                  HierarchyPortOrigin::Self, n});
            } else if (n->num_srcs() > 0) {
                // inline 组件的有 driver 的 `ch_in` ⇒ 体内 wire（扁平规则）
                mod.body.push_back(HierarchyBodyNode{n, vname(n)});
            } else {
                // 无 driver ⇒ **逐级向上冒泡**（ADR-060 §2.2）⇒ 顶层可驱动
                Component* cur = mod_c;
                while (cur != nullptr) {
                    const auto idx = module_index.at(cur);
                    out.modules[idx].ports.push_back(HierarchyPort{
                        vname(n), /*is_input=*/true, n->size(),
                        HierarchyPortOrigin::Bubbled, n});
                    if (cur == &root) {
                        break;
                    }
                    cur = nearest_layer(tree.parent.at(cur), tree);
                }
            }
        } else if (ty == lnodetype::type_output) {
            const auto origin = (owner == mod_c) ? HierarchyPortOrigin::Self
                                                  : HierarchyPortOrigin::InlineFolded;
            mod.ports.push_back(
                HierarchyPort{vname(n), /*is_input=*/false, n->size(), origin, n});
        } else {
            mod.body.push_back(HierarchyBodyNode{n, vname(n)});
        }
    }

    // ── 实例（= 「最近的成层祖先」关系，inline 链折叠成一个实例名）
    std::unordered_map<const Component*, std::size_t> instance_index;   // 子组件 → index
    for (auto* c : tree.postorder) {
        if (!tree.layered[c] || c == &root) {
            continue;
        }
        Component* pm = nearest_layer(tree.parent.at(c), tree);
        const auto mi = module_index.at(pm);
        // 实例名 = 从父 module 的组件到 c 的**相对路径**（`.` → `_`）
        std::vector<const Component*> chain;
        for (Component* cur = c; cur != pm; cur = tree.parent.at(cur)) {
            chain.push_back(cur);
        }
        std::string name;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            if (!name.empty()) {
                name += "_";
            }
            name += (*it)->local_name();
        }
        HierarchyInstance inst;
        inst.instance_name = std::move(name);
        inst.module_key = module_key(c);
        instance_index[c] = out.modules[mi].instances.size();
        out.modules[mi].instances.push_back(std::move(inst));
    }

    // ── 连接表（只记跨 module 的边）+ 实例端口映射
    for (const auto& owned : ctx->get_nodes()) {
        lnodeimpl* v = owned.get();
        const uint32_t nsrc = v->num_srcs();
        for (uint32_t i = 0; i < nsrc; ++i) {
            lnodeimpl* u = v->src(i);
            if (u == nullptr) {
                continue;
            }
            // clock/reset 不走「信号边」⇒ 它们由 R10 的 clock/reset 端口贯通处理
            if (is_clock_or_reset(u) || is_clock_or_reset(v)) {
                continue;
            }
            Component* um_c = mod_of.at(u);
            Component* vm_c = mod_of.at(v);
            if (um_c == vm_c) {
                continue;
            }
            // 跨 module 的 ch_mem ⇒ 响亮拒绝（ADR-060 §2.1：mem MUST 与使用者同层）
            if (is_mem_node(u) || is_mem_node(v)) {
                CHFATAL_EXCEPTION(
                    "hierarchical analysis: cross-module ch_mem access ('%s' in '%s' "
                    "<- '%s' in '%s'). ADR-060 requires the memory and its user to be "
                    "in the same module. Two legal remedies: (1) the parent holds the "
                    "ch_mem and passes address/wdata down and rdata up through ports; "
                    "(2) the child holds the ch_mem and exposes the read result as its "
                    "own ch_out.",
                    v->name().c_str(), module_key(vm_c).c_str(), u->name().c_str(),
                    module_key(um_c).c_str());
            }

            // 只支持**直接父子**（含 inline 链折叠）⇒ 否则需要 pass-through（T3-④）
            Component* direct_parent_of_vm =
                (vm_c == &root) ? nullptr : nearest_layer(tree.parent.at(vm_c), tree);
            const bool parent_drives_child = (um_c == direct_parent_of_vm);
            Component* direct_parent_of_um =
                (um_c == &root) ? nullptr : nearest_layer(tree.parent.at(um_c), tree);
            const bool child_drives_parent = (vm_c == direct_parent_of_um);

            if (!parent_drives_child && !child_drives_parent) {
                // 兄弟边 / 跨层 pass-through ⇒ 需要父层 wire 中转（T3-④）
                CHFATAL_EXCEPTION(
                    "hierarchical analysis: unsupported cross-module edge between "
                    "sibling / non-adjacent modules ('%s'@'%s' -> '%s'@'%s'). "
                    "A parent-module wire (or a pass-through port) is needed; that is "
                    "T3-④'s obligation.",
                    u->name().c_str(), module_key(um_c).c_str(), v->name().c_str(),
                    module_key(vm_c).c_str());
            }

            // ⚠️ 只有**子侧**端点必须是端口：
            //    · 父 → 子：`v` MUST 是子 module 的 **input 端口**
            //      （父侧可以是端口，也可以是体内 wire —— e.g. 被 inline 折叠的
            //       `ch_in` 有 driver ⇒ 体内 wire ⇒ 正是扁平规则）
            //    · 子 → 父：`u` MUST 是子 module 的 **output 端口**
            //    两侧都不满足 ⇒ 需要**隐式端口合成** ⇒ T3-④ 的义务 ⇒ 本层拒绝。
            bool v_is_input = false;
            const bool v_is_port = find_port(module_index.at(vm_c), v, v_is_input) != nullptr;
            bool u_is_input = false;
            const bool u_is_port = find_port(module_index.at(um_c), u, u_is_input) != nullptr;
            const bool child_side_ready =
                parent_drives_child ? (v_is_port && v_is_input) : (u_is_port && !u_is_input);
            if (!child_side_ready) {
                CHFATAL_EXCEPTION("%s (edge: '%s'@'%s' -> '%s'@'%s')", kImplicitPortUnsupported,
                                  u->name().c_str(), module_key(um_c).c_str(),
                                  v->name().c_str(), module_key(vm_c).c_str());
            }

            HierarchyEdge e;
            e.from_node = vname(u);
            e.from_module = module_key(um_c);
            e.to_node = vname(v);
            e.to_module = module_key(vm_c);
            out.edges.push_back(std::move(e));

            if (parent_drives_child) {
                // 子 input ← 父节点
                auto& inst = out.modules[module_index.at(um_c)]
                                 .instances[instance_index.at(vm_c)];
                inst.connections.push_back(HierarchyConnection{vname(v), vname(u), true});
            } else {
                // 子 output → 父节点
                auto& inst = out.modules[module_index.at(vm_c)]
                                 .instances[instance_index.at(um_c)];
                inst.connections.push_back(HierarchyConnection{vname(u), vname(v), false});
            }
        }
    }

    // ── 归属空洞：`owner == nullptr` 的非 clock/reset 节点 ⇒ 归顶层 + CHWARN
    //    （capability `rtl-hierarchical-output`：不属于任何组件的节点 MUST 归顶层）
    for (const auto& owned : ctx->get_nodes()) {
        lnodeimpl* n = owned.get();
        if (n->get_parent() != nullptr || is_clock_or_reset(n)) {
            continue;
        }
        warn("hierarchical analysis: node '" + n->name() +
             "' belongs to no component => placed in the top module");
    }

    // ── 确定性排序（golden 可逐字节比对）
    for (auto& m : out.modules) {
        std::stable_sort(m.ports.begin(), m.ports.end(),
                         [](const HierarchyPort& a, const HierarchyPort& b) {
                             return a.node->id() < b.node->id();
                         });
        std::stable_sort(m.body.begin(), m.body.end(),
                         [](const HierarchyBodyNode& a, const HierarchyBodyNode& b) {
                             return a.node->id() < b.node->id();
                         });
        for (auto& inst : m.instances) {
            std::stable_sort(inst.connections.begin(), inst.connections.end(),
                             [](const HierarchyConnection& a, const HierarchyConnection& b) {
                                 return a.child_port < b.child_port;
                             });
        }
        std::stable_sort(m.instances.begin(), m.instances.end(),
                         [](const HierarchyInstance& a, const HierarchyInstance& b) {
                             return a.instance_name < b.instance_name;
                         });
    }
    std::stable_sort(out.edges.begin(), out.edges.end(),
                     [](const HierarchyEdge& a, const HierarchyEdge& b) {
                         if (a.from_module != b.from_module) {
                             return a.from_module < b.from_module;
                         }
                         if (a.from_node != b.from_node) {
                             return a.from_node < b.from_node;
                         }
                         if (a.to_module != b.to_module) {
                             return a.to_module < b.to_module;
                         }
                         return a.to_node < b.to_node;
                     });

    return out;
}

std::string HierarchyAnalysis::to_text() const {
    std::ostringstream os;
    os << "# ch-hierarchy-analysis v1\n";
    for (const auto& m : modules) {
        os << "[module] key=" << m.key << " type=" << m.module_type_name
           << " root=" << (m.is_root ? 1 : 0) << " ports=" << m.ports.size()
           << " body=" << m.body.size() << " insts=" << m.instances.size() << "\n";
        for (const auto& p : m.ports) {
            const char* origin = "self";
            switch (p.origin) {
            case HierarchyPortOrigin::Self:
                origin = "self";
                break;
            case HierarchyPortOrigin::ClockReset:
                origin = "clock_reset";
                break;
            case HierarchyPortOrigin::InlineFolded:
                origin = "inline_folded";
                break;
            case HierarchyPortOrigin::Bubbled:
                origin = "bubbled";
                break;
            }
            os << "  port " << (p.is_input ? "in " : "out") << " " << p.name
               << " w=" << p.width << " origin=" << origin << "\n";
        }
        for (const auto& n : m.body) {
            os << "  body " << ch::core::to_string(n.node->type()) << " " << n.name
               << " w=" << n.node->size() << "\n";
        }
        for (const auto& inst : m.instances) {
            os << "  inst " << inst.instance_name << " -> " << inst.module_key << "\n";
            for (const auto& c : inst.connections) {
                os << "    conn " << c.child_port << (c.into_child ? " <- " : " -> ")
                   << c.parent_side << "\n";
            }
        }
    }
    for (const auto& e : edges) {
        os << "[edge] " << e.from_module << "." << e.from_node << " -> "
           << e.to_module << "." << e.to_node << "\n";
    }
    for (const auto& w : warnings) {
        os << "[warn] " << w << "\n";
    }
    return os.str();
}

}   // namespace ch
