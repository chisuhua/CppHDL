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

#include "ast/mem_port_impl.h"
#include "codegen_naming.h"
#include "core/context.h"
#include "core/lnodeimpl.h"
#include "hierarchy_chain.h"
#include "hierarchy_synth_count.h"
#include "logger.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ch {
namespace {

using ch::core::lnodeimpl;
using ch::core::mem_port_impl;
using ch::core::mem_port_type;
using ch::core::mem_write_port_impl;
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
///
/// ⚠️ **根组件永远是 RTL 顶层 module** ⇒ 即使它自己**没**声明成层（= 全 inline 设计），
///    走到根时 MUST 返回根（`nullptr` 会让根的节点**全部丢失** ⇒ 产物只剩空壳）。
Component* nearest_layer(Component* c, const Tree& t) {
    while (c != nullptr) {
        auto it = t.layered.find(c);
        if (it != t.layered.end() && it->second) {
            return c;
        }
        auto pit = t.parent.find(c);
        if (pit == t.parent.end()) {
            return (c == t.root) ? c : nullptr;   // 根 = RTL 顶层
        }
        c = pit->second;
    }
    return nullptr;
}

/// **跨模块 `ch_mem` 的独立预扫描**（capability `hierarchy-friendliness-diagnosis`
/// 的 A1 口径）。
///
/// ⚠️ **为什么需要它**：分析层对跨模块 `ch_mem` 是**永远抛**（ADR-060 §2.1）
/// ⇒ ⇒ 「有跨模块 mem」这个事实**只存在于异常里** ⇒ ⇒ 若报告只写「不可分析」，
/// 用户**看不到**到底是不是 mem 的问题 ⇒ ⇒ 这里在**分析失败之后**独立再扫一遍，
/// 报出**真实**计数 ⇒ ⇒ 「有跨模块 mem」不会被静默表达成「0」。
///
/// ⚠️ **判定口径与分析层**同一份**：节点归属 = `nearest_layer(owner)`（★ inline
/// 折叠后的 module 归属）⇒ 边跨 module（`mod_of[u] != mod_of[v]`）⇒ 且边的一端是
/// mem 节点（`is_mem_node`）⇒ ⇒ **与 `analyze_hierarchy` 的判定同源**。
///
/// ⚠️ **容错 ⇒ 计数是「尽力而为」的**（★ **不是**精确值）：
/// · 用 `find`（★ **不是** `at`）⇒ ⇒ 分析在前置校验阶段就失败时归属表可能不完整；
/// · 归属为 `nullptr`（★ 节点归属的组件**不在**组件树内 —— 前置校验①那类用户错误）
///   ⇒ ⇒ **跳过**（★ MUST NOT 解引用 `nullptr` ⇒ 见下面实现）。
/// ⇒ ⇒ **后果**：计数可能**小于**真实值 ⇒ ⇒ 用户面文档 MUST 说「尽力而为」，
///    MUST NOT 说「真实计数」。
struct MemScan {
    std::size_t count = 0;
    std::vector<std::pair<std::string, std::string>> hits;  ///< (module, node 名)
};

MemScan scan_cross_module_mem(Component& root) {
    MemScan scan;
    auto* ctx = root.context();
    if (ctx == nullptr) {
        return scan;
    }
    Tree tree;
    tree.root = &root;
    collect(&root, tree);

    std::unordered_map<const lnodeimpl*, Component*> mod_of;
    for (const auto& owned : ctx->get_nodes()) {
        auto* owner = owned.get()->get_parent();
        mod_of[owned.get()] = (owner == nullptr) ? &root : nearest_layer(owner, tree);
    }

    // ⚠️ **边遍历口径与分析层一致**：`v` = 被驱动端，`u` = 驱动端（★ 每条边恰好
    //    访问一次 ⇒ ⇒ 计数不重复）
    for (const auto& owned : ctx->get_nodes()) {
        lnodeimpl* v = owned.get();
        const uint32_t nsrc = v->num_srcs();
        for (uint32_t i = 0; i < nsrc; ++i) {
            lnodeimpl* u = v->src(i);
            if (u == nullptr) {
                continue;
            }
            if (is_clock_or_reset(u) || is_clock_or_reset(v)) {
                continue;
            }
            auto um = mod_of.find(u);
            auto vm = mod_of.find(v);
            // ⚠️⚠️ **归属可能为空**（critic BLOCKER 2）：`nearest_layer` 对**不在组件树内**
            //    的 owner 返回 `nullptr`（★ 前置校验①「节点归属的组件不在 root 的子树里」
            //    就是这类用户错误 ⇒ ⇒ 分析会抛 ⇒ ⇒ 预扫描仍会在同一 ctx 上跑）
            //    ⇒ ⇒ **MUST NOT** 对 `nullptr` 解引用（★ 原实现 `vm->second->name()`
            //    ⇒ ⇒ SIGSEGV ⇒ ⇒ 严于「返回报告」这条契约）。
            if (um == mod_of.end() || vm == mod_of.end() ||
                um->second == nullptr || vm->second == nullptr) {
                // ⚠️ 归属不明 ⇒ **跳过**（★ 计数可能偏小 ⇒ 见 `MemScan` 的注释）
                continue;
            }
            if (um->second == vm->second) {
                continue;   // 同 module ⇒ 不是跨模块
            }
            if (!is_mem_node(u) && !is_mem_node(v)) {
                continue;
            }
            ++scan.count;
            scan.hits.emplace_back(vm->second->name(), u->name());
        }
    }
    return scan;
}

}   // namespace

