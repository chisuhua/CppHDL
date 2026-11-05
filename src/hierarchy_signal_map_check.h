// src/hierarchy_signal_map_check.h
//
// **信号映射表**的机械判据（ADR-058 §2.2 · T3-⑤ B1）。
//
// 判据（全部**内在一致** ⇒ 不需要外部参照物）：
//   ① `node_id` **唯一**（同一信号 MUST NOT 出现两行）；
//   ② **顶层端口全覆盖**：每个顶层端口 MUST 有一条目，且 `hier_top_port_name`
//      与之**逐字相同**（⇒ 顶层可观测的信号一个都不能漏）；
//   ③ `flat.is_port` ⇔ `hier_top_port_name` **非空**（⇒ 「在扁平里是顶层端口」
//      与「在层次化顶层可见」这两件事 MUST 同时成立或同时不成立）；
//   ④ `hier_top_port_name` 非空时 MUST 是**顶层端口之一**（⇒ 不许凭空写一个名字）；
//   ⑤ 每个条目 MUST 至少有一个**层次化位置**（⇒ 不许有「查不到在哪」的条目）；
//   ⑥ `hier_top_port_name` 非空 ⇒ MUST **真的**有一个层次化位置
//      `{root_module, 该端口名, is_port = true}`（⇒ 「声称在顶层却查不到位置」必红）；
//   ⑦ `hier_top_port_name` 为空 ⇒ MUST **没有**任何 `{root_module, .., is_port = true}`
//      的位置（⇒ 「实际在顶层却写成空」必红 —— 空名合法与意外为空必须可区分）。
//
// ⚠️ 纯函数（无 I/O、无 IR）⇒ ⇒ 可单测；发射器 MUST 在**第一次 I/O 之前**调用它
//    （⇒ 表不可信 ⇒ **不产出文件**，fail-closed）。
// ⚠️ 「顶层端口」与「根 module 名」由**调用方**给出（= 根 module 的端口表）⇒ ⇒ 本判据
//    **不复述**发射器的端口逻辑（否则两处漂移 ⇒ 判据自证）。

#pragma once

#include "codegen_verilog.h"
#include "hierarchy_verilog.h"

#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace ch {
namespace detail {

/// 返回空串 = 通过；否则返回**点名信号**的诊断。
inline std::string check_signal_map(const std::vector<SignalMapEntry> &entries,
                                     const std::vector<std::string> &top_ports,
                                     const std::string &root_module) {
    // ① `node_id` 唯一
    std::set<std::uint32_t> seen_ids;
    for (const auto &e : entries) {
        if (!seen_ids.insert(e.node_id).second) {
            return "signal map has duplicate node_id " + std::to_string(e.node_id) +
                   " (signal '" + e.signal + "')";
        }
    }
    // ② 顶层端口全覆盖（**双向**：漏 ⇒ 红；多 ⇒ 红）
    std::set<std::string> top_set(top_ports.begin(), top_ports.end());
    for (const auto &p : top_ports) {
        bool found = false;
        for (const auto &e : entries) {
            if (e.hier_top_port_name == p) {
                found = true;
                break;
            }
        }
        if (!found) {
            return "top-level port '" + p +
                   "' has no entry in the signal map (the map must cover every observable "
                   "signal)";
        }
    }
    for (const auto &e : entries) {
        // ③ `flat.is_port` ⇔ `hier_top_port_name` 非空
        if (e.flat.is_port != !e.hier_top_port_name.empty()) {
            return "signal '" + e.signal +
                   "' disagrees between its flat position and its hierarchical top port";
        }
        // ④ 顶层端口名 MUST 是真端口
        if (!e.hier_top_port_name.empty() && top_set.count(e.hier_top_port_name) == 0) {
            return "signal '" + e.signal + "' claims top-level port '" + e.hier_top_port_name +
                   "', which is not a top-level port";
        }
        // ⑤ 至少一个层次化位置
        if (e.hier.empty()) {
            return "signal '" + e.signal + "' has no hierarchical location";
        }
        // ⑥ 声称在顶层 ⇒ MUST 真的有那个位置
        bool has_top_position = false;
        bool has_root_port_position = false;
        for (const auto &loc : e.hier) {
            if (loc.module != root_module || !loc.is_port) {
                continue;
            }
            has_root_port_position = true;
            if (loc.name == e.hier_top_port_name) {
                has_top_position = true;
            }
        }
        if (!e.hier_top_port_name.empty() && !has_top_position) {
            return "signal '" + e.signal + "' claims top-level port '" + e.hier_top_port_name +
                   "', but its hierarchical locations contain no such port in module '" +
                   root_module + "'";
        }
        // ⑦ 顶层端口名为空 ⇒ MUST 真的不在顶层（区分「合法空」与「意外空」）
        if (e.hier_top_port_name.empty() && has_root_port_position) {
            return "signal '" + e.signal + "' has a port in the top-level module '" + root_module +
                   "' but its top-level port name is empty";
        }
        // 扁平侧的 module 名 MUST 是扁平产物的真名（⇒ 「扁平位置」不许漂）
        if (e.flat.module != kFlatModuleName) {
            return "signal '" + e.signal + "' points at flat module '" + e.flat.module +
                   "', but the flat artifact's module is '" + std::string(kFlatModuleName) + "'";
        }
    }
    return {};
}

}   // namespace detail
}   // namespace ch
