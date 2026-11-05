// src/hierarchy_port_check.h
//
// 端口完整性的**机械判据**（ADR-058 v1.12 §2.3 的 G2 · ADR-062 §5）。
//
// 判据：`expected ⊆ actual`（**子集**不是判等 —— 多余端口由「名字唯一性」+
// 「`ModuleView` 只由发射循环构造」这两条不变量覆盖）+ **同 module 内名字唯一**。
//
// ⚠️ 纯函数（无 I/O、无 IR）⇒ ⇒ 可单测；发射器 MUST 在**第一次 I/O 之前**调用它。
// ⚠️ 比对**名字 + 方向 + 宽度**三项（只比名字 ⇒ 「方向反了」这种真缺陷会漏）。

#pragma once

#include "core/lnodeimpl.h"
#include "hierarchy_verilog.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace ch {
namespace detail {

/// 返回空串 = 通过；否则返回**点名 module + 端口**的诊断。
inline std::string check_port_table(const std::vector<PortProvenance> &expected,
                                    const std::vector<PortProvenance> &actual) {
    for (const auto &e : expected) {
        bool found = false;
        for (const auto &a : actual) {
            if (a.module != e.module || a.port != e.port) {
                continue;
            }
            if (a.is_input != e.is_input || a.width != e.width) {
                return "port '" + e.port + "' of module '" + e.module +
                       "' has inconsistent direction/width";
            }
            found = true;
            break;
        }
        if (!found) {
            return "port '" + e.port + "' of module '" + e.module +
                   "' is missing from the emitted port table";
        }
    }
    for (std::size_t i = 0; i < actual.size(); ++i) {
        for (std::size_t j = i + 1; j < actual.size(); ++j) {
            if (actual[i].module == actual[j].module && actual[i].port == actual[j].port) {
                return "duplicate port '" + actual[i].port + "' in module '" +
                       actual[i].module + "'";
            }
        }
    }
    return {};
}

/// **接口白名单**（ADR-062 §2.6）的可判定形态：端口节点 MUST 是
/// **数据端口**（`ch_uint` / `ch_bool` ⇒ `type_input` / `type_output`）或
/// **时序基础设施**（`type_clock` / `type_reset`）。
///
/// ⚠️ 其余节点类型若出现在端口表 ⇒ 拒绝并点名：
/// `type_mem` / `type_mem_read_port` / `type_mem_write_port`（数组无端口形态）·
/// `type_reg`（状态不是接口）· `type_proxy`（bundle 成员不是端口）·
/// `type_op` / `type_mux` / `type_bitsupdate` / `type_lit`（体内逻辑）。
///
/// ⚠️ **可达性现状（2026-11-05 · B3c spike）**：分析层的端口循环只从
/// `type_input` / `type_output` / `clock` / `reset` 构造端口表 ⇒ ⇒ 上面这些类型
/// **当前没有一条公开 API 路径**能进端口表（详见 change 的 tasks T0l）。
/// 本守卫是**防御性**的：它保证「将来有人把 mem 读端口 / 寄存器 / bundle 切片
/// 抬成端口」时**立刻响亮失败**，而不是静默产出非法接口。
inline bool is_whitelisted_port_node(core::lnodetype ty) {
    switch (ty) {
    case core::lnodetype::type_input:
    case core::lnodetype::type_output:
    case core::lnodetype::type_clock:
    case core::lnodetype::type_reset:
        return true;
    default:
        return false;
    }
}

/// 返回空串 = 通过；否则点名 `module` + `port`（ADR-062 §2.6 的「点名端口」）。
inline std::string
check_interface_whitelist(const std::string &module,
                          const std::vector<std::pair<std::string, core::lnodetype>> &ports) {
    for (const auto &p : ports) {
        if (!is_whitelisted_port_node(p.second)) {
            return "port '" + p.first + "' of module '" + module +
                   "' has a non-whitelisted interface type (ADR-062 §2.6: only "
                   "ch_uint/ch_bool data ports plus clock/reset are allowed)";
        }
    }
    return {};
}

}   // namespace detail
}   // namespace ch
