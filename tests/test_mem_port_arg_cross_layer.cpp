// tests/test_mem_port_arg_cross_layer.cpp
//
// `ch_mem` 端口实参跨层（ADR-066 决策 ⑦ / ⑧）· **AC1 / AC3 / AC4 / AC8** 的判据面。
//
// ⚠️ **本文件是「(ii) 形态」的专用判据面**（父持 `ch_mem` + 实参视线跨层）⇒ ⇒ 与
// `tests/test_hierarchy_analysis.cpp` 的**既有** (i) 桶用例**分工明确**：
//   · (i) 桶（mem 本体跨 module）⇒ ⇒ 既有文件（**强化**其断言，见该文件）
//   · (ii) 桶（仅实参视线跨层）⇒ ⇒ **本文件**
//   · (iii) 桶（非相邻实参）⇒ ⇒ **本文件**
//
// ⚠️ **值判据 MUST 手推**（AGENTS.md ANTI-PATTERNS：禁只抄仿真输出）⇒ ⇒ 本文件用
// **可枚举的初值表**（`init_data`）⇒ ⇒ 期望值 = **查表**（不是抄输出）⇒ ⇒
// 「非 0」只是下界 ⇒ ⇒ 本文件断言的是**确定值**。
//
// ⚠️ **判别力的两面**（每条判据 MUST 有正反两面）：
//   · 负向用例（(i)/(iii) MUST 仍红）⇒ ⇒ 抓「误放行」
//   · 正向用例（(ii) MUST 绿 + 值精确）⇒ ⇒ 抓「误拒 / 值错」
// ⇒ ⇒ **只测一面** ⇒ 恒绿 ⇒ ⇒ 零判别力。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "codegen_naming.h"
#include "hierarchy_verilog.h"
#include "module.h"
#include "simulator.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ═════════════════════════════════════════════════════════════════════════
// DUT 组 ①：**(ii) 形态** —— mem 本体在**父**（`top`）· 实参视线**跨层**（取自子的 `ch_out`）
//
// ⚠️ **形态**（ADR-066 决策 ⑦ 的 (ii)）：
//   · mem 本体与使用者（mem 端口节点）**同 module** = `top` ⇒ ⇒ ① == ②
//   · `addr` / `wdata` / `en` 三个实参槽的 src **跨 module**（来自子 `top.addr_src`）
//   · 该 src 的 owner（折叠后）与使用 module **相邻**（子 → 父）⇒ ⇒ 非 (iii)
// ═════════════════════════════════════════════════════════════════════════

/// **子组件**：把父层送来的 `ch_in` **转发**成自己的 `ch_out`（⇒ ⇒ 父层的 mem 端口
/// 实参从这些 `ch_out` 取 ⇒ ⇒ **实参视线跨层**）。
///
/// ⚠️ **形态要点**：子的 `ch_out` MUST 由**子内部**驱动（`ch_in` → `ch_out`）⇒ ⇒
// **不是**「父层驱动子的 `ch_out`」（那会把边方向反过来 ⇒ ⇒ 发射器判
/// 「child side is not an input port」⇒ ⇒ **合法拒绝**）。
class ArgSrc : public ch::Component {
public:
    CH_HIERARCHY_LAYER("arg_src");
    __io(ch_in<ch_uint<4>> addr_in; ch_in<ch_uint<8>> wdata_in; ch_in<ch_bool> en_in;
         ch_out<ch_uint<4>> addr; ch_out<ch_uint<8>> wdata; ch_out<ch_bool> en;)
    ArgSrc(ch::Component *p = nullptr, const std::string &n = "arg_src")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        io().addr <<= io().addr_in;
        io().wdata <<= io().wdata_in;
        io().en <<= io().en_in;
    }
};

