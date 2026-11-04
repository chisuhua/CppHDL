// tests/test_hierarchy_verilog.cpp
//
// 层次化 RTL 输出 · T3-④（层次发射）的判据。
//
// 判据面（本文件覆盖的部分，逐条对应 change 的 Acceptance）：
//   · 三层设计 ⇒ **3 个 module + 2 条实例化语句**；
//   · RTL 顶层 module 的端口表 MUST 与扁平产物**逐字逐序相同**；
//   · 非根 module 的端口 ⊇ 自身 `ch_in` ∪ `ch_out`（且**已由父层驱动也算**）；
//   · 零跨 module `assign`（父层只用实例端口映射）；
//   · clock / reset **分开判**（纯 RAM 写 module 只要 clock）；
//   · 产物**确定性**（两次生成逐字节相同）；
//   · 错误模型：校验失败 ⇒ `ok == false` + 无新文件。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "codegen_verilog.h"
#include "device.h"
#include "hierarchy_verilog.h"
#include "module.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

const char *kOutDir = "/tmp/opencode/hieremit";

std::string read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool file_exists(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    return in.good();
}

/// 该文件里某个 module 的文本（`module <name> (...)` 到下一个 `module ` 之前）
std::string module_text(const std::string &all, const std::string &name) {
    const std::string head = "module " + name + " (";
    const auto at = all.find(head);
    if (at == std::string::npos) {
        return {};
    }
    const auto next = all.find("\nmodule ", at + head.size());
    return all.substr(at, next == std::string::npos ? std::string::npos : next - at);
}

std::size_t count_occurrences(const std::string &haystack, const std::string &needle) {
    std::size_t n = 0;
    for (auto at = haystack.find(needle); at != std::string::npos;
         at = haystack.find(needle, at + needle.size())) {
        ++n;
    }
    return n;
}

// ── DUT ①：三层（全部成层）───────────────────────────────────────────────
class L3 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("l3_unit");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> r{0_d};   // ⇒ 需要 clock + reset
    L3(ch::Component *p = nullptr, const std::string &n = "leaf") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        r->next = io().din;
        io().dout <<= r;
    }
};

class M3 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("m3_unit");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<L3> leaf{"leaf"};
    M3(ch::Component *p = nullptr, const std::string &n = "mid") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().din;
        io().dout <<= leaf.io().dout;
    }
};

class T3 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("t3_unit");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<M3> mid{"mid"};
    T3(ch::Component *p = nullptr, const std::string &n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mid.io().din <<= io().din;
        io().dout <<= mid.io().dout;
    }
};

// ── DUT ②：只有 mem 写端口的成层子模块（⇒ 要 clock，**不要** reset）────────
class RamWriter : public ch::Component {
public:
    CH_HIERARCHY_LAYER("ram_writer");
    __io(ch_in<ch_uint<4>> addr; ch_in<ch_uint<8>> wdata; ch_in<ch_bool> we;)
    ch_mem<ch_uint<8>, 16> mem{"mem"};
    RamWriter(ch::Component *p = nullptr, const std::string &n = "wr") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { mem.write(io().addr, io().wdata, io().we); }
};

class RamTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("ram_top");
    __io(ch_in<ch_uint<4>> addr; ch_in<ch_uint<8>> wdata; ch_in<ch_bool> we;)
    ch::ch_module<RamWriter> wr{"wr"};
    RamTop(ch::Component *p = nullptr, const std::string &n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        wr.io().addr <<= io().addr;
        wr.io().wdata <<= io().wdata;
        wr.io().we <<= io().we;
    }
};

// ── DUT ②b：**全 inline**（三个组件都不声明成层）⇒ 单 module ⇒ 与扁平逐字节相同
class IA : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> r{0_d};
    IA(ch::Component *p = nullptr, const std::string &n = "a") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        r->next = io().din;
        io().dout <<= r;
    }
};

class IB : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<IA> a{"a"};
    IB(ch::Component *p = nullptr, const std::string &n = "b") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        a.io().din <<= io().din;
        io().dout <<= a.io().dout;
    }
};

class IInline : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<IB> b{"b"};
    IInline(ch::Component *p = nullptr, const std::string &n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        b.io().din <<= io().din;
        io().dout <<= b.io().dout;
    }
};

// ── DUT ③：已 build 但未 add_child（错误模型）────────────────────────────
class Stray : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    Stray(ch::Component *p = nullptr, const std::string &n = "stray") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class Lonely : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    Lonely(ch::Component *p = nullptr, const std::string &n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

}   // namespace

