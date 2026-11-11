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
// ⚠️ `sanitize_verilog_name` 的**转义规则**（非法字符 / 数字开头）是从
//    `verilogwriter::sanitize_name` **逐字搬来**的 ⇒ 搬动 MUST NOT 改变输出
//    （否则既有 Verilog goldens / Verilator 缓存全部失效）。
//
// ⚠️ **2026-11-11 · 关键字表已补全**（`port-naming-ratchet` N3「⑥ 关键字表补全」）：
//    原表是**务实子集**（31 词）⇒ ⇒ `bit` / `time` / `event` / `table` 等
//    **合法 C++ 成员名会产出非法 Verilog**（如 `logic bit;` ⇒ 语法错误）。
//    现表 = IEEE 1800-2017 保留字全集（**246 词 · verilator 实测**）。
//    ⇒ ⇒ **实测对现网产物零影响**（8585 个标识符中 0 个与关键字完全相等）。

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
    // ⚠️ **关键字表 = IEEE 1800-2017 SystemVerilog 保留字全集**（2026-11-11 补全）。
    //    ⚠️ **实测来源**：逐词用本仓的 verilator（`build/verilator-install/bin/verilator`）
    //    实测「`logic <词>;` 是否报语法错误」⇒ ⇒ **245 个词被判定为保留字**
    //    （对照词 `zzznotakeyword` / `foo` / `my_port` 均通过 ⇒ ⇒ 探针有判别力）。
    //    ⇒ ⇒ **不是凭记忆抄的清单**，而是**本仓工具链的实际判定**。
    //    ⚠️ **零产物影响（实测）**：现网产物中抽取到的 8585 个标识符里，
    //    与本表**完全相等**的有 **0 个** ⇒ ⇒ 补全表**不改变任何现有端口名**。
    //    （节点名恒带 owner 路径前缀 ⇒ `top_time` ≠ 裸 `time` ⇒ ⇒ 无碰撞。）
    //    ⚠️ **`endcasex` / `endcasez` 不是保留字**（casex/casez 的合法变体）⇒ ⇒ 已排除。
    // ⚠️ **关键字表 = IEEE 1800-2017 SystemVerilog 保留字全集**（2026-11-11 补全）。
    //    ⚠️ **实测来源**：逐词用本仓的 verilator（`build/verilator-install/bin/verilator`）
    //    实测「`logic <词>;` 是否报语法错误」⇒ ⇒ **246 个词被判定为保留字**
    //    （对照词 `zzznotakeyword` / `foo` / `my_port` 均通过 ⇒ ⇒ 探针有判别力）。
    //    ⇒ ⇒ **不是凭记忆抄的清单**，而是**本仓工具链的实际判定**。
    //    ⚠️ **零产物影响（实测）**：现网产物中抽取到的 8585 个标识符里，
    //    与本表**完全相等**的有 **0 个** ⇒ ⇒ 补全表**不改变任何现有端口名**。
    //    （节点名恒带 owner 路径前缀 ⇒ `top_time` ≠ 裸 `time` ⇒ ⇒ 无碰撞。）
    //    ⚠️ **`endcasex` / `endcasez` 不是保留字**（casex/casez 的合法变体）⇒ ⇒ 已排除。
    static const std::unordered_set<std::string> keywords = {
        "accept_on", "alias", "always", "always_comb", "always_ff", "always_latch",
        "and", "assert", "assign", "assume", "automatic", "before", "begin", "bind",
        "bins", "binsof", "bit", "break", "buf", "bufif0", "bufif1", "byte", "case",
        "casex", "casez", "cell", "chandle", "checker", "class", "clocking", "cmos",
        "config", "const", "constraint", "context", "continue", "cover", "covergroup",
        "coverpoint", "cross", "deassign", "default", "defparam", "design", "disable",
        "dist", "do", "edge", "else", "end", "endcase", "endchecker", "endclass",
        "endclocking", "endconfig", "endfunction", "endgenerate", "endgroup",
        "endinterface", "endmodule", "endpackage", "endprimitive", "endprogram",
        "endproperty", "endsequence", "endspecify", "endtable", "endtask", "enum",
        "event", "expect", "export", "extends", "extern", "final", "first_match", "for",
        "force", "foreach", "forever", "fork", "forkjoin", "function", "generate",
        "genvar", "highz0", "highz1", "if", "iff", "ifnone", "ignore_bins",
        "illegal_bins", "implements", "implies", "import", "incdir", "include",
        "initial", "inout", "input", "inside", "instance", "int", "integer",
        "interconnect", "interface", "intersect", "join", "join_any", "join_none",
        "large", "let", "liblist", "library", "local", "localparam", "logic", "longint",
        "macromodule", "matches", "medium", "modport", "module", "nand", "negedge",
        "nettype", "new", "nexttime", "nmos", "nor", "noshowcancelled", "not", "notif0",
        "notif1", "null", "or", "output", "package", "packed", "parameter", "pmos",
        "posedge", "primitive", "priority", "program", "property", "protected", "pull0",
        "pull1", "pulldown", "pullup", "pulsestyle_ondetect", "pulsestyle_onevent",
        "pure", "rand", "randc", "randcase", "randsequence", "rcmos", "real",
        "realtime", "ref", "reg", "reject_on", "release", "repeat", "restrict",
        "return", "rnmos", "rpmos", "rtran", "rtranif0", "rtranif1", "s_always",
        "s_eventually", "s_nexttime", "s_until", "s_until_with", "scalared", "sequence",
        "shortint", "shortreal", "showcancelled", "signed", "small", "soft", "solve",
        "specify", "specparam", "static", "string", "strong", "strong0", "strong1",
        "struct", "super", "supply0", "supply1", "sync_accept_on", "sync_reject_on",
        "table", "tagged", "task", "this", "throughout", "time", "timeprecision",
        "timeunit", "tran", "tranif0", "tranif1", "tri", "tri0", "tri1", "triand",
        "trior", "trireg", "type", "typedef", "union", "unique", "unique0", "unsigned",
        "until", "until_with", "untyped", "use", "uwire", "var", "vectored", "virtual",
        "void", "wait", "wait_order", "wand", "weak", "weak0", "weak1", "while",
        "wildcard", "wire", "with", "within", "wor", "xnor", "xor"};
    if (keywords.find(sanitized) != keywords.end()) {
        sanitized += "_r";
    }

    return sanitized;
}

