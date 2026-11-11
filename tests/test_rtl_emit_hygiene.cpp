// tests/test_rtl_emit_hygiene.cpp
//
// **产物卫生**（`rtl-emit-product-tooling` **C6**）的判据。
//
// ⚠️ **判别力**（本文件 MUST 抓住的具体缺陷）：
//   ① **默认路径被改动** ⇒ ⇒ `emit_hygiene` **默认 off** 时产物 MUST
//      **逐字节不变**（批次表 ★3：「层次化产物逐字节不变」这条 AC **只在**该开关
//      默认 off 时成立）⇒ ⇒ 若默认路径变了 ⇒ ⇒ 本文件转红；
//   ② **卫生项没生效** ⇒ ⇒ 开关 on 时 MUST：`` `default_nettype none `` 有 ·
//      blanket `lint_off` **无** · **零引用信号数 == 0** · **恒等直通数 == 0**；
//   ③ **剪枝误伤** ⇒ ⇒ 剪掉「跨 module 实例连接」的端点会让实例连接**静默断线**
//      ⇒ ⇒ 本文件用**两层设计**验实例语句仍在（保护规则的判别性）。
//
// ⚠️ **期望值 MUST 手推**（禁抄仿真输出）。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"
#include "hierarchy_verilog.h"

#include <fstream>
#include <memory>
#include <set>
#include <regex>
#include <sstream>
#include <string>

using namespace ch::core;

namespace {

// ── 设计 A：扁平（含**未用**的内部节点 ⇒ 卫生剪枝的靶）────────────────
class FlatDut : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      prog_we{"prog_we"};    // ⚠️ **没人用**
        ch_in<ch_uint<8>>   prog_data{"prog_data"};
        ch_out<ch_uint<8>>  result{"result"};
    )

    FlatDut(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        // ⚠️ 造一个**没人读**的组合节点 ⇒ ⇒ 卫生模式 MUST 剪掉它
        auto unused = io().prog_data + ch_uint<8>(1_d);
        (void)unused;
        io().result <<= io().prog_data;
    }
};

// ── 设计 B：两层成层（验「实例连接」在剪枝下不被误伤）───────────────
class HygLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("hyg_leaf");
    __io(ch_in<ch_uint<8>> din{"din"}; ch_out<ch_uint<8>> dout{"dout"};)
    ch_reg<ch_uint<8>> r{0_d, "r"};
    HygLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        r->next = io().din;
        io().dout <<= r;
    }
};

class HygTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("hyg_top");
    __io(ch_in<ch_uint<8>> din{"din"}; ch_out<ch_uint<8>> dout{"dout"};)
    ch::ch_module<HygLeaf> leaf{"leaf"};
    HygTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().din;
        io().dout <<= leaf.io().dout;
    }
};

// ── 设计 C：**mux 选字面量**（BLOCKER-1 的**最小复现**）───────────────
// ⚠️ 实测事故：剪枝曾按「使用者**类型**」一刀切（`op`/`mux`/`bitsupdate` 都内联）
//   ⇒ ⇒ `print_mux` **不内联**（按名字引用）⇒ ⇒ 字面量被剪掉、引用仍在
//   ⇒ ⇒ `default_nettype none` 下**直接编译失败**（RV32I 扁平 + 层次化双双 exit=1）。
//   ⇒ ⇒ 本用例 = 该缺陷的**常驻回归**（纯文本判据，不依赖 verilator）。
class MuxLitDut : public ch::Component {
public:
    __io(ch_in<ch_bool> sel{"sel"}; ch_out<ch_uint<8>> dout{"dout"};)
    MuxLitDut(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        io().dout <<= select(io().sel, ch_uint<8>(5_d), ch_uint<8>(7_d));
    }
};

