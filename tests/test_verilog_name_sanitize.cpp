// tests/test_verilog_name_sanitize.cpp
//
// `sanitize_verilog_name` 的**关键字转义**判据
// （`port-naming-ratchet-2026-11.md` N3「⑥ 关键字表补全」的判据）。
//
// ⚠️ **为什么需要独立文件**（不并入 `test_component_naming.cpp`）：
//   后者判的是「**组件路径**的拼接口径」（ADR-059 D3/D4）⇒ ⇒ 判别力 = 前缀重复。
//   本文件判的是「**Verilog 标识符**的合法性」⇒ ⇒ 判别力 = 产出的 Verilog 能否被
//   verilator 解析 ⇒ ⇒ 两者是**不同的缺陷族** ⇒ ⇒ 混在一起会让「哪个缺陷族转红」
//   不可归因 ⇒ ⇒ 故独立。
//
// ⚠️ **判别力**（本文件 MUST 抓住的具体缺陷）：
//   修复前关键字表是**务实子集**（31 词）⇒ ⇒ `bit` / `time` / `event` / `table`
//   **不在表里** ⇒ ⇒ 一个**合法 C++ 成员名** `bit` 会产出**非法 Verilog**
//   （`logic bit;` ⇒ verilator: `syntax error, unexpected bit`）
//   ⇒ ⇒ 本文件 MUST 转红。
//
// ⚠️ **为什么判据是「单元判据」而不是「产物 diff」**：
//   现网产物里的节点名**恒带 owner 路径前缀** ⇒ ⇒ `top_time` ≠ 裸 `time`
//   ⇒ ⇒ 即便把表补全到全集，**产物也一个字节都不会变** ⇒ ⇒ ⇒
//   用「产物逐字节比对」做判据 = **恒绿**（零判别力）⇒ ⇒ MUST 用单元判据。

#include "catch_amalgamated.hpp"

#include "codegen_naming.h"

#include <set>
#include <string>