/// **顶层**：持有 `ch_mem` · 实参槽取自子 `ch_out`（= (ii) 形态）。
///
/// ⚠️ **mem 初值表** = `{0xA0, 0xA1, ..., 0xAF}`（**手推**的查表基准 ⇒ ⇒ 值判据
/// **不依赖**仿真输出 ⇒ ⇒ 禁抄）。
class MemArgTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_arg_top");
    __io(ch_in<ch_uint<4>> addr_in; ch_in<ch_uint<8>> wdata_in; ch_in<ch_bool> en_in;
         ch_out<ch_uint<8>> rdata;)
    /// ⚠️ **初值表**（手推基准）：地址 k ⇒ 值 `0xA0 + k`
    ch_mem<ch_uint<8>, 16> mem{std::vector<std::uint64_t>{
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF}, "mem"};
    ch::ch_module<ArgSrc> src{"src"};
    MemArgTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        // 父层驱动子的三个 **ch_in** ⇒ ⇒ 形成**父 → 子**的跨层边
        src.io().addr_in <<= io().addr_in;
        src.io().wdata_in <<= io().wdata_in;
        src.io().en_in <<= io().en_in;
        // ★ **(ii) 形态的核心**：mem 端口的**实参槽**取自**子**的 `ch_out`
        //   ⇒ mem 本体在 `top` · 使用者（端口节点）也在 `top` ⇒ ⇒ ① == ②
        //   ⇒ ⇒ 实参视线跨层 ⇒ ⇒ (ii)
        io().rdata <<= mem.aread(src.io().addr);
    }
};

/// **顶层**（写侧）：mem 的 `addr` / `wdata` / `en` **全部**取自子 `ch_out`
/// ⇒ ⇒ 覆盖 V21 的**写侧**（`wdata` / `en` 两槽）。
class MemArgWriteTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_arg_write_top");
    __io(ch_in<ch_uint<4>> addr_in; ch_in<ch_uint<8>> wdata_in; ch_in<ch_bool> en_in;
         ch_out<ch_uint<8>> rdata;)
    ch_mem<ch_uint<8>, 16> mem{std::vector<std::uint64_t>{
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
        0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF}, "mem"};
    ch::ch_module<ArgSrc> src{"src"};
    MemArgWriteTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        src.io().addr_in <<= io().addr_in;
        src.io().wdata_in <<= io().wdata_in;
        src.io().en_in <<= io().en_in;
        // ★ 写侧：`write(addr, wdata, en)` ⇒ ⇒ 三个实参槽**全部**跨层
        mem.write(src.io().addr, src.io().wdata, src.io().en);
        io().rdata <<= mem.aread(src.io().addr);
    }
};

/// ═════════════════════════════════════════════════════════════════════════
// DUT 组 ②：**(iii) 形态** —— 实参来自**孙**组件（非相邻）⇒ ⇒ MUST 仍被拒
// ═════════════════════════════════════════════════════════════════════════

/// **孙**组件：产出 `addr`（⇒ ⇒ 父层的 mem 端口实参来自**孙** ⇒ ⇒ 非相邻）。
class GrandArgSrc : public ch::Component {
public:
    CH_HIERARCHY_LAYER("grand_arg_src");
    __io(ch_out<ch_uint<4>> addr;)
    GrandArgSrc(ch::Component *p = nullptr, const std::string &n = "grand_arg_src")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().addr <<= io().addr; }
};

/// **中间**组件：持有孙组件（⇒ ⇒ 孙的 `ch_out` 在中间层 ⇒ ⇒ 顶层拿它 = **非相邻**）。
class MidArgSrc : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mid_arg_src");
    __io(ch_in<ch_uint<4>> addr_in; ch_out<ch_uint<4>> addr;)
    ch::ch_module<GrandArgSrc> grand{"grand"};
    MidArgSrc(ch::Component *p = nullptr, const std::string &n = "mid_arg_src")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        grand.io().addr <<= io().addr_in;
        io().addr <<= grand.io().addr;
    }
};

