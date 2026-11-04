// include/codegen_naming.h
//
// Verilog 命名规则的**唯一真源**（扁平发射器 `verilogwriter` 与层次化分析层共用）。
//
// ⚠️ 为什么必须共用（ADR-061 决策④「命名权威 = ctx 级全局表」）：
//    · 扁平产物里 `ch_in` 的默认名都是 `unnamed_input` ⇒ 必须**全局去重**
//      （计数器后缀 `_1` / `_2` …）；
//    · 层次化产物 MUST 让顶层端口名与扁平产物**逐字相同** ⇒ ⇒ 两处若各写一份
//      去重逻辑，任一处漂移都会让「等价性」判据变成**自定义的空话**；
//    · 去重按 `ctx->get_eval_list()` 的拓扑序累积 ⇒ ⇒ **顺序敏感**
//      ⇒ ⇒ 两处 MUST 用同一个列表、同一个算法。
//
// ⚠️ 本文件只做**命名**：不读 IR 结构、不改 IR。
// ⚠️ `sanitize_verilog_name` 的规则表是从 `verilogwriter::sanitize_name`
//    **逐字搬来**的（含关键字清单）⇒ 搬动 MUST NOT 改变输出（否则既有
//    Verilog goldens / Verilator 缓存全部失效）。

#pragma once

#include "core/lnodeimpl.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ch {

/// Verilog 标识符清洗 + 关键字转义。
///
/// · 非法字符（非字母数字下划线）⇒ `_`；
/// · 首字符为数字 ⇒ 前缀 `_`；
/// · Verilog/SystemVerilog 保留字 ⇒ 后缀 `_r`（务实子集，覆盖真实冲突，
///   例如 `ch_reg` 的默认名 `reg` 否则会发成非法的 `logic [7:0] reg;`）。
inline std::string sanitize_verilog_name(const std::string &name) {
    std::string sanitized = name;
    // Replace illegal characters with underscores
    std::replace_if(
        sanitized.begin(), sanitized.end(),
        [](char c) { return !std::isalnum(c) && c != '_'; }, '_');

    // Ensure it doesn't start with a digit
    if (!sanitized.empty() && std::isdigit(sanitized[0])) {
        sanitized = "_" + sanitized;
    }

    // Escape Verilog/SystemVerilog reserved keywords by appending "_r" suffix.
    // Pragmatic subset of IEEE 1800-2017 reserved words covering real
    // collisions (e.g., ch_reg's default name "reg" would otherwise emit an
    // illegal "logic [7:0] reg;" declaration).
    static const std::unordered_set<std::string> keywords = {
        "reg", "wire", "logic", "assign", "module", "endmodule",
        "input", "output", "inout", "always", "always_ff", "always_comb",
        "initial", "begin", "end", "if", "else", "case", "endcase",
        "parameter", "localparam", "signed", "unsigned", "genvar",
        "generate", "endgenerate", "tri", "wand", "wor", "supply0",
        "supply1"};
    if (keywords.find(sanitized) != keywords.end()) {
        sanitized += "_r";
    }

    return sanitized;
}

/// 为 `eval_order` 里的每个节点生成**全局唯一**的 Verilog 名。
///
/// ⚠️ `eval_order` MUST 是 `context::get_eval_list()`（拓扑序）—— 计数器按
/// base name 递增，换顺序就换名字。
inline std::unordered_map<const ch::core::lnodeimpl *, std::string>
build_verilog_name_table(const std::vector<ch::core::lnodeimpl *> &eval_order) {
    std::unordered_map<const ch::core::lnodeimpl *, std::string> table;
    std::unordered_map<std::string, int> counts;
    table.reserve(eval_order.size());
    for (ch::core::lnodeimpl *node : eval_order) {
        if (node == nullptr) {
            continue;
        }
        const std::string base = sanitize_verilog_name(node->name());
        std::string unique = base;
        const int counter = counts[base]++;
        if (counter > 0) {
            unique = base + "_" + std::to_string(counter);
        }
        table[node] = std::move(unique);
    }
    return table;
}

}   // namespace ch