namespace {

// ── 判据 1：草案点名的 4 个词 MUST 被转义 ───────────────────────────────
// （`port-naming-ratchet-2026-11.md` metis finding 8 点名的缺口词）
TEST_CASE("sanitize_verilog_name: 草案点名的 4 个缺口关键字 MUST 被转义", "[codegen][naming]") {
    REQUIRE(ch::sanitize_verilog_name("bit") == "bit_r");
    REQUIRE(ch::sanitize_verilog_name("time") == "time_r");
    REQUIRE(ch::sanitize_verilog_name("event") == "event_r");
    REQUIRE(ch::sanitize_verilog_name("table") == "table_r");
}

// ── 判据 2：转义 MUST 是**自反可判定**的 ───────────────────────────────
// （`port-naming-ratchet-2026-11.md` :259 的订正：可判条件从正则改为
//   **自反检查** `sanitize(名) == 名` ⇒ ⇒ 合法名 MUST 原样返回）
TEST_CASE("sanitize_verilog_name: 合法标识符 MUST 原样返回（自反）", "[codegen][naming]") {
    for (const char *name : {"prog_we", "data_out", "valid", "ready", "my_port", "_x9", "a1_b2"}) {
        CAPTURE(name);
        REQUIRE(ch::sanitize_verilog_name(name) == std::string(name));
    }
}

// ── 判据 3：非法字符 / 数字开头的规则 MUST NOT 被关键字改动影响 ─────────
// （这两条规则是从 `verilogwriter::sanitize_name` 逐字搬来的 ⇒ ⇒ 搬动 MUST NOT 改变输出）
TEST_CASE("sanitize_verilog_name: 非法字符与数字开头的既有规则 MUST NOT 变", "[codegen][naming]") {
    REQUIRE(ch::sanitize_verilog_name("a-b") == "a_b");
    REQUIRE(ch::sanitize_verilog_name("a.b") == "a_b");
    REQUIRE(ch::sanitize_verilog_name("1abc") == "_1abc");
    REQUIRE(ch::sanitize_verilog_name("") == "");
}

// ── 判据 4：**非**保留字 MUST NOT 被转义（反向判据）────────────────────
// ⚠️ **反向护栏**：只查「关键字被转义」的话，**为让判据绿而误加词表**同样会绿
//   ⇒ ⇒ 必须同时有「不该转的**不能**转」这一面。
//   ⚠️ `endcasex` / `endcasez` 是 casex/casez 的**合法变体**⇒ ⇒ 实测不是保留字
//   ⇒ ⇒ 它们 MUST 原样返回（若被误加进表 ⇒ ⇒ 本判据转红）。
TEST_CASE("sanitize_verilog_name: 非保留字 MUST NOT 被转义（反向判据）", "[codegen][naming]") {
    for (const char *name : {"endcasex", "endcasez", "global", "xorendgenerate",
                             "zzznotakeyword", "my_module", "bit_width", "timed_out"}) {
        CAPTURE(name);
        REQUIRE(ch::sanitize_verilog_name(name) == std::string(name));
    }
}

// ── 判据 5：关键字表 MUST 覆盖 IEEE 1800-2017 的**核心**保留字 ─────────
// （不是穷举 —— 穷举 245 词会让本文件变成一张快照表 ⇒ ⇒ 只钉住**每一类**的代表）
TEST_CASE("sanitize_verilog_name: 关键字表 MUST 覆盖各语法类的代表词", "[codegen][naming]") {
    // 网表 / 声明 / 连续赋值 / 过程控制 / 分支 / 循环 / 任务函数 / 类与包
    // / 时序检查 / 强度与驱动 / 三态 / 配置 / 生命周期 / 断言
    for (const char *kw : {"wire", "logic", "reg", "assign", "always", "always_ff",
                           "always_comb", "initial", "begin", "end", "if", "else",
                           "case", "endcase", "for", "forever", "repeat", "while",
                           "task", "endtask", "function", "endfunction", "class",
                           "endclass", "package", "endpackage", "assert", "assume",
                           "supply0", "supply1", "tri0", "tri1", "triand", "trior",
                           "config", "endconfig", "program", "endprogram", "generate",
                           "endgenerate", "genvar", "signed", "unsigned", "integer",
                           "real", "shortint", "int", "byte", "string", "void", "null",
                           "wait", "fork", "join", "disable", "force", "release",
                           "deassign", "edge", "posedge", "negedge", "highz0", "pull0",
                           "small", "medium", "large", "rand", "randc", "randcase",
                           "constraint", "cover", "property", "sequence", "typedef",
                           "enum", "union", "struct", "virtual", "interface",
                           "modport", "clocking", "specify", "endspecify", "s_always",
                           "s_eventually", "s_until", "s_nexttime", "accept_on",
                           "reject_on", "sync_accept_on", "expect", "restrict",
                           "randsequence", "solve", "matches", "inside", "dist",
                           "unique", "unique0", "priority"}) {
        CAPTURE(kw);
        REQUIRE(ch::sanitize_verilog_name(kw) == std::string(kw) + "_r");
    }
}

// ── 判据 6：转义 MUST 是**幂等**的（转义后的名字再转义 MUST 不再变）────
// ⚠️ 这条防的是「`_r` 后缀的名字又被当成关键字」的类缺陷
//   （例如若表里混进了 `bit_r`，则 `bit` → `bit_r` → `bit_r_r` ⇒ ⇒ 名会漂移）。
TEST_CASE("sanitize_verilog_name: 关键字转义 MUST 幂等", "[codegen][naming]") {
    for (const char *kw : {"bit", "time", "event", "table", "wire", "reg", "logic",
                           "always", "assign", "input", "output"}) {
        CAPTURE(kw);
        const std::string once = ch::sanitize_verilog_name(kw);
        REQUIRE(once == std::string(kw) + "_r");
        REQUIRE(ch::sanitize_verilog_name(once) == once);
    }
}

}   // namespace