/// **顶层**：mem 本体在顶层 · 实参取自**孙**组件的 `ch_out`（经中间层转发 ⇒ ⇒ 非相邻）。
class MemArgGrandTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_arg_grand_top");
    __io(ch_in<ch_uint<4>> addr_in; ch_out<ch_uint<8>> rdata;)
    ch_mem<ch_uint<8>, 16> mem{std::vector<std::uint64_t>{
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF}, "mem"};
    ch::ch_module<MidArgSrc> mid{"mid"};
    MemArgGrandTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mid.io().addr_in <<= io().addr_in;
        // ⚠️ 实参来自**孙**（`grand`）⇒ ⇒ 与使用 module（顶层）**非相邻** ⇒ ⇒ (iii)
        io().rdata <<= mem.aread(mid.instance().grand.io().addr);
    }
};



// ═════════════════════════════════════════════════════════════════════════
// DUT 组 ③：**边界清单**（AC8）—— 多 `ch_mem` · `sread` 类 · 决策 ⑤ 共网
// ═════════════════════════════════════════════════════════════════════════

/// **多 `ch_mem`**：顶层持**两个** mem（不同初值表）⇒ ⇒ 两个端口的实参**都**跨层
/// ⇒ ⇒ 判别力 = 「其中一个漏登记」⇒ ⇒ 另一个的值仍对 ⇒ ⇒ 必须**两个都断言**。
class MemTwoTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_two_top");
    __io(ch_in<ch_uint<4>> addr_in; ch_out<ch_uint<8>> rdata_a; ch_out<ch_uint<8>> rdata_b;)
    /// ⚠️ 初值表 A（手推）：地址 k ⇒ `0xE0 + k`
    ch_mem<ch_uint<8>, 16> mem_a{std::vector<std::uint64_t>{
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
        0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF}, "mem_a"};
    /// ⚠️ 初值表 B（手推）：地址 k ⇒ `0xF0 + k`（⇒ ⇒ 与 A **可区分** ⇒ ⇒
    ///   「两个 mem 读串了」也能被抓）
    ch_mem<ch_uint<8>, 16> mem_b{std::vector<std::uint64_t>{
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
        0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF}, "mem_b"};
    ch::ch_module<ArgSrc> src{"src"};
    MemTwoTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        src.io().addr_in <<= io().addr_in;
        src.io().wdata_in <<= ch_uint<8>(0_d);
        src.io().en_in <<= ch_bool(false);
        io().rdata_a <<= mem_a.aread(src.io().addr);
        io().rdata_b <<= mem_b.aread(src.io().addr);
    }
};

/// **`sread` 类**（1 拍读）· mem 本体在顶层 · 实参跨层。
///
/// ⚠️ **为什么单独覆盖**：`sread` 走 `always_ff`（1 拍）⇒ ⇒ 与 `aread`（0 拍组合）
/// 的**时序**完全不同 ⇒ ⇒ 若某条实现路径只处理了 `aread` ⇒ ⇒ `sread` 的实参槽会漏。
class MemSyncTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_sync_top");
    __io(ch_in<ch_uint<4>> addr_in; ch_out<ch_uint<8>> rdata;)
    /// ⚠️ 初值表（手推）：地址 k ⇒ `0xC0 + k`
    ch_mem<ch_uint<8>, 16> mem{std::vector<std::uint64_t>{
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF}, "mem"};
    ch::ch_module<ArgSrc> src{"src"};
    MemSyncTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        src.io().addr_in <<= io().addr_in;
        src.io().wdata_in <<= ch_uint<8>(0_d);
        src.io().en_in <<= ch_bool(false);
        // ★ `sread`（1 拍读）⇒ ⇒ 与 `aread`（0 拍）的时序面**不同**
        io().rdata <<= mem.sread(src.io().addr);
    }
};