/// 判断一个节点**是不是「显式命名的端口」**。
///
/// ⚠️ **判据 = 节点类型是 input / output** **且** 名字的**最后一段**（`.` 之后）
///   **不是**框架的**默认名**（`unnamed_input` / `unnamed_output`）。
///   · `ch_logic_in` 的**默认**构造用 `"unnamed_input"`（`include/core/io.h:143`）
///     ⇒ ⇒ 未具名**输入**端口的节点名 = `<组件路径>.unnamed_input`；
///   · `ch_logic_out` 的**默认**构造用 `"unnamed_output"`（`include/core/io.h:50`）
///     ⇒ ⇒ 未具名**输出**端口的节点名 = `<组件路径>.unnamed_output`；
///   · `__io` 内的**显式** NSDMI（`ch_in<T> ident{"ident"};`）用用户给的串
///     ⇒ ⇒ 显式端口的节点名 = `<组件路径>.<用户串>`。
/// ⇒ ⇒ 标识符**不含** `.` ⇒ ⇒ 判据无歧义。
///
/// ⚠️ **踩坑记录（2026-11-11 · 实测）**：第一版只排除了 `unnamed_input`
///   ⇒ ⇒ **输出**端口的默认名 `unnamed_output` 被漏掉 ⇒ ⇒ 多个 `unnamed_output`
///   被误判成「显式名」⇒ ⇒ **假冲突**（`rv32i_pipeline.h` 的 `pipeline.unnamed_output`
///   与 `top.unnamed_output` 同 base）⇒ ⇒ 日志里刷出大量假 ERROR
///   ⇒ ⇒ 被 `verify_example_test_wiring.sh` 的 C3「跑但报错」判据抓到。
///
/// ⚠️ **为什么 MUST 限「端口」**（2026-11-11 · 实测踩坑）：
///   若把**任何**非默认名结尾的节点都当「显式名」⇒ ⇒ 框架节点
///   （`default_clock` / `default_reset`）也会**占位**⇒ ⇒ 未具名节点全体后移
///   ⇒ ⇒ **产物巨变**（实测：RV32I SoC 的层次化产物差异上千行）。
///   ⇒ ⇒ 限「端口」后 ⇒ ⇒ 现有端口**全部未具名** ⇒ ⇒ **零 churn**。
///
/// ⚠️ **已知边界**：用户**显式**把端口命名为 `"unnamed_input"` / `"unnamed_output"`
///   ⇒ ⇒ 与未具名**不可区分** ⇒ ⇒ 该端口会走计数器路径（拿不到「名字稳定」）。
inline bool is_explicit_named_port(const ch::core::lnodeimpl *node) {
    if (node == nullptr) {
        return false;
    }
    const auto t = node->type();
    if (t != ch::core::lnodetype::type_input && t != ch::core::lnodetype::type_output) {
        return false;
    }
    const std::string &node_name = node->name();
    const std::size_t dot = node_name.rfind('.');
    const std::string last = (dot == std::string::npos) ? node_name
                                                         : node_name.substr(dot + 1);
    return last != "unnamed_input" && last != "unnamed_output";
}

