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

#include "hierarchy_verilog.h"

#include <cstddef>
#include <string>
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

}   // namespace detail
}   // namespace ch