/// **决策 ⑤ 共网**：同一子 `ch_out` **既**直通父 `output` 端口 **又**作 `(ii)` 实参
/// ⇒ ⇒ MUST 扇出**同一根**父侧网（⇒ ⇒ 否则「两根网」⇒ ⇒ `Duplicate pin connection`）。
class MemSharedOutTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_shared_out_top");
    __io(ch_in<ch_uint<4>> addr_in; ch_out<ch_uint<8>> rdata; ch_out<ch_uint<4>> addr_out;)
    ch_mem<ch_uint<8>, 16> mem{std::vector<std::uint64_t>{
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
        0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF}, "mem"};
    ch::ch_module<ArgSrc> src{"src"};
    MemSharedOutTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        src.io().addr_in <<= io().addr_in;
        src.io().wdata_in <<= ch_uint<8>(0_d);
        src.io().en_in <<= ch_bool(false);
        // ★ 同一子 `ch_out` **既**直通父 output 端口 **又**作 mem 实参
        io().addr_out <<= src.io().addr;
        io().rdata <<= mem.aread(src.io().addr);
    }
};

// ═════════════════════════════════════════════════════════════════════════
// 工具
// ═════════════════════════════════════════════════════════════════════════

std::string read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

/// 提取 `module <name> (...)` 的**body**（= 端口表之后到 `endmodule` 之前）。
///
/// ⚠️ **作用域锚定**（ADR-066 §5 V2）：`all.find(...)` **不算**作用域锚定 ⇒ ⇒
/// 本函数按**端口表的结束位置**切分 ⇒ ⇒ body 只含该 module 的内部声明 / 语句。
std::string module_body(const std::string &text, const std::string &module_name) {
    const std::string header = "module " + module_name;
    std::size_t pos = text.find(header);
    if (pos == std::string::npos) {
        return {};
    }
    const std::size_t paren = text.find('(', pos);
    if (paren == std::string::npos) {
        return {};
    }
    // 端口表结束 = 与 header 处同深度的 ')'
    int depth = 0;
    std::size_t end = std::string::npos;
    for (std::size_t i = paren; i < text.size(); ++i) {
        if (text[i] == '(') {
            ++depth;
        } else if (text[i] == ')') {
            --depth;
            if (depth == 0) {
                end = i;
                break;
            }
        }
    }
    if (end == std::string::npos) {
        return {};
    }
    const std::size_t endmod = text.find("endmodule", end);
    if (endmod == std::string::npos) {
        return {};
    }
    return text.substr(end, endmod - end);
}

/// 在 body 内查找一条 `logic` 声明（**宽度敏感**）：`logic [W-1:0] <name>;`
/// 或 `logic <name>;`。
bool declares_logic(const std::string &body, const std::string &name) {
    const std::string needle_a = "logic [3:0] " + name + ";";
    const std::string needle_b = "logic " + name + ";";
    return body.find(needle_a) != std::string::npos ||
           body.find(needle_b) != std::string::npos;
}

/// 在 body 内查找一条 `assign`（⇒ 用于确认 mem 端口**读了**哪个网）。
bool has_assign_from(const std::string &body, const std::string &lhs_fragment,
                     const std::string &rhs_fragment) {
    std::size_t pos = 0;
    while ((pos = body.find("assign ", pos)) != std::string::npos) {
        const std::size_t eol = body.find('\n', pos);
        const std::string line = body.substr(pos, (eol == std::string::npos ? body.size() : eol) - pos);
        if (line.find(lhs_fragment) != std::string::npos &&
            line.find(rhs_fragment) != std::string::npos) {
            return true;
        }
        if (eol == std::string::npos) {
            break;
        }
        pos = eol;
    }
    return false;
}

}   // namespace