// ═════════════════════════════════════════════════════════════════════════
// 三层：3 个 module + 2 条实例化语句 · 顶层端口 = 扁平端口
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical emission: three layers", "[hierarchy][emit]") {
    ch::ch_device<T3> dev;
    const std::string path = std::string(kOutDir) + "/three_layers.v";

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);
    REQUIRE(r.files.size() == 1);

    const std::string all = read_file(path);
    REQUIRE_FALSE(all.empty());

    // 3 个 module（名字 = <type>_<序号>；根 = top）
    const std::size_t module_count = count_occurrences(all, "module ");
    REQUIRE(module_count == 3);
    REQUIRE_FALSE(module_text(all, "top").empty());
    REQUIRE(all.find("module t3_unit") == std::string::npos);   // 根用 top 这个名字
    REQUIRE(all.find("module m3_unit (") != std::string::npos);
    REQUIRE(all.find("module l3_unit (") != std::string::npos);

    // 2 条实例化语句（父 → 子）
    REQUIRE(all.find("m3_unit mid (") != std::string::npos);
    REQUIRE(all.find("l3_unit leaf (") != std::string::npos);

    // ⭐ RTL 顶层端口表：clock → reset → 无 driver 的 input → 顶层自己的 ch_out
    //    ⚠️ **成层**设计下顶层端口是扁平端口的**子集**（边界情形 #7：子输出无人消费
    //    ⇒ 它留在**子 module** 的端口上，不上 RTL 顶）⇒ ⇒ 「等于扁平」只在**全 inline** 时成立
    const std::string top_mod = module_text(all, "top");
    REQUIRE(top_mod.find("input default_clock") != std::string::npos);
    REQUIRE(top_mod.find("input default_reset") != std::string::npos);
    REQUIRE(top_mod.find("top_unnamed_input") != std::string::npos);
    REQUIRE(top_mod.find("top_unnamed_output") != std::string::npos);
    // 子模块的输出 MUST NOT 出现在顶层端口里（它是子 module 的 output）
    REQUIRE(top_mod.find("top_mid_leaf_unnamed_output") == std::string::npos);

    // ⭐ 非根 module 的端口 ⊇ 自身 ch_in ∪ ch_out
    const std::string mid_mod = module_text(all, "m3_unit");
    REQUIRE(mid_mod.find("input ") != std::string::npos);
    REQUIRE(mid_mod.find("output ") != std::string::npos);

    // ⭐ 零跨 module assign：父层只用**实例端口映射**（`.port(net)`）
    REQUIRE(top_mod.find(".top_mid_unnamed_input(top_unnamed_input)") != std::string::npos);
    REQUIRE(top_mod.find(".top_mid_unnamed_output(top_unnamed_output)") != std::string::npos);
    // ⚠️ 子 module 的信号名 MUST NOT 出现在父 module 的 body 里（= 未声明信号）
    REQUIRE(top_mod.find("top_mid_unnamed_input =") == std::string::npos);
    REQUIRE(top_mod.find("top_mid_unnamed_output =") == std::string::npos);

    // ⭐ 确定性：再生成一次 ⇒ 逐字节相同
    auto r2 = ch::toVerilogHierarchical(std::string(kOutDir) + "/three_layers_2.v",
                                        dev.instance());
    REQUIRE(r2.ok);
    REQUIRE(read_file(std::string(kOutDir) + "/three_layers_2.v") == all);
}

// ═════════════════════════════════════════════════════════════════════════
// clock / reset **分开判**：纯 RAM 写的子 module 只有 clock
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical emission: ram-only module gets clock but no reset",
          "[hierarchy][emit][clock]") {
    ch::ch_device<RamTop> dev;
    const std::string path = std::string(kOutDir) + "/ram_write.v";

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);

    const std::string all = read_file(path);
    const std::string wr = module_text(all, "ram_writer");
    REQUIRE_FALSE(wr.empty());

    // clock：mem 写端口的 `always_ff` 用到它 ⇒ MUST 是端口
    REQUIRE(wr.find("input default_clock") != std::string::npos);
    // reset：mem 写端口的 `always_ff` **没有** reset 分支 ⇒ MUST NOT 是端口
    REQUIRE(wr.find("input default_reset") == std::string::npos);
    // 根 module 无条件带 clock + reset
    const std::string top = module_text(all, "top");
    REQUIRE(top.find("input default_clock") != std::string::npos);
    REQUIRE(top.find("input default_reset") != std::string::npos);
}

// ═════════════════════════════════════════════════════════════════════════
// 错误模型：校验失败 ⇒ `ok == false` + **不留下任何新文件**
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical emission: validation failure leaves no file",
          "[hierarchy][emit][error]") {
    const std::string path = std::string(kOutDir) + "/should_not_exist.v";
    std::remove(path.c_str());
    REQUIRE_FALSE(file_exists(path));

    ch::core::context ctx("stray_ctx");
    std::unique_ptr<Lonely> top;
    std::unique_ptr<Stray> stray;
    {
        ch::core::ctx_swap swap(&ctx);
        ch::Component::ConstructionFrame frame;
        top = std::make_unique<Lonely>(nullptr, "top");
        stray = std::make_unique<Stray>(nullptr, "top.stray");
    }
    top->build(&ctx);
    stray->build(&ctx);   // ⚠️ 从不 add_child

    auto r = ch::toVerilogHierarchical(path, *top);
    REQUIRE_FALSE(r.ok);
    REQUIRE_FALSE(r.error.empty());
    INFO("error: " << r.error);
    REQUIRE(r.error.find("NOT registered") != std::string::npos);

    // 校验先于 I/O ⇒ 没有留下任何文件（连 .tmp 都没有）
    REQUIRE_FALSE(file_exists(path));
    REQUIRE_FALSE(file_exists(path + ".tmp"));
}

// ═════════════════════════════════════════════════════════════════════════
// 全 inline ⇒ 单 module，且与扁平产物**逐字节相同**（ADR-061 决策② 的「零变化」面）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical emission: all-inline design equals the flat output",
          "[hierarchy][emit][inline]") {
    ch::ch_device<IInline> dev;
    const std::string path = std::string(kOutDir) + "/all_inline.v";

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);

    const std::string hier = read_file(path);
    // 只有 1 个 module
    REQUIRE(count_occurrences(hier, "module ") == 1);
    // 没有任何实例化语句
    REQUIRE(hier.find("_unit ") == std::string::npos);

    std::ostringstream flat;
    ch::verilogwriter w(dev.context());
    w.print(flat);
    // ⚠️ 层次化文件多了「module 归属注释」前缀 ⇒ 去掉后再逐字节比
    const auto at = hier.find("/* verilator lint_off WIDTHEXPAND */");
    REQUIRE(at != std::string::npos);
    REQUIRE(hier.substr(at) == flat.str());
}
