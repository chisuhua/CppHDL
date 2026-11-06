#ifndef CH_SRC_HIERARCHY_SYNTH_COUNT_H
#define CH_SRC_HIERARCHY_SYNTH_COUNT_H

// src/hierarchy_synth_count.h
//
// 隐式边**合成端口去重口径**的**单一实现**（★ 发射器与诊断**共用**）。
//
// ⚠️ **为什么要有这个内部头**（capability `hierarchy-friendliness-diagnosis` 的
//    Requirement 3「诊断 MUST 与发射器同源」）：
//    诊断要报「需合成多少个**端口**」⇒ ⇒ 该数量 MUST 与发射产物里
//    `origin == Synthesized` 的隐式合成端口数**相等** ⇒ ⇒ 若诊断**自己**复刻
//    「哪个端点是子侧」「行 1 是否可合成」⇒ ⇒ **第二份判定** ⇒ ⇒ 与发射器漂移
//    ⇒ ⇒ 违反「同源」契约。
//    ⇒ ⇒ 故把**去重 key** 与**可合成性判定**抽到这里 ⇒ ⇒ 两侧共用同一份。
//
// ⚠️ **为什么是 `src/` 下的内部头**（**不是** `include/`）：
//    ★ 仓内已有同款先例 —— `src/hierarchy_chain.h` · `src/hierarchy_port_check.h` ·
//      `src/hierarchy_port_set_check.h` · `src/hierarchy_signal_map_check.h`
//    ⇒ ⇒ **公开 API 面零扩张**（`include/` 下不新增任何东西）。
//
// ⚠️ **调用方必须两侧都用**（发射器 `src/hierarchy_verilog.cpp` + 诊断
//    `src/hierarchy_analysis.cpp`）⇒ ⇒ 否则又会退化成两份实现。

#include "hierarchy_analysis.h"

#include "core/lnodeimpl.h"   // ★ 需要 `lnodeimpl::type()` ⇒ 完整类型

#include <string>
#include <utility>

namespace ch::detail {

/// 隐式边的**合成端口去重 key** = **(子侧 module key, 子侧节点指针)**。
///
/// ⚠️ **子侧 = 合成端口所在的那个 module 的端点**：
/// · 行 2（`child_drives_parent == true`，父层读子内部节点 ⇒ 合成**子侧 output**）
///   ⇒ 子侧 = **驱动端**（`from_module` + `from_ptr`）
/// · 行 1（`child_drives_parent == false`，父层驱动子内部节点 ⇒ 合成**子侧 input**）
///   ⇒ 子侧 = **被驱动端**（`to_module` + `to_ptr`）
///
/// ⇒ ⇒ **同一子 module 的同一节点只合成一个端口** ⇒ ⇒ 端口数 = key 的**去重**数量。
inline std::pair<std::string, const core::lnodeimpl *>
synth_port_key(const PendingImplicitEdge &edge) {
    if (edge.child_drives_parent) {
        return {edge.from_module, edge.from_ptr};
    }
    return {edge.to_module, edge.to_ptr};
}

/// 行 1（父驱子）的**可合成性**判定（★ 与发射器同款 ⇒ 单一判定实现）。
///
/// ⚠️ **可合成的子侧端点 = 有独立 driver 槽位的节点**（ADR-060 v1.9 §2.1 行 1）：
/// · `type_reg`（`next` 槽位 · `print_reg` 重定向）
/// · `type_proxy`（bundle 字段 · `print_proxy` 重定向）
/// ⚠️ **MUST NOT** 收 `type_output`（父驱动子 `ch_out` ⇒ 拒绝）/ `type_op`
///   （改写运算操作数 ⇒ 拒绝）—— ⇒ 那些边由发射器交给 `PortPolicy` 处置
///   ⇒ ⇒ **不**合成端口 ⇒ ⇒ 本判定为 false ⇒ ⇒ 诊断**不**把它们计入端口数。
///
/// ⚠️ 另需「驱动端在**父 module** 内」（`driver_in_parent`）⇒ ⇒ 否则不是
///   「父层驱动子」这一形态。
inline bool implicit_input_synthesizable(const PendingImplicitEdge &edge,
                                         const HierarchyAnalysis &analysis) {
    if (edge.child_drives_parent) {
        return true;   // 行 2 无此判定（子侧合成 output）
    }
    if (edge.to_ptr == nullptr || edge.from_ptr == nullptr) {
        return false;
    }
    const auto child_type = edge.to_ptr->type();
    const bool child_is_redirectable =
        (child_type == core::lnodetype::type_reg ||
         child_type == core::lnodetype::type_proxy);
    if (!child_is_redirectable) {
        return false;
    }
    const auto drv_mod = analysis.module_of_node.find(edge.from_ptr);
    const bool driver_in_parent =
        drv_mod != analysis.module_of_node.end() && drv_mod->second == edge.from_module;
    return driver_in_parent;
}

/// **W1**（ADR-060 v1.10 · v1.13 明确覆盖**全部**合成 site）：
/// 跨层边两端宽度 MUST 一致；不一致 ⇒ **合成之前**响亮拒绝。
///
/// ⚠️ **为什么也在这个共享头里**（发射器原本是内联 lambda）：
///    诊断要报「需合成多少个端口」⇒ ⇒ 该数量 MUST 与发射器**相等**
///    ⇒ ⇒ 宽度不一致的边**不**合成端口 ⇒ ⇒ 诊断 MUST 用**同一份**判定排除它们
///    ⇒ ⇒ 否则诊断会**多算** ⇒ ⇒ 违反「与发射器同源」。
/// ⚠️ **错误消息逐字保留**（★ 发射器原本的措辞）⇒ ⇒ 既有产物/测试不受影响。
inline bool synth_width_ok(const std::string &desc, const core::lnodeimpl *from,
                           const core::lnodeimpl *to, std::string &err) {
    if (from->size() == to->size()) {
        return true;
    }
    err = desc + ": cross-module edge width mismatch (" + std::to_string(from->size()) +
          "-bit driver -> " + std::to_string(to->size()) +
          "-bit target). ADR-060 W1 forbids implicit extension/truncation (the output "
          "disables WIDTHEXPAND/WIDTHTRUNC). Remedy: slice or extend explicitly on the "
          "parent side, or drive an explicit port of the child instead.";
    return false;
}

}  // namespace ch::detail

#endif  // CH_SRC_HIERARCHY_SYNTH_COUNT_H