// ═════════════════════════════════════════════════════════════════════════
// AC4：**(ii) 形态**的产物形态 —— 父侧 MUST 有**显式 `logic` 声明**（⇒ 非 implicit wire）
//
// ⚠️ **判据的可判性**（ADR-066 决策 ⑧ ②）：父侧网名 = **驱动端点的 ctx 全局名**
// ⇒ ⇒ 而 `node_names_` = **同一份**全局名表 ⇒ ⇒ mem 端口读到的名字 **就是**网名
// ⇒ ⇒ ⇒ **判据** = 「mem 读端口的 `assign` 里出现的那个网名，在**父 module body** 内
// MUST 有 `logic` 声明」⇒ ⇒ 缺声明 ⇒ ⇒ Verilog 当 **implicit wire（1-bit）**
// ⇒ ⇒ **宽度截断 ⇒ 值错** ⇒ ⇒ 且 lint 门**不加 `-Wall`** ⇒ ⇒ **可能静默**。
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("mem port arg cross layer - the (ii) shape emits a declared parent-side net",
          "[hierarchy][mem][cross-layer]") {
    ch::ch_device<MemArgTop> dev;
    const std::string path = "/tmp/cpphdl_mem_arg_ii.v";
    std::remove(path.c_str());

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);

    const std::string product = read_file(path);
    REQUIRE_FALSE(product.empty());

    const std::string body = module_body(product, "top");
    REQUIRE_FALSE(body.empty());

    // 父侧落点网的**名字** = 驱动端点（子 `ch_out`）的 ctx 全局名 ⇒ ⇒ 从**子 module 的
    // 端口表**里取那个 output 端口名 ⇒ ⇒ 父 body MUST 有同名 `logic` 声明。
    const std::string child_body = module_body(product, "arg_src");
    REQUIRE_FALSE(child_body.empty());
    // 子 module 端口表里**第一个 4-bit output**（= 本 DUT 的 `addr` 端口）
    // ⚠️ **作用域锚定**：`module_body` 已把子 module 的端口表切掉 ⇒ ⇒ 这里的
    // `output [3:0]` 出现在**别的** module 里也不会被误取。
    std::string wire_name;
    {
        const std::string header = "module arg_src (";
        const std::size_t hpos = product.find(header);
        REQUIRE(hpos != std::string::npos);
        const std::size_t pos = product.find("output [3:0] ", hpos);
        REQUIRE(pos != std::string::npos);
        const std::size_t nl = product.find('\n', pos);
        const std::string line = product.substr(pos, nl - pos);
        const std::size_t last = line.find_last_of(' ');
        REQUIRE(last != std::string::npos);
        wire_name = line.substr(last + 1);
        // 去掉行尾的 ','（端口表分隔）
        if (!wire_name.empty() && wire_name.back() == ',') {
            wire_name.pop_back();
        }
    }
    REQUIRE_FALSE(wire_name.empty());

    // ★ **AC4 的判据**：父 body MUST 有该名字的**显式 `logic` 声明**
    //   ⇒ ⇒ 否则 = Verilog **implicit wire（1-bit）** ⇒ ⇒ 宽度截断 ⇒ 值错
    INFO("wire name = " << wire_name);
    INFO("parent body:\n" << body);
    REQUIRE(declares_logic(body, wire_name));

    // ★ **反向判据**（防止「判据恒真」）：把 wire_name 换成**未声明**的名字
    //   ⇒ ⇒ 必然找不到 `logic` 声明 ⇒ ⇒ 证明本判据**有判别力**
    REQUIRE_FALSE(declares_logic(body, wire_name + "_not_declared"));

    // ★ mem 端口 MUST **读**该网（⇒ ⇒ 实参槽的重定向**真的**发生了）
    //   ⚠️ 若 mem 端口读的是**别的**名字 ⇒ ⇒ 该名字若无声明 ⇒ implicit wire（1-bit）
    //   ⇒ ⇒ 宽度截断 ⇒ 值错 ⇒ ⇒ 且 lint 门**不加 `-Wall`** ⇒ ⇒ 可能静默
    REQUIRE(has_assign_from(body, "top_mem_aread_data_proxy", wire_name));
}