// ── 设计 D：`a + b` ⇒ 产生**中间 proxy 链**（output walk 的靶）──────────
// ⚠️ 实测事故：`print_logic` 的 output 分支 walk proxy 链，只发
//   `assign <port> = <终点>;` ⇒ ⇒ 中间 proxy **一个字的文本都不发**
//   ⇒ ⇒ 但它们在**图**里仍有使用者（那个端口）⇒ ⇒ 「users 全死」规则剪不掉
//   ⇒ ⇒ 产物留下 `assign top__add = top_add;` 这种零引用线（实测命中）。
class AddDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a{"a"}; ch_in<ch_uint<8>> b{"b"};
         ch_out<ch_uint<8>> dout{"dout"};)
    AddDut(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().a + io().b; }
};

// ── 工具 ────────────────────────────────────────────────────────────
std::string emit_flat_text(context *ctx, bool hygiene) {
    std::ostringstream oss;
    ch::verilogwriter writer(ctx, hygiene);
    writer.print(oss);
    return oss.str();
}

/// 零引用信号数（声明了但**没人读**的 `logic`）—— **手推口径**见下。
std::size_t count_zero_reference(const std::string &v) {
    std::regex decl_re(R"(^\s*logic(?:\s*\[[^\]]*\])?\s+([A-Za-z_]\w*)\s*;)", std::regex::multiline);
    std::regex port_re(R"(^\s*(?:input|output|inout)\s*(?:\[[^\]]*\])?\s+([A-Za-z_]\w*))",
                       std::regex::multiline);
    std::regex assign_re(R"(^\s*assign\s+([A-Za-z_]\w*)\s*=\s*([^;]+);)", std::regex::multiline);

    std::set<std::string> decls, ports, read;
    for (auto it = std::sregex_iterator(v.begin(), v.end(), decl_re); it != std::sregex_iterator(); ++it) {
        decls.insert((*it)[1]);
    }
    for (auto it = std::sregex_iterator(v.begin(), v.end(), port_re); it != std::sregex_iterator(); ++it) {
        ports.insert((*it)[1]);
    }
    for (auto it = std::sregex_iterator(v.begin(), v.end(), assign_re); it != std::sregex_iterator(); ++it) {
        const std::string rhs = (*it)[2];
        std::regex ident(R"(\b([A-Za-z_]\w*)\b)");
        for (auto jt = std::sregex_iterator(rhs.begin(), rhs.end(), ident); jt != std::sregex_iterator(); ++jt) {
            read.insert((*jt)[1]);
        }
    }
    std::size_t n = 0;
    for (const auto &d : decls) {
        if (ports.count(d) == 0 && read.count(d) == 0) {
            ++n;
        }
    }
    return n;
}

/// 恒等直通数（`assign LHS = ...` 而 LHS **从不被读**且**不是端口**）
std::size_t count_identity(const std::string &v) {
    std::regex port_re(R"(^\s*(?:input|output|inout)\s*(?:\[[^\]]*\])?\s+([A-Za-z_]\w*))",
                       std::regex::multiline);
    std::regex assign_re(R"(^\s*assign\s+([A-Za-z_]\w*)\s*=\s*([^;]+);)", std::regex::multiline);
    std::set<std::string> ports, read, lhs;
    for (auto it = std::sregex_iterator(v.begin(), v.end(), port_re); it != std::sregex_iterator(); ++it) {
        ports.insert((*it)[1]);
    }
    std::regex ident(R"(\b([A-Za-z_]\w*)\b)");
    for (auto it = std::sregex_iterator(v.begin(), v.end(), assign_re); it != std::sregex_iterator(); ++it) {
        lhs.insert((*it)[1]);
        const std::string rhs = (*it)[2];
        for (auto jt = std::sregex_iterator(rhs.begin(), rhs.end(), ident); jt != std::sregex_iterator(); ++jt) {
            read.insert((*jt)[1]);
        }
    }
    std::size_t n = 0;
    for (const auto &l : lhs) {
        if (read.count(l) == 0 && ports.count(l) == 0) {
            ++n;
        }
    }
    return n;
}