HierarchyAnalysis analyze_hierarchy(Component& root, ImplicitPortHandling handling) {
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
    // ── (i) **mem 本体跨 module** ⇒ 响亮拒绝（ADR-066 决策 ⑦ v1.2 · 判别程序 ①②③）
    //    ⚠️ **为什么必须独立扫描**：mem 本体（`memimpl`）**零 `add_src`**，端口的 addr/en 也
    //    可能是**父层**节点 ⇒ ⇒ 字面 (i) **不产生任何跨 module 边** ⇒ ⇒ 下面的边闸门对它
    //    **不可达**（实测：分析放行 + 发射期 `mem[<子内名>]` ⇒ implicit wire ⇒ 宽度截断）。
    //    ⇒ ⇒ 判据 = **节点级归属比对**：持有 module（`port->parent()` 的折叠后归属）
    //    ≠ 使用 module（端口节点自身的折叠后归属）。
    for (const auto& owned : ctx->get_nodes()) {
        auto* port = dynamic_cast<mem_port_impl*>(owned.get());
        if (port == nullptr || port->parent() == nullptr) {
            continue;
        }
        const auto it_hold = mod_of.find(port->parent());
        const auto it_use = mod_of.find(port);
        if (it_hold == mod_of.end() || it_use == mod_of.end()) {
            continue;   // 防御：未登记归属 ⇒ 不误拒
        }
        if (it_hold->second != it_use->second) {
            CHFATAL_EXCEPTION(
                "hierarchical analysis: cross-module ch_mem access (ch_mem '%s' owned by "
                "module '%s', used by a port in module '%s' for node '%s'). "
                "ADR-066 decision 7 (i): the ch_mem body and its user MUST live in the same "
                "module. Legal remedies: (1) move the read/write ports into the module that "
                "owns the ch_mem; (2) keep the ch_mem in the user's module and pass "
                "address/wdata down and rdata up through ports.",
                port->parent()->name().c_str(), it_hold->second->name().c_str(),
                it_use->second->name().c_str(), port->name().c_str());
        }
    }

    // ── 节点 → module key（发射器用：判断驱动边是否跨 module）
    for (const auto& owned : ctx->get_nodes()) {
        auto* owner = owned->get_parent();
        Component* mc = (owner == nullptr) ? &root : nearest_layer(owner, tree);
        if (mc != nullptr) {
            out.module_of_node[owned.get()] = mc->name();
        }
    }

    // ── 模块清单（拓扑序：post-order 里成层的 + 根）
    std::unordered_map<const Component*, std::size_t> module_index;
    std::unordered_map<std::string, std::size_t> module_index_by_key;
    for (auto* c : tree.postorder) {
        if (!tree.layered[c] && c != &root) {
            continue;
        }
        HierarchyModule m;
        m.key = module_key(c);
        m.module_type_name = c->module_type_name();
        m.is_root = (c == &root);
        module_index[c] = out.modules.size();
        module_index_by_key[m.key] = out.modules.size();
        out.modules.push_back(std::move(m));
    }

    // ── 端口表 + 体内节点
    //    ⚠️ 遍历顺序 = ctx 节点创建序（确定性）
    //    ⚠️ **冒泡**（端口 + 逐跳连接）在实例建好之后统一生成（G3-2）⇒ 先记录节点
    std::vector<lnodeimpl *> pending_bubbles;
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
                if (n->num_srcs() == 0) {
                    // ⚠️ **F2**（ADR-060 v1.13 G3）：成层组件的**悬空** `ch_in`
                    //    也 MUST 逐级冒泡（否则它只在子 module 上 ⇒ 顶层驱动不到）
                    pending_bubbles.push_back(n);
                }
            } else if (n->num_srcs() > 0) {
                // inline 组件的有 driver 的 `ch_in` ⇒ 体内 wire（扁平规则）
                mod.body.push_back(HierarchyBodyNode{n, vname(n)});
            } else {
                // 无 driver ⇒ **逐级向上冒泡**（ADR-060 §2.2）⇒ 顶层可驱动
                // ⚠️ 端口 + **逐跳连接**统一在实例建好之后生成（G3-2 ⇒ 见下）
                pending_bubbles.push_back(n);
            }
        } else if (ty == lnodetype::type_output) {
            // ⚠️ **ADR-061 决策② 的两作用域表**（F3 · 2026-11-05）：
            //    · `owner == 本 module` ⇒ **自身端口**（角色①）
            //    · `owner` 是**被 inline 的**子组件 + 本 module 是 **RTL 顶层**
            //      ⇒ ⇒ **顶层 output 端口**（角色②的上格 —— 与扁平规则一致 ⇒ ⇒
            //      ⇒ 「全 inline ⇒ 与扁平逐字节一致」判据不受影响）
            //    · 同上但本 module 是**非顶层成层** ⇒ ⇒ **体内普通信号**（角色③）
            //      ⇒ ⇒ **不上该 module 的端口表** ⇒ ⇒ 由发射层发 `logic` 声明
            //      （⚠️ 缺声明 ⇒ `assign` 引用未声明信号 ⇒ Verilator 报 undeclared）
            if (owner == mod_c) {
                mod.ports.push_back(HierarchyPort{vname(n), /*is_input=*/false, n->size(),
                                                  HierarchyPortOrigin::Self, n});
            } else if (mod.is_root) {
                mod.ports.push_back(HierarchyPort{vname(n), /*is_input=*/false, n->size(),
                                                  HierarchyPortOrigin::InlineFolded, n});
            } else {
                mod.body.push_back(HierarchyBodyNode{n, vname(n)});
            }
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

    // ── **冒泡**（ADR-060 v1.13 G3）：端口 + **逐跳 pass-through 连接** ──────────
    //    ⚠️ **F2**：成层组件的悬空 `ch_in` 也要冒泡（原先只登记 `Self` 端口）。
    //    ⚠️ **存量 inline 缺口**：原实现只登记端口、**不生成连接** ⇒ 折进非根成层
    //       module 时中间层空连接 ⇒ 值传到中途**断线且零报错**（结构/lint 都看不见）。
    //    ⚠️ 连接生成在**本层**（G3-2）：发射器 MUST NOT 自行推导冒泡连接。
    {
        // module key → (父 module key, 父 module 内的实例名)
        std::map<std::string, std::pair<std::string, std::string>> parent_of_key;
        for (const auto &m : out.modules) {
            for (const auto &inst : m.instances) {
                parent_of_key[inst.module_key] = {m.key, inst.instance_name};
            }
        }
        auto module_index_or_throw = [&](const std::string &mk) -> std::size_t {
            const auto it = module_index_by_key.find(mk);
            if (it == module_index_by_key.end()) {
                CHFATAL_EXCEPTION("hierarchical analysis: module '%s' not found while bubbling",
                                  mk.c_str());
            }
            return it->second;
        };
        // G3-3：同名 + 同向 + 同宽 ⇒ **幂等复用**；否则响亮拒绝（不静默择一）
        auto ensure_bubbled_port = [&](const std::string &mk, const lnodeimpl *n) {
            auto &ports = out.modules[module_index_or_throw(mk)].ports;
            for (const auto &p : ports) {
                if (p.name != vname(n)) {
                    continue;
                }
                if (p.is_input && p.width == n->size()) {
                    return;   // owner 的 Self 端口 / 既有 Bubbled 端口
                }
                CHFATAL_EXCEPTION(
                    "hierarchical analysis: bubbled input '%s' collides with an existing port "
                    "of module '%s' (direction/width mismatch)",
                    vname(n).c_str(), mk.c_str());
            }
            ports.push_back(HierarchyPort{vname(n), /*is_input=*/true, n->size(),
                                          HierarchyPortOrigin::Bubbled, n});
        };
        for (auto *n : pending_bubbles) {
            Component *mod_c = mod_of[n];
            if (mod_c == nullptr) {
                continue;
            }
            // 链 = [owner module … root]
            std::vector<Component *> chain;
            for (Component *c = mod_c;;) {
                chain.push_back(c);
                if (c == &root) {
                    break;
                }
                c = nearest_layer(tree.parent.at(c), tree);
            }
            BubbledInput bi;
            bi.node = n;
            bi.module = module_key(mod_c);
            for (auto *c : chain) {
                bi.chain.push_back(module_key(c));
            }
            out.bubbled_inputs.push_back(std::move(bi));

            // 端口：链上每一级（owner 的 `Self` 端口 / 已有的 `Bubbled` 端口 ⇒ 幂等跳过）
            for (auto *c : chain) {
                ensure_bubbled_port(module_key(c), n);
            }

            // 连接：顶端 = root（不属 `path`），底端 = owner module（其端口名 = 全局名）
            std::vector<std::string> path;
            for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
                if (*it == &root) {
                    continue;
                }
                path.push_back(module_key(*it));
            }
            if (path.empty()) {
                continue;   // owner 就是 root ⇒ 不跨 module 边界
            }
            const auto hops = ch::detail::plan_pass_chain(
                path, vname(n), vname(n), /*leaf_is_port=*/true, vname(n), parent_of_key);
            if (hops.empty()) {
                CHFATAL_EXCEPTION(
                    "hierarchical analysis: broken ancestor chain while bubbling '%s'",
                    vname(n).c_str());
            }
            for (const auto &hop : hops) {
                bool linked = false;
                for (auto &inst : out.modules[module_index_or_throw(hop.parent_module)].instances) {
                    if (inst.instance_name != hop.instance_name ||
                        inst.module_key != hop.child_module) {
                        continue;
                    }
                    bool exists = false;
                    for (const auto &c : inst.connections) {
                        if (c.child_port == hop.child_port) {
                            if (c.parent_side != hop.parent_arg) {
                                CHFATAL_EXCEPTION(
                                    "hierarchical analysis: bubbled port '%s' is already "
                                    "connected to a different signal in module '%s'",
                                    hop.child_port.c_str(), hop.parent_module.c_str());
                            }
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        inst.connections.push_back(
                            HierarchyConnection{hop.child_port, hop.parent_arg, /*into_child=*/true});
                    }
                    linked = true;
                    break;
                }
                if (!linked) {
                    CHFATAL_EXCEPTION(
                        "hierarchical analysis: instance '%s' of module '%s' not found while "
                        "bubbling '%s'",
                        hop.instance_name.c_str(), hop.parent_module.c_str(), vname(n).c_str());
                }
            }
        }
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
            // 只支持**直接父子**（含 inline 链折叠）⇒ 否则需要 pass-through（T3-④）
            Component* direct_parent_of_vm =
                (vm_c == &root) ? nullptr : nearest_layer(tree.parent.at(vm_c), tree);
            const bool parent_drives_child = (um_c == direct_parent_of_vm);
            Component* direct_parent_of_um =
                (um_c == &root) ? nullptr : nearest_layer(tree.parent.at(um_c), tree);
            const bool child_drives_parent = (vm_c == direct_parent_of_um);

            // ── 跨 module 的 ch_mem ⇒ **三桶**（ADR-066 决策 ⑦ v1.2 / 决策 ⑧ v1.3）
            //    (i)   mem 本体跨 module ⇒ 已由上方**独立扫描**拒绝（此处兜底）
            //    (ii)  mem 本体与使用者同 module、仅 addr/en/wdata **实参视线**跨层
            //          （且实参来源**相邻**）⇒ **放行** ⇒ 走下面的常规跨 module 边路径
            //          （决策 ① 的父侧落点网 ⇒ 决策 ⑧ 的「登记义务」天然满足）
            //    (iii) 非相邻实参（孙 / 兄弟）⇒ **仍拒**（由下面的相邻性规则）
            if (is_mem_node(u) || is_mem_node(v)) {
                bool allow_ii = false;
                if (auto* port = dynamic_cast<mem_port_impl*>(v)) {
                    const bool is_arg_slot =
                        (u == port->addr()) || (u == port->enable()) ||
                        (port->port_type() == mem_port_type::write &&
                         u == static_cast<mem_write_port_impl*>(port)->wdata());
                    // ⚠️ 只放行**实参槽** ⇒ `data_output()`（rdata）/ cd 槽跨层 **不属** (ii)
                    //    （ADR-066 决策 ⑧ ⑤：rdata 跨层 ⇒ 仍按 (i) 拒）
                    allow_ii = is_arg_slot && (parent_drives_child || child_drives_parent);
                }
                if (!allow_ii) {
                    CHFATAL_EXCEPTION(
                        "hierarchical analysis: cross-module ch_mem access ('%s' in '%s' "
                        "<-'%s' in '%s'). ADR-060 §2.1 / ADR-066 decision 7 requires the "
                        "ch_mem BODY and its user to be in the same module. Legal remedies: "
                        "(1) keep the ch_mem in the user's module and pass address/wdata down "
                        "and rdata up through ports; (2) move the read/write ports into the "
                        "module that owns the ch_mem; (3) if the operand comes from a "
                        "grandchild or a sibling module, route it through a transit wire in "
                        "the common ancestor (decision 7 (iii)).",
                        v->name().c_str(), module_key(vm_c).c_str(), u->name().c_str(),
                        module_key(um_c).c_str());
                }
            }

            if (!parent_drives_child && !child_drives_parent) {
                // 兄弟边 / 跨层 pass-through ⇒ 需要父层 wire 中转（T3-④）
                if (handling == ImplicitPortHandling::Reject) {
                    CHFATAL_EXCEPTION(
                        "hierarchical analysis: unsupported cross-module edge between "
                        "sibling / non-adjacent modules ('%s'@'%s' -> '%s'@'%s'). "
                        "A parent-module wire (or a pass-through port) is needed; that is "
                        "T3-④'s obligation.",
                        u->name().c_str(), module_key(um_c).c_str(), v->name().c_str(),
                        module_key(vm_c).c_str());
                }
                // `Record` ⇒ 记录（⇒ 发射器按 `PortPolicy` 处置）
                PendingImplicitEdge pe;
                pe.from_node = vname(u);
                pe.from_module = module_key(um_c);
                pe.to_node = vname(v);
                pe.to_module = module_key(vm_c);
                pe.reason = "sibling / non-adjacent modules";
                pe.from_ptr = u;
                pe.to_ptr = v;
                pe.child_drives_parent = child_drives_parent;
                out.pending_implicit_edges.push_back(std::move(pe));
                continue;
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
                if (handling == ImplicitPortHandling::Reject) {
                    CHFATAL_EXCEPTION("%s (edge: '%s'@'%s' -> '%s'@'%s')",
                                      kImplicitPortUnsupported, u->name().c_str(),
                                      module_key(um_c).c_str(), v->name().c_str(),
                                      module_key(vm_c).c_str());
                }
                // `Record` ⇒ 记录（⇒ 发射器按 `PortPolicy` 处置）
                PendingImplicitEdge pe;
                pe.from_node = vname(u);
                pe.from_module = module_key(um_c);
                pe.to_node = vname(v);
                pe.to_module = module_key(vm_c);
                pe.reason = parent_drives_child ? "child side is not an input port"
                                                : "child side is not an output port";
                pe.from_ptr = u;
                pe.to_ptr = v;
                pe.child_drives_parent = child_drives_parent;
                // ⚠️ 实例名**仅在直接父子关系下**有定义（⇒ 非相邻边留空 ⇒ ⇒ 仍是 fail-closed）
                // ⚠️ **方向决定谁是「子」**：
                //    · 行 2（子 → 父）⇒ ⇒ 子 = 驱动端所在 module（`um_c`）· 父 = `vm_c`
                //    · 行 1（父 → 子）⇒ ⇒ 子 = 被驱动端所在 module（`vm_c`）· 父 = `um_c`
                if (parent_drives_child || child_drives_parent) {
                    Component *child_c = child_drives_parent ? um_c : vm_c;
                    Component *parent_c = child_drives_parent ? vm_c : um_c;
                    const auto inst_it = instance_index.find(child_c);
                    if (inst_it != instance_index.end()) {
                        const auto &insts = out.modules[module_index.at(parent_c)].instances;
                        if (inst_it->second < insts.size()) {
                            pe.child_instance_name = insts[inst_it->second].instance_name;
                        }
                    }
                }
                out.pending_implicit_edges.push_back(std::move(pe));
                continue;
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

// ═══════════════════════════════════════════════════════════════════════════
// 层次友好度**诊断**（★ 只读 · capability `hierarchy-friendliness-diagnosis`）
// ═══════════════════════════════════════════════════════════════════════════

std::string HierarchyDiagnosis::to_text() const {
    // ⚠️ **确定性**：所有集合**排序后**输出 ⇒ ⇒ 同一设计连续两次诊断**逐字节相同**
    //    （★ 明细的顺序 MUST NOT 依赖 `module_of_node`（`unordered_map`）的遍历序）
    std::ostringstream oss;
    oss << "verdict=" << (verdict == HierarchyFriendliness::kClean
                              ? "kClean"
                              : (verdict == HierarchyFriendliness::kNeedsWork ? "kNeedsWork"
                                                                                : "kNotAnalyzable"))
        << "\n";
    auto opt = [&oss](const char *key, const std::optional<std::size_t> &v) {
        oss << key << "=";
        if (v.has_value()) {
            oss << *v;
        } else {
            oss << "<unmeasured>";
        }
        oss << "\n";
    };
    opt("implicit_edge_count", implicit_edge_count);
    opt("synthesized_port_count", synthesized_port_count);
    opt("dangling_input_count", dangling_input_count);
    opt("unconsumed_child_output_count", unconsumed_child_output_count);
    opt("cross_module_mem_count", cross_module_mem_count);
    oss << "module_count=" << module_count << "\n";

    std::vector<HierarchyModuleSize> sizes = module_sizes;
    std::sort(sizes.begin(), sizes.end(),
              [](const HierarchyModuleSize &a, const HierarchyModuleSize &b) {
                  return a.module < b.module;
              });
    for (const auto &s : sizes) {
        oss << "module " << s.module << " ports=" << s.port_count
            << " instances=" << s.instance_count << " body=" << s.body_count << "\n";
    }

    std::vector<HierarchyDiagnosisDetail> det = details;
    std::sort(det.begin(), det.end(),
              [](const HierarchyDiagnosisDetail &a, const HierarchyDiagnosisDetail &b) {
                  if (a.category != b.category) return a.category < b.category;
                  if (a.module != b.module) return a.module < b.module;
                  if (a.node != b.node) return a.node < b.node;
                  return a.reason < b.reason;
              });
    for (const auto &d : det) {
        oss << "detail " << d.category << " | " << d.module << " | " << d.node << " | "
            << d.reason << "\n";
    }
    return oss.str();
}

HierarchyDiagnosis diagnose_hierarchy(Component &root) {
    HierarchyDiagnosis diag;

    // ── ① 分析（★ MUST 用 `Record` ⇒ 否则遇到第一条隐式边就抛 ⇒ 退化为
    //    「不可分析」⇒ 与「一次报全」的目的相反）
    std::unique_ptr<HierarchyAnalysis> analysis;
    std::string failure_reason;
    try {
        analysis = std::make_unique<HierarchyAnalysis>(
            analyze_hierarchy(root, ImplicitPortHandling::Record));
    } catch (const std::runtime_error &e) {
        // ⚠️ **只捕 `std::runtime_error`**（★ 分析层的 15 个抛点全是它）
        //    ⇒ ⇒ `std::out_of_range` / `std::bad_alloc` 会**冒泡**
        //    ⇒ ⇒ 框架 bug **不会**被伪装成「你的设计不可分析」
        failure_reason = e.what();
    }

    // ── ② 跨模块 `ch_mem` 的**独立预扫描**（A1）⇒ ⇒ 无论分析成败都做
    //    ⇒ ⇒ 「有跨模块 mem」不会被静默表达成「0」
    const MemScan mem_scan = scan_cross_module_mem(root);
    diag.cross_module_mem_count = mem_scan.count;

    auto *ctx = root.context();
    const auto names = (ctx != nullptr)
                           ? ch::build_verilog_name_table(ctx->get_eval_list())
                           : std::unordered_map<const lnodeimpl *, std::string>();
    auto vname = [&names](const lnodeimpl *n) -> std::string {
        if (n == nullptr) {
            return "<null>";
        }
        auto it = names.find(n);
        return it == names.end() ? std::string("<unnamed>") : it->second;
    };

    if (!analysis) {
        // ── 不可分析 ⇒ 返回报告（★ **不抛**）⇒ ⇒ 4 项 = 未测得（★ **不是 0**）
        diag.verdict = HierarchyFriendliness::kNotAnalyzable;
        diag.implicit_edge_count.reset();
        diag.synthesized_port_count.reset();
        diag.dangling_input_count.reset();
        diag.unconsumed_child_output_count.reset();
        diag.details.push_back(HierarchyDiagnosisDetail{"not_analyzable", "", "",
                                                         failure_reason});
        for (const auto &hit : mem_scan.hits) {
            diag.details.push_back(
                HierarchyDiagnosisDetail{"cross_module_mem", hit.first, hit.second,
                                          "cross-module ch_mem access (pre-scan)"});
        }
        return diag;
    }

    const HierarchyAnalysis &an = *analysis;

    // ── ③ 隐式**边**数（★ 精确：分析层直出）
    diag.implicit_edge_count = an.pending_implicit_edges.size();

    // ── ④ 需合成的**端口**数（★ 与发射器**同源**：去重 key + 可合成性判定 +
    //    宽度判定**全部**走 `src/hierarchy_synth_count.h` 的共享原语）
    {
        std::set<std::pair<std::string, const lnodeimpl *>> keys;
        for (const auto &e : an.pending_implicit_edges) {
            // ⚠️⚠️ **路由门**（critic BLOCKER 1）：发射器对**每条** pending 边先分流 ——
            //    · 兄弟 / 非相邻边（`child_instance_name` **恒空**）⇒ ⇒ 走「祖先链」
            //      分支（第三轴链 / 向上 pass-through 链）⇒ ⇒ **不**合成
            //      `__if_` / `__x_` 端口（`src/hierarchy_verilog.cpp:419-483` /
            //      `:486-574` ⇒ 两处都 `continue`）⇒ ⇒ **本项 MUST NOT 计它们**
            //      （★ 向上 pass-through 产的 `__up_` 端口按 delta 明确**排除**；
            //        第三轴链产的端口**既非** `__if_`/`__x_` **也非** `__up_` 前缀
            //        ⇒ ⇒ delta 的「按端口名前缀分别计数」对它**不可执行**）
            //    · 直接父子边 ⇒ 分析层**必定**填了实例名（`:619`）⇒ ⇒ 反过来，
            //      `child_instance_name` 非空 ⇔ 该边会走到行 1 / 行 2 分支
            //      （发射器 `:663-666` 的门在同一条件下**永不**触发 ⇒ ⇒ 无需复刻）
            if (e.child_instance_name.empty()) {
                continue;   // ★ 非相邻边 ⇒ 发射器走链式分支 ⇒ 不合成 `__if_`/`__x_`
            }
            if (!detail::implicit_input_synthesizable(e, an)) {
                continue;   // 行 1 不可合成 ⇒ 发射器不建端口 ⇒ 不计
            }
            std::string werr;
            if (!detail::synth_width_ok("implicit edge", e.from_ptr, e.to_ptr, werr)) {
                continue;   // 宽度不一致 ⇒ 合成**之前**就被拒 ⇒ 不建端口 ⇒ 不计
            }
            keys.insert(detail::synth_port_key(e));
        }
        diag.synthesized_port_count = keys.size();
    }

    // ── ⑤ 跨层悬空输入数（★ 口径同发射器：根自己的 / inline 折叠进根的**不计**）
    std::string root_key;
    for (const auto &m : an.modules) {
        if (m.is_root) {
            root_key = m.key;
        }
    }
    std::size_t dangling = 0;
    for (const auto &b : an.bubbled_inputs) {
        if (b.module == root_key) {
            continue;   // 不跨 module 边界 ⇒ 不属「隐式悬空」
        }
        ++dangling;
        diag.details.push_back(HierarchyDiagnosisDetail{"dangling_input", b.module,
                                                         vname(b.node),
                                                         "undriven input bubbles up"});
    }
    diag.dangling_input_count = dangling;

    // ── ⑥ 未被子层消费的输出数（★ 并集推导 + 四条排除规则 ⇒ **不**重遍历 IR）
    {
        // 反向索引：module key → (父 module, 实例名, 连接表)
        std::unordered_map<std::string,
                           std::vector<const HierarchyInstance *>>
            instances_of;
        for (const auto &m : an.modules) {
            for (const auto &inst : m.instances) {
                instances_of[inst.module_key].push_back(&inst);
            }
        }
        std::size_t unconsumed = 0;
        for (const auto &m : an.modules) {
            if (m.is_root) {
                continue;
            }
            const auto insts = instances_of.find(m.key);
            for (const auto &p : m.ports) {
                if (p.is_input) {
                    continue;
                }
                if (p.origin != HierarchyPortOrigin::Self &&
                    p.origin != HierarchyPortOrigin::InlineFolded) {
                    continue;
                }
                // ① `edges` 里有「从该端口出发」的跨 module 边 ⇒ 已消费
                bool consumed = false;
                for (const auto &e : an.edges) {
                    if (e.from_module == m.key && e.from_node == p.name) {
                        consumed = true;
                        break;
                    }
                }
                // ② `pending_implicit_edges` 里被隐式消费 ⇒ 已消费
                //    （★ **必须并集** —— 兄弟/非相邻边在分析层是 `continue`
                //    **在** `edges.push_back` **之前** ⇒ ⇒ 只看 `edges` 必假阳性）
                if (!consumed) {
                    for (const auto &pe : an.pending_implicit_edges) {
                        if (pe.from_module == m.key && pe.from_node == p.name) {
                            consumed = true;
                            break;
                        }
                    }
                }
                // ③ + ④ **按实例**判：★ 该端口 MUST 在**所有**相关实例上都被消费
                //    才算「已消费」⇒ ⇒ **不是**按 module 去重 —— 否则「多实例只有
                //    一个消费」会被误判成「已消费」（⇒ 掩盖了部分实例没人消费）
                //    ⇒ ★ 排除 `into_child == true` 的连接（那是父驱动子 ⇒ 不是
                //    「消费子输出」）
                //    ⚠️ 计数单位 = **端口**（★ 不是「实例-端口对」）
                //    ⚠️ **实施期实测**：本分支当前**不可达** —— 任何
                //    `into_child == false` 的 connection 都与一条 `an.edges` 条目
                //    **成对**产生（分析层 `:627-643`）⇒ ⇒ ① 必先匹配 ⇒ ⇒ 真正
                //    兜住「多实例只有一个消费」的是「**端口名全局唯一**」这条性质
                //    （两个实例的同名端口名字不同 ⇒ ⇒ 各自的边各自匹配）。
                //    ⇒ ⇒ 本分支**保留**为「edges 未记录该消费」时的兜底，
                //    但 **MUST NOT** 被当成「多实例防线」来依赖 ⇒ ⇒ 如实登记。
                if (!consumed && insts != instances_of.end()) {
                    bool all_consumed = true;
                    for (const auto *inst : insts->second) {
                        bool inst_consumed = false;
                        for (const auto &conn : inst->connections) {
                            if (!conn.into_child && conn.child_port == p.name) {
                                inst_consumed = true;
                                break;
                            }
                        }
                        if (!inst_consumed) {
                            all_consumed = false;   // ★ 任一实例没消费 ⇒ 整体未消费
                            break;
                        }
                    }
                    consumed = all_consumed;
                }
                if (!consumed) {
                    ++unconsumed;
                    diag.details.push_back(HierarchyDiagnosisDetail{
                        "unconsumed_child_output", m.key, p.name,
                        "child output is not consumed by its parent"});
                }
            }
        }
        diag.unconsumed_child_output_count = unconsumed;
    }

    // ── ⑦ 各层规模（★ 三个数**分开给**）
    diag.module_count = an.modules.size();
    for (const auto &m : an.modules) {
        HierarchyModuleSize s;
        s.module = m.key;
        s.port_count = m.ports.size();
        s.instance_count = m.instances.size();
        s.body_count = m.body.size();
        diag.module_sizes.push_back(std::move(s));
    }

    // ── ⑧ 隐式边的明细（★ 带 `reason` ⇒ 用户才知道该「加一个端口」还是
    //    「加一条 N 跳链」）
    for (const auto &e : an.pending_implicit_edges) {
        diag.details.push_back(HierarchyDiagnosisDetail{
            "implicit_edge", e.from_module, vname(e.from_ptr), e.reason});
    }

    // ── ⑨ 结论（★ 3 档）
    const bool any = (diag.implicit_edge_count.value_or(0) > 0) ||
                     (diag.synthesized_port_count.value_or(0) > 0) ||
                     (diag.dangling_input_count.value_or(0) > 0) ||
                     (diag.unconsumed_child_output_count.value_or(0) > 0) ||
                     (diag.cross_module_mem_count.value_or(0) > 0);
    diag.verdict = any ? HierarchyFriendliness::kNeedsWork : HierarchyFriendliness::kClean;
    return diag;
}

}   // namespace ch
