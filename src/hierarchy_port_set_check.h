// src/hierarchy_port_set_check.h
//
// **顶层端口集合**的两条精确判据（ADR-058 §2.2 ②③）。
//
// ② **输入端口集合 MUST 相等**（名字 + 宽度逐项配对）；
// ③ **输出端口集合 MUST 等于精确期望集** = 扁平输出集 − {`ch_out` 的最近成层祖先 ≠ 根 的全部节点}。
//
// ⚠️ **为什么不用 `⊆`**：丢端口正是**子集方向** ⇒ ⇒ 只判「期望集 ⊆ 实际集」
//    ⇒ 漏发一半端口也能过 ⇒ ⇒ 本文件的两条判据都是**双向比对**（缺 ⇒ 红 · 多 ⇒ 红）。
// ⚠️ **为什么期望集要「手推」**：`moved_out`（被移出顶层的那批输出）**MUST NOT** 从
//    层次化产物反推（那会变成「实际集 − 实际集」的**循环定义** ⇒ 恒真 ⇒ 无判别力）
//    ⇒ ⇒ 它 MUST 由**独立的参照物**给出（当前 = 单测里**手推**的期望集；
//    「不减法」这类缺陷只有这样才抓得住）。
// ⚠️ 纯函数（无 I/O、无 IR）⇒ ⇒ 可单测；**顺序不参与**比对（端口顺序是另一条判据）。
//
// ⚠️ 「扁平侧」的端口表 MUST 来自**扁平发射器自己的视图**
//    （`make_flat_module_view`）⇒ ⇒ 与层次化侧是**不同代码路径** ⇒ ⇒
//    「归属 / 层判定」类缺陷对它**盲**（= 有判别力）。

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ch {
namespace detail {

/// 端口集合的一行（判据用 · **value-only**）。
struct PortSetEntry {
    std::string name;
    bool is_input = false;
    std::uint32_t width = 0;
};

/// 判据 ②：输入端口集合 MUST **相等**（名字 + 宽度逐项配对 · 双向）。
/// 返回空串 = 通过；否则点名缺失 / 多余的那一个。
inline std::string check_input_port_sets(const std::vector<PortSetEntry> &flat,
                                         const std::vector<PortSetEntry> &hier) {
    for (const auto &f : flat) {
        if (!f.is_input) {
            continue;
        }
        bool found = false;
        for (const auto &h : hier) {
            if (h.name != f.name) {
                continue;
            }
            if (!h.is_input) {
                return "flat input '" + f.name + "' is an output in the hierarchical port table";
            }
            if (h.width != f.width) {
                return "input '" + f.name + "' has width " + std::to_string(h.width) +
                       " in the hierarchical port table but " + std::to_string(f.width) +
                       " in the flat one";
            }
            found = true;
            break;
        }
        if (!found) {
            return "input '" + f.name +
                   "' is missing from the hierarchical top-level port table (the port sets MUST "
                   "be EQUAL, not a subset)";
        }
    }
    for (const auto &h : hier) {
        if (!h.is_input) {
            continue;
        }
        bool found = false;
        for (const auto &f : flat) {
            if (f.name == h.name && f.is_input) {
                found = true;
                break;
            }
        }
        if (!found) {
            return "input '" + h.name +
                   "' is in the hierarchical top-level port table but not in the flat one";
        }
    }
    return {};
}

/// 判据 ③：层次化顶层输出集合 MUST **等于**「扁平输出集 − 被移出顶层的那批」。
///
/// @param flat     扁平产物的顶层端口（**输出与输入都可放**；本判据只看输出）
/// @param hier     层次化顶层 module 的端口（只看输出）
/// @param moved_out **手推**的「被移出顶层」期望集（= 最近成层祖先 ≠ 根的那些 `ch_out`）
inline std::string check_output_port_set(const std::vector<PortSetEntry> &flat,
                                          const std::vector<PortSetEntry> &hier,
                                          const std::vector<PortSetEntry> &moved_out) {
    // 期望集 = 扁平输出集 − 被移出顶层的那批
    std::vector<PortSetEntry> expected;
    for (const auto &f : flat) {
        if (f.is_input) {
            continue;
        }
        bool moved = false;
        for (const auto &m : moved_out) {
            if (m.name == f.name) {
                moved = true;
                break;
            }
        }
        if (!moved) {
            expected.push_back(f);
        }
    }
    // 期望 ⊆ 实际（缺 ⇒ 红）
    for (const auto &e : expected) {
        bool found = false;
        for (const auto &h : hier) {
            if (h.name != e.name) {
                continue;
            }
            if (h.is_input) {
                return "flat output '" + e.name + "' is an input in the hierarchical port table";
            }
            if (h.width != e.width) {
                return "output '" + e.name + "' has width " + std::to_string(h.width) +
                       " in the hierarchical port table but " + std::to_string(e.width) +
                       " in the flat one";
            }
            found = true;
            break;
        }
        if (!found) {
            return "output '" + e.name +
                   "' is missing from the hierarchical top-level port table";
        }
    }
    // 实际 ⊆ 期望（多 ⇒ 红）⇒ ⇒ 「不减法」的缺陷在这里被抓
    for (const auto &h : hier) {
        if (h.is_input) {
            continue;
        }
        bool in_expected = false;
        for (const auto &e : expected) {
            if (e.name == h.name) {
                in_expected = true;
                break;
            }
        }
        if (!in_expected) {
            return "output '" + h.name +
                   "' is in the hierarchical top-level port table but must not be (it belongs to "
                   "a layered subtree and MUST be moved out of the top level)";
        }
    }
    return {};
}

}   // namespace detail
}   // namespace ch