/// 被引用但**未声明**的标识符数（BLOCKER-1 的**唯一直接判据**）。
///
/// ⚠️ MUST 先剥掉三类非标识符来源（否则假阳性）：注释 / 编译指令 / 数字字面量。
std::size_t count_undeclared_refs(const std::string &v) {
    std::string t = v;
    // ① 注释
    t = std::regex_replace(t, std::regex(R"(/\*.*?\*/)", std::regex::extended), " ");
    t = std::regex_replace(t, std::regex(R"(//[^\n]*)"), " ");
    // ② 反引号编译指令（`` `default_nettype none `` 的 `none` 不是信号）
    t = std::regex_replace(t, std::regex(R"((^|\n)[ \t]*`[^\n]*)"), "\n");
    // ③ 数字字面量（`8'h1` 会被拆出 `h1`）+ module 名
    t = std::regex_replace(t, std::regex(R"([0-9]*'[sS]?[bodhBODH][0-9a-fA-F_]+)"), " ");
    t = std::regex_replace(t, std::regex(R"(module[ \t]+[A-Za-z_]\w*)"), "module ");

    std::set<std::string> declared, ids;
    std::regex decl_re(R"(^\s*(?:logic|wire|reg)\s*(?:\[[^\]]*\])?\s+([A-Za-z_]\w*))",
                       std::regex::multiline);
    std::regex port_re(R"(^\s*(?:input|output|inout)\s*(?:\[[^\]]*\])?\s*([A-Za-z_]\w*))",
                       std::regex::multiline);
    for (auto it = std::sregex_iterator(t.begin(), t.end(), decl_re); it != std::sregex_iterator(); ++it) {
        declared.insert((*it)[1]);
    }
    for (auto it = std::sregex_iterator(t.begin(), t.end(), port_re); it != std::sregex_iterator(); ++it) {
        declared.insert((*it)[1]);
    }
    static const std::set<std::string> kKeywords = {
        "module", "endmodule", "input", "output", "inout", "logic", "wire", "reg",
        "assign", "always_ff", "always_comb", "begin", "end", "if", "else",
        "posedge", "negedge", "or", "and", "not", "case", "endcase", "default",
        "function", "endfunction", "localparam", "parameter", "integer", "initial",
        "final", "for", "while", "repeat", "forever", "signed", "unsigned", "return"};
    std::regex ident(R"(\b([A-Za-z_][A-Za-z_0-9]*)\b)");
    std::size_t n = 0;
    for (auto it = std::sregex_iterator(t.begin(), t.end(), ident); it != std::sregex_iterator(); ++it) {
        const std::string id = (*it)[1];
        if (kKeywords.count(id) != 0 || declared.count(id) != 0) {
            continue;
        }
        if (ids.insert(id).second) {
            ++n;
        }
    }
    return n;
}

}   // namespace

// ============================================================================
// 判据 1（**默认路径不变**）：`emit_hygiene` 默认 off ⇒ 产物含 blanket lint_off
// ============================================================================
TEST_CASE("hygiene: 默认 off ⇒ 产物含 blanket lint_off（默认路径不变）",
          "[rtl][emit][hygiene]") {
    ch::ch_device<FlatDut> dev;
    const std::string v = emit_flat_text(dev.context(), /*hygiene=*/false);

    REQUIRE(v.find("/* verilator lint_off WIDTHEXPAND */") != std::string::npos);
    REQUIRE(v.find("/* verilator lint_off WIDTHTRUNC */") != std::string::npos);
    // ⚠️ 默认路径**不**发 `default_nettype none`
    REQUIRE(v.find("`default_nettype none") == std::string::npos);
    // ⚠️ 默认路径**保留**零引用信号（未剪）
    REQUIRE(count_zero_reference(v) > 0);
}