// ═════════════════════════════════════════════════════════════════════════
// AC3：**(ii) 形态的值判据** —— mem 端口读到的**值** MUST 是**手推**的确定值
//
// ⚠️ **期望值手推**：mem 初值表 = `{0xA0..0xAF}` ⇒ ⇒ 读地址 k ⇒ 值 `0xA0 + k`
// ⇒ ⇒ **精确期望值**（非 0 只是下界）⇒ ⇒ 且**不抄**仿真输出。
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("mem port arg cross layer - the (ii) shape reads the hand-derived value",
          "[hierarchy][mem][cross-layer][value]") {
    ch::ch_device<MemArgTop> dev;
    ch::Simulator sim(dev.context());

    // ⚠️ 期望值 = **查表**（手推）：地址 k ⇒ `0xA0 + k`
    const std::uint64_t k_addr = 5;
    const std::uint64_t k_expected = 0xA0 + k_addr;

    sim.set_input_value(dev.instance().io().addr_in, k_addr);
    sim.set_input_value(dev.instance().io().wdata_in, 0);
    sim.set_input_value(dev.instance().io().en_in, 0);
    sim.tick();

    const std::uint64_t got =
        static_cast<std::uint64_t>(sim.get_port_value(dev.instance().io().rdata));
    INFO("addr = " << k_addr << " · expected = 0x" << std::hex << k_expected
         << " · got = 0x" << got);
    REQUIRE(got == k_expected);
}

// ═════════════════════════════════════════════════════════════════════════
// AC3 的**写侧**（V21：写 `wdata` / `en` 两侧 MUST 断言精确期望值）
//
// ⚠️ **写侧**：`mem.write(addr, wdata, en)` 的三个实参**全部**跨层 ⇒ ⇒ 覆盖
// `wdata` 与 `en` 两槽 ⇒ ⇒ 若任一槽漏登记 ⇒ ⇒ 写侧错 ⇒ ⇒ 但**读侧对** ⇒ ⇒
// 「非 0 只是下界」的反例。
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("mem port arg cross layer - the write side takes wdata and en across the layer",
          "[hierarchy][mem][cross-layer][value]") {
    ch::ch_device<MemArgWriteTop> dev;
    ch::Simulator sim(dev.context());

    // ⚠️ **手推**：地址 3 · 写 `0x5A` · en=1 ⇒ ⇒ 读回 `0x5A`
    //   （写端口 = 1 拍 ⇒ ⇒ 读侧是 0-cycle 组合读 ⇒ ⇒ 读到**上一拍**写进去的值）
    const std::uint64_t k_addr = 3;
    const std::uint64_t k_wdata = 0x5A;

    sim.set_input_value(dev.instance().io().addr_in, k_addr);
    sim.set_input_value(dev.instance().io().wdata_in, k_wdata);
    sim.set_input_value(dev.instance().io().en_in, 1);
    sim.tick();                                   // 写端口在**这一拍**提交
    sim.tick();                                   // ⇒ 之后组合读看到写进去的值

    const std::uint64_t got =
        static_cast<std::uint64_t>(sim.get_port_value(dev.instance().io().rdata));
    INFO("addr = " << k_addr << " · wdata = 0x" << std::hex << k_wdata
         << " · got = 0x" << got);
    REQUIRE(got == k_wdata);

    // ★ **反向判据**（防恒真）：换一个地址 ⇒ ⇒ 读到的**不是** `0x5A`
    //   ⇒ ⇒ 证明「读到的是**写进去的那个值**」而非「恒为某个常量」
    sim.set_input_value(dev.instance().io().addr_in, 7);
    sim.set_input_value(dev.instance().io().wdata_in, 0x33);
    sim.tick();
    sim.tick();
    const std::uint64_t got2 =
        static_cast<std::uint64_t>(sim.get_port_value(dev.instance().io().rdata));
    INFO("second got = 0x" << std::hex << got2);
    REQUIRE(got2 == 0x33);
    REQUIRE(got2 != got);
}