/// 为 `eval_order` 里的每个节点生成**全局唯一**的 Verilog 名。
///
/// ⚠️ `eval_order` MUST 是 `context::get_eval_list()`（拓扑序）—— 计数器按
/// base name 递增，换顺序就换名字。
///
/// ⚠️ **显式命名的端口绕开计数器**（2026-11-11 · `port-naming-ratchet` **N3**）：
///   **未具名**节点的序号由「**eval-list 里排在它前面的同名节点个数**」决定
///   ⇒ ⇒ **加一个无关节点**就可能让**后面**的端口全体**后移一位**
///   ⇒ ⇒ 端口名**不稳定** ⇒ ⇒ 「按名引用」的外部资产（TB / wrapper / 文档）
///   **静默失配**。
///   ⇒ ⇒ **显式命名的端口 MUST 绕开**计数器 ⇒ ⇒ 它的名字只由
///   「组件路径 + 用户串」决定 ⇒ ⇒ **对图的无关改动稳定**。
///
/// ⚠️ **冲突判定是「端口作用域」的局部判定**（metis 订正 ①）：
///   MUST NOT 复用层次化发射的 `kRejectDuplicates`（那是**整份设计**的拒绝策略
///   ⇒ ⇒ 会把「一个端口重名」升级成「整个产物发不出来」）⇒ ⇒ 本处只**点名报告**
///   ⇒ ⇒ 并**仍然给出唯一名**（退回计数器路径）⇒ ⇒ 产物永远**合法**。
///
/// ⚠️ **两遍扫描**（显式名必须**先占位**，否则后到的未具名节点会抢走同一个 base）：
///   ① 第一遍：收集**显式命名端口**的 base；
///   ② 第二遍：其余节点走计数器，但**跳过已被显式端口占用**的 base。
inline std::unordered_map<const ch::core::lnodeimpl *, std::string>
build_verilog_name_table(const std::vector<ch::core::lnodeimpl *> &eval_order) {
    std::unordered_map<const ch::core::lnodeimpl *, std::string> table;
    std::unordered_map<std::string, int> counts;
    table.reserve(eval_order.size());

    // ── ① 第一遍：显式命名端口**占位** ───────────────────────────────
    std::unordered_set<std::string> reserved;
    for (ch::core::lnodeimpl *node : eval_order) {
        if (!is_explicit_named_port(node)) {
            continue;
        }
        const std::string base = sanitize_verilog_name(node->name());
        if (!reserved.insert(base).second) {
            // ⚠️ **端口作用域**的局部判定：点名报告，但不拒绝整份设计
            CHERROR("端口命名冲突：两个显式命名端口 sanitize 成同一个 Verilog 名 '%s'"
                    "（节点 '%s'）⇒ ⇒ 后者退回计数器路径拿唯一名",
                    base.c_str(), node->name().c_str());
        }
    }
    // 被显式端口占用的 base ⇒ ⇒ 计数器**从 1 起**（未具名节点不得抢名）
    for (const std::string &base : reserved) {
        counts[base] = 1;
    }

    // ── ② 第二遍：发名 ─────────────────────────────────────────────
    std::unordered_set<std::string> claimed_by_explicit;
    for (ch::core::lnodeimpl *node : eval_order) {
        if (node == nullptr) {
            continue;
        }
        const std::string base = sanitize_verilog_name(node->name());
        if (is_explicit_named_port(node) && claimed_by_explicit.insert(base).second) {
            // 显式命名端口 ⇒ ⇒ **绕开计数器** ⇒ ⇒ 名字稳定
            table[node] = base;
            continue;
        }
        // 其余（含**冲突**的显式端口）走计数器 ⇒ ⇒ 名字仍然唯一 ⇒ 产物合法
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