// ============================================================================
// 判据 2（**卫生生效**）：开关 on ⇒ 四项卫生全部到位
// ============================================================================
TEST_CASE("hygiene: 开关 on ⇒ default_nettype none + 不发 blanket lint_off + 零引用归零",
          "[rtl][emit][hygiene]") {
    ch::ch_device<FlatDut> dev;
    const std::string v = emit_flat_text(dev.context(), /*hygiene=*/true);

    // ① 头部：`default_nettype none` + **确定性**元数据头（⚠️ **无时间戳**）
    REQUIRE(v.find("`default_nettype none") != std::string::npos);
    REQUIRE(v.find("schema=rtl-emit-manifest-v1") != std::string::npos);
    REQUIRE(v.find("hygiene=on") != std::string::npos);
    // ⚠️ **时间只进 manifest** ⇒ ⇒ 产物**不含**年份/时间戳形态（否则不确定性）
    REQUIRE(v.find("2026") == std::string::npos);

    // ② **不发** blanket lint_off
    REQUIRE(v.find("/* verilator lint_off WIDTHEXPAND */") == std::string::npos);
    REQUIRE(v.find("/* verilator lint_off WIDTHTRUNC */") == std::string::npos);

    // ③ **零引用信号 == 0** · ④ **恒等直通 == 0**
    REQUIRE(count_zero_reference(v) == 0);
    REQUIRE(count_identity(v) == 0);

    // ⚠️ 仍含 module 声明与端口（不是把产物剪空了）
    REQUIRE(v.find("module top") != std::string::npos);
    REQUIRE(v.find("top_result") != std::string::npos);
}

// ============================================================================
// 判据 3（**确定性**）：开关 on ⇒ 两次发射逐字节相同
// ============================================================================
TEST_CASE("hygiene: 开关 on ⇒ 两次发射逐字节相同（确定性）", "[rtl][emit][hygiene]") {
    std::string a, b;
    {
        ch::ch_device<FlatDut> d;
        a = emit_flat_text(d.context(), true);
    }
    {
        ch::ch_device<FlatDut> d;
        b = emit_flat_text(d.context(), true);
    }
    REQUIRE(a == b);
}

// ============================================================================
// 判据 4（**剪枝不误伤实例连接**）：两层成层 · 开关 on ⇒ 实例语句仍在
// ============================================================================
// ⚠️ 理由：跨 module 的实例连接（`.port(net)`）**不是**图里的边
//   ⇒ ⇒ 只看 `get_users()` 会把它判成「零引用」⇒ ⇒ 剪掉 ⇒ ⇒ **静默断线**
//   ⇒ ⇒ 保护规则 = 「出现在 `body_epilogue` 里的名字 MUST NOT 剪」。
TEST_CASE("hygiene: 开关 on ⇒ 跨 module 实例连接 MUST NOT 被剪断",
          "[rtl][emit][hygiene]") {
    ch::ch_device<HygTop> dev;
    ch::HierarchyEmitOptions opts;
    opts.emit_hygiene = true;
    const std::string path = "test_rtl_emit_hygiene_hier.v";
    const auto r = ch::toVerilogHierarchical(path, dev.instance(), opts);
    REQUIRE(r.ok);

    std::ifstream f(path);
    std::ostringstream oss;
    oss << f.rdbuf();
    const std::string v = oss.str();

    // 两层 ⇒ 两个 module（⚠️ 根 module 名由 `top_module_name` 决定，默认 `top`）
    REQUIRE(v.find("module top") != std::string::npos);
    REQUIRE(v.find("module hyg_leaf") != std::string::npos);
    // 🔴 **实例语句仍在**（保护规则的判据）
    REQUIRE(v.find("hyg_leaf leaf (") != std::string::npos);
    // 🔴 实例的端口连接仍在（否则就是「剪断了」）
    //   ⚠️ 端口名是**带前缀的**（`.top_leaf_din(...)`）—— 这是既有命名口径
    REQUIRE(v.find(".top_leaf_din(") != std::string::npos);
    REQUIRE(v.find(".top_leaf_dout(") != std::string::npos);
    // ⚠️ `default_nettype none` 下**任何未声明标识符**都会编译失败
    //   ⇒ ⇒ 本判据 + lint 门（CI 的 verify_hierarchical_rtl）共同兜底
    REQUIRE(v.find("`default_nettype none") != std::string::npos);
}