// ═════════════════════════════════════════════════════════════════════════
// AC1（负向 ②）：**(iii) 形态**（非相邻实参）⇒ ⇒ MUST **仍被拒**
//
// ⚠️ **判别力**：若 (iii) 被误放行 ⇒ ⇒ 真跨层访问被静默接受 ⇒ ⇒ 隐藏共享状态
// ⇒ ⇒ 本用例 MUST 转红。
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("mem port arg cross layer - a non-adjacent operand (grandchild) is still rejected",
          "[hierarchy][mem][cross-layer][reject]") {
    ch::ch_device<MemArgGrandTop> dev;
    const std::string path = "/tmp/cpphdl_mem_arg_grand.v";
    std::remove(path.c_str());

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE_FALSE(r.ok);
    // 诊断 MUST 点名 mem 跨层访问（⇒ ⇒ 可行动）
    REQUIRE(r.error.find("cross-module ch_mem") != std::string::npos);
    // ★ **归因订正**（AC6）：(iii) 支下 mem 本体与使用者**已在同层**
    //   ⇒ ⇒ 诊断 **MUST NOT** 说「mem 与使用者 MUST 同 module」（那是 (i) 支的话）
    REQUIRE(r.error.find("memory and its user to be in the same module") ==
            std::string::npos);
}

// ═════════════════════════════════════════════════════════════════════════
// AC8-①：多 `ch_mem` —— 两个 mem 的实参**都**跨层 ⇒ ⇒ 两个值 MUST **各自**等于手推值
//
// ⚠️ **判别力**：若只有一个 mem 的实参被正确登记 ⇒ ⇒ 另一个读到**别的** mem 的值
// ⇒ ⇒ 「两个都断言」才能抓到（只断言一个 ⇒ ⇒ 漏检）。
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("mem port arg cross layer - two ch_mem instances both take cross-layer operands",
          "[hierarchy][mem][cross-layer][multi]") {
    ch::ch_device<MemTwoTop> dev;
    ch::Simulator sim(dev.context());

    const std::uint64_t k_addr = 4;
    sim.set_input_value(dev.instance().io().addr_in, k_addr);
    sim.tick();

    const std::uint64_t got_a =
        static_cast<std::uint64_t>(sim.get_port_value(dev.instance().io().rdata_a));
    const std::uint64_t got_b =
        static_cast<std::uint64_t>(sim.get_port_value(dev.instance().io().rdata_b));
    INFO("addr = " << k_addr << " · A expected = 0x" << std::hex << (0xE0 + k_addr)
         << " · got = 0x" << got_a << " · B expected = 0x" << (0xF0 + k_addr)
         << " · got = 0x" << got_b);
    // ⚠️ 手推：初值表 A ⇒ `0xE0 + k` · 初值表 B ⇒ `0xF0 + k`
    REQUIRE(got_a == (0xE0 + k_addr));
    REQUIRE(got_b == (0xF0 + k_addr));
    // ★ **反向判据**：两个 mem 的值 MUST **不同**（⇒ ⇒ 「读串了」也能被抓）
    REQUIRE(got_a != got_b);
}