// ============================================================================
// 判据 5（**BLOCKER-1 回归**）：mux 选字面量 ⇒ 字面量 MUST 保留
// ============================================================================
// ⚠️ 判别力：把 `is_literal_referenced_by` 的 `print_mux` 判据改成「内联」
//   （即恢复 BLOCKER-1 的缺陷形态）⇒ ⇒ 本用例**必红**。
TEST_CASE("hygiene: mux 选字面量 ⇒ 字面量的声明 MUST 保留（BLOCKER-1 回归）",
          "[rtl][emit][hygiene]") {
    ch::ch_device<MuxLitDut> dev;
    const std::string v = emit_flat_text(dev.context(), /*hygiene=*/true);

    // 🔴 **无未声明引用** —— BLOCKER-1 的直接判据
    REQUIRE(count_undeclared_refs(v) == 0);
    // 🔴 字面量**被声明**且**被 mux 按名引用**
    REQUIRE(v.find("logic [7:0] top_uint_lit;") != std::string::npos);
    REQUIRE(v.find("top_uint_lit_1") != std::string::npos);
    REQUIRE(v.find("? top_uint_lit : top_uint_lit_1") != std::string::npos);
    // 两个 lit 都有各自的赋值 ⇒ 值是 5 / 7（手推）
    REQUIRE(v.find("assign top_uint_lit = 8'h5;") != std::string::npos);
    REQUIRE(v.find("assign top_uint_lit_1 = 8'h7;") != std::string::npos);
}

// ============================================================================
// 判据 6：中间 proxy 链（`print_logic` 的 output walk）⇒ 零引用必须为 0
// ============================================================================
// ⚠️ 判别力：去掉 `compute_dead_nodes()` 里的「链上被跳过的中间 proxy」种子
//   ⇒ ⇒ 产物留下 `assign top__add = top_add;` ⇒ ⇒ 本用例**必红**。
TEST_CASE("hygiene: 端口 assign 的中间 proxy MUST 被剪（零引用归零）",
          "[rtl][emit][hygiene]") {
    ch::ch_device<AddDut> dev;
    const std::string v = emit_flat_text(dev.context(), /*hygiene=*/true);

    REQUIRE(count_undeclared_refs(v) == 0);
    // 中间 proxy（`top__add_add`：以 `_` 分隔的转发链）MUST NOT 出现在产物里
    REQUIRE(v.find("top__add_add") == std::string::npos);
    // **终点** driver 仍被 output 引用（且带显式宽度转换：9 位 add → 8 位 out）
    REQUIRE(v.find("logic [8:0] top_add_add;") != std::string::npos);
    REQUIRE(v.find("assign top_dout = 8'(top_add_add);") != std::string::npos);
    // 零引用 / 恒等直通均为 0
    REQUIRE(count_zero_reference(v) == 0);
    REQUIRE(count_identity(v) == 0);
}

// ============================================================================
// 判据 7：`default_nettype wire` 恢复（宏状态**跨文件延续**）
// ============================================================================
TEST_CASE("hygiene: 产物尾部 MUST 恢复 default_nettype wire（每文件一次）",
          "[rtl][emit][hygiene]") {
    ch::ch_device<FlatDut> dev;
    const std::string v = emit_flat_text(dev.context(), /*hygiene=*/true);

    // 头部 `none` 一次 + 尾部 `wire` 一次 ⇒ 合计 2 处 `default_nettype`
    std::size_t n = 0;
    for (std::size_t i = v.find("`default_nettype"); i != std::string::npos;
         i = v.find("`default_nettype", i + 1)) {
        ++n;
    }
    REQUIRE(n == 2);
    REQUIRE(v.find("`default_nettype none") != std::string::npos);
    REQUIRE(v.find("`default_nettype wire") != std::string::npos);
    // 尾部在 `endmodule` **之后**
    REQUIRE(v.find("endmodule") < v.find("`default_nettype wire"));
}