// ═════════════════════════════════════════════════════════════════════════
// AC8-②：`sread` 类（1 拍读）· 实参跨层 ⇒ ⇒ 产物形态 MUST 与 `aread` **同样**成立
//
// ⚠️ **为什么必须单独覆盖**：`sread` 走 `always_ff`（1 拍）⇒ ⇒ 与 `aread`（0 拍组合）
// 的**时序**面完全不同 ⇒ ⇒ 若某条实现路径只处理了 `aread` ⇒ ⇒ `sread` 的实参槽会漏。
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("mem port arg cross layer - a sync-read (sread) port also takes a cross-layer operand",
          "[hierarchy][mem][cross-layer][sread]") {
    ch::ch_device<MemSyncTop> dev;
    const std::string path = "/tmp/cpphdl_mem_arg_sread.v";
    std::remove(path.c_str());

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);

    const std::string product = read_file(path);
    REQUIRE_FALSE(product.empty());
    const std::string body = module_body(product, "top");
    REQUIRE_FALSE(body.empty());

    // ★ 父侧 MUST 有落点网的**显式 `logic` 声明**（⇒ 非 implicit wire ⇒ 非宽度截断）
    //   ⇒ 名字 = 驱动端点（子 `ch_out`）的 ctx 全局名
    const std::string child_body = module_body(product, "arg_src");
    REQUIRE_FALSE(child_body.empty());
    std::string wire_name;
    {
        const std::size_t hpos = product.find("module arg_src (");
        REQUIRE(hpos != std::string::npos);
        const std::size_t pos = product.find("output [3:0] ", hpos);
        REQUIRE(pos != std::string::npos);
        const std::size_t nl = product.find('\n', pos);
        const std::string line = product.substr(pos, nl - pos);
        const std::size_t last = line.find_last_of(' ');
        REQUIRE(last != std::string::npos);
        wire_name = line.substr(last + 1);
        if (!wire_name.empty() && wire_name.back() == ',') {
            wire_name.pop_back();
        }
    }
    REQUIRE_FALSE(wire_name.empty());
    INFO("wire name = " << wire_name);
    REQUIRE(declares_logic(body, wire_name));
    // ★ **反向判据**
    REQUIRE_FALSE(declares_logic(body, wire_name + "_not_declared"));
}

// ═════════════════════════════════════════════════════════════════════════
// AC8-③：决策 ⑤ 共网 —— 同一子 `ch_out` **既**直通父 output **又**作 mem 实参
// ⇒ ⇒ MUST 扇出**同一根**父侧网（⇒ ⇒ 两根网 ⇒ ⇒ `Duplicate pin connection`）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("mem port arg cross layer - a shared child output fans out to ONE parent-side net",
          "[hierarchy][mem][cross-layer][single-net]") {
    ch::ch_device<MemSharedOutTop> dev;
    const std::string path = "/tmp/cpphdl_mem_arg_shared.v";
    std::remove(path.c_str());

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);

    const std::string product = read_file(path);
    const std::string body = module_body(product, "top");
    REQUIRE_FALSE(body.empty());

    // 落点网的名字 = 驱动端点（子 `ch_out`）的 ctx 全局名
    std::string wire_name;
    {
        const std::size_t hpos = product.find("module arg_src (");
        REQUIRE(hpos != std::string::npos);
        const std::size_t pos = product.find("output [3:0] ", hpos);
        REQUIRE(pos != std::string::npos);
        const std::size_t nl = product.find('\n', pos);
        const std::string line = product.substr(pos, nl - pos);
        const std::size_t last = line.find_last_of(' ');
        REQUIRE(last != std::string::npos);
        wire_name = line.substr(last + 1);
        if (!wire_name.empty() && wire_name.back() == ',') {
            wire_name.pop_back();
        }
    }
    REQUIRE_FALSE(wire_name.empty());
    INFO("wire name = " << wire_name);

    // ★ **恰好一根** `logic` 声明（⇒ ⇒ 决策 ⑤ 的单连接不变式）
    std::size_t decl_count = 0;
    for (std::size_t pos = body.find("logic"); pos != std::string::npos;
         pos = body.find("logic", pos + 1)) {
        if (declares_logic(body.substr(pos, body.find(';', pos) - pos + 1), wire_name)) {
            ++decl_count;
        }
    }
    INFO("declaration count = " << decl_count);
    REQUIRE(decl_count == 1);

    // ★ **反向判据**（防恒真）：产物里 MUST **没有** `Duplicate pin connection` 迹象
    //   ⇒ ⇒ 且实例化语句里该 pin 只出现**一次**
    REQUIRE(product.find("Duplicate pin connection") == std::string::npos);
    std::size_t pin_occurrences = 0;
    for (std::size_t pos = product.find("." + wire_name + "("); pos != std::string::npos;
         pos = product.find("." + wire_name + "(", pos + 1)) {
        ++pin_occurrences;
    }
    INFO("pin connection occurrences = " << pin_occurrences);
    REQUIRE(pin_occurrences == 1);
}
