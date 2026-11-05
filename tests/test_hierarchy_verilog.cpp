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
#include "bundle/stream_bundle.h"
#include "codegen_verilog.h"
#include "device.h"
#include "hierarchy_verilog.h"
#include "module.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
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

// ── DUT ④：**两条**兄弟边（⇒ ⇒ 两条「需要隐式端口合成」的边）────────────
//    ⇒ ⇒ 用例 MUST 有**两条** ⇒ ⇒ 否则「报全部」与「报第一条」不可区分
class SibLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("sib_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    SibLeaf(ch::Component *p = nullptr, const std::string &n = "leaf") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class TwoSiblingTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("two_sibling_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<SibLeaf> a{"a"};
    ch::ch_module<SibLeaf> b{"b"};
    ch::ch_module<SibLeaf> c{"c"};
    ch::ch_module<SibLeaf> d{"d"};
    TwoSiblingTop(ch::Component *p = nullptr, const std::string &n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        a.io().din <<= io().din;
        b.io().din <<= a.io().dout;    // ⚠️ 兄弟边 ①
        c.io().din <<= io().din;
        d.io().din <<= c.io().dout;    // ⚠️ 兄弟边 ②
        io().dout <<= b.io().dout;
    }
};

// ── DUT ⑤：**父层读子成员的内部寄存器** ⇒ ⇒ ADR-060 §2.1 **行 2** ───────
//    （子侧端点**不是**子端口 ⇒ ⇒ 必须合成**子侧 output** 端口）
class InnerRow2 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("inner_row2");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> acc{0_d};    // ⚠️ 内部寄存器（**不是**端口）
    InnerRow2(ch::Component *p = nullptr, const std::string &n = "inner")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        acc->next = io().din;
        io().dout <<= acc;
    }
};

class OuterRow2 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("outer_row2");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<InnerRow2> inner{"inner"};
    OuterRow2(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        inner.io().din <<= io().din;
        // ⚠️⚠️ 父层读**子成员的内部寄存器** ⇒ ⇒ 行 2 的典型形态
        io().dout <<= inner.instance().acc;
    }
};

// ── DUT ⑥：**父层直接改子层寄存器的 next** ⇒ ⇒ ADR-060 §2.1 **行 1** ─────
//    （子侧端点**不是**子端口 ⇒ ⇒ 必须合成**子侧 input** 端口 + 驱动重定向）
class InnerRow1 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("inner_row1");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> acc{0_d};    // ⚠️ 内部寄存器（父层直接改它的 next）
    InnerRow1(ch::Component *p = nullptr, const std::string &n = "inner")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= acc; }
};

class OuterRow1 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("outer_row1");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<InnerRow1> inner{"inner"};
    OuterRow1(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        inner.io().din <<= io().din;
        // ⚠️⚠️ 父层直接改**子层寄存器的 next** ⇒ ⇒ 行 1 的典型形态
        inner.instance().acc->next = io().din;
        io().dout <<= inner.io().dout;
    }
};

// ── DUT ⑦：**父层驱动子模块的 bundle 字段**（proxy 子形态 · ADR-060 v1.9 行 1）──
//    ⚠️ 字段节点必须先 `as_master()` 创建，否则 `<<=` 落到「另建 assign 节点」的路径
class InnerProxy : public ch::Component {
public:
    CH_HIERARCHY_LAYER("inner_proxy");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_stream<ch_uint<8>> strm;
    InnerProxy(ch::Component *p = nullptr, const std::string &n = "inner")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        strm.as_master();
        strm.payload <<= io().din;
        io().dout <<= strm.payload;
    }
};

class OuterProxy : public ch::Component {
public:
    CH_HIERARCHY_LAYER("outer_proxy");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<InnerProxy> inner{"inner"};
    OuterProxy(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        inner.io().din <<= io().din;
        // ⚠️⚠️ 父层驱动**子模块的 bundle 字段** ⇒ ⇒ proxy 子形态
        inner.instance().strm.payload <<= io().din;
        io().dout <<= inner.io().dout;
    }
};

// ── DUT ⑧：**父层改写子模块内部运算节点的操作数**（第七格 ⇒ MUST 拒绝）────
class InnerOp : public ch::Component {
public:
    CH_HIERARCHY_LAYER("inner_op");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_uint<8> sum;
    InnerOp(ch::Component *p = nullptr, const std::string &n = "inner")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        sum = io().din + ch_uint<8>(1_d);
        io().dout <<= sum;
    }
};

class OuterOp : public ch::Component {
public:
    CH_HIERARCHY_LAYER("outer_op");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<InnerOp> inner{"inner"};
    OuterOp(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        inner.io().din <<= io().din;
        // ⚠️⚠️ 父层改写**子模块内部运算节点的操作数** ⇒ ⇒ 第七格 ⇒ MUST 拒绝
        inner.instance().sum <<= io().din;
        io().dout <<= inner.io().dout;
    }
};

// ── DUT ⑨：**父层驱动子模块的 `ch_out` 端口**（第八格 ⇒ MUST 拒绝）────────
class InnerOutDrv : public ch::Component {
public:
    CH_HIERARCHY_LAYER("inner_outdrv");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    InnerOutDrv(ch::Component *p = nullptr, const std::string &n = "inner")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class OuterOutDrv : public ch::Component {
public:
    CH_HIERARCHY_LAYER("outer_outdrv");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<InnerOutDrv> inner{"inner"};
    OuterOutDrv(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        inner.io().din <<= io().din;
        // ⚠️⚠️ 父层驱动**子模块的 output 端口** ⇒ ⇒ 第八格 ⇒ MUST 拒绝
        //    （合法 Verilog 里实例的 output 只能由实例内部驱动）
        inner.io().dout <<= io().din;
        io().dout <<= inner.io().dout;
    }
};

// ── DUT ⑩：**inline 后代**的 `ch_out` 落在**非顶层**成层 module（F3）────────
//    ⚠️ `F3Leaf` **不**声明 `CH_HIERARCHY_LAYER` ⇒ ⇒ 默认 inline（无 module）
class F3Leaf : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    F3Leaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class F3Mid : public ch::Component {
public:
    CH_HIERARCHY_LAYER("f3_mid");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<F3Leaf> leaf{"leaf"};
    F3Mid(ch::Component *p = nullptr, const std::string &n = "mid")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().din;
        // ⚠️ **消费 inline 后代的 `ch_out`** ⇒ ⇒ 它落在 **mid**（非顶层成层）
        io().dout <<= leaf.io().dout;
    }
};

class F3Top : public ch::Component {
public:
    CH_HIERARCHY_LAYER("f3_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<F3Mid> mid{"mid"};
    F3Top(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mid.io().din <<= io().din;
        io().dout <<= mid.io().dout;
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
// 策略枚举的**单一权威**（ADR-060 §2.4）+ 粒度作用域（ADR-061 v1.6.3）
// ═════════════════════════════════════════════════════════════════════════

// ⚠️ 编译期判据：层次化发射的选项结构 MUST 用 `ch::PortPolicy` / `ch::UndrivenPolicy`
//    （`include/component.h` 的**唯一定义**）⇒ ⇒ **不存在第二份副本**。
static_assert(std::is_same_v<decltype(HierarchyEmitOptions{}.port_policy), ch::PortPolicy>,
              "port_policy MUST use the single authoritative ch::PortPolicy");
static_assert(std::is_same_v<decltype(HierarchyEmitOptions{}.undriven_policy), ch::UndrivenPolicy>,
              "undriven_policy MUST use the single authoritative ch::UndrivenPolicy");
static_assert(HierarchyEmitOptions{}.port_policy == ch::PortPolicy::SynthesizeWarn,
              "default port policy MUST be SynthesizeWarn (ADR-060 §2.4)");
static_assert(HierarchyEmitOptions{}.undriven_policy == ch::UndrivenPolicy::Bubble,
              "default undriven policy MUST be Bubble (ADR-060 §2.4)");

TEST_CASE("hierarchical emission: granularity scope covers construction and emission",
          "[hierarchy][emit][granularity]") {
    // ① **半生效**（负向）：scope **只**包构造、发射在 scope 之外
    //    ⇒ ⇒ 发射时全局粒度已复原 ⇒ ⇒ 产物 MUST = `RespectDeclarations` 形态。
    //    ⚠️ T3 的三个组件都**有**类级声明 ⇒ ⇒ `ForceInline` 与 `RespectDeclarations`
    //    **可区分**（否则判据退化为恒真 —— metis 的要求）。
    {
        std::unique_ptr<ch::ch_device<T3>> dev;
        {
            ch::Component::GranularityScope scope(ch::Granularity::ForceInline);
            dev = std::make_unique<ch::ch_device<T3>>();   // 构造在 scope 内
        }   // scope 析构 ⇒ 粒度复原 ⇒ **发射在 scope 之外**
        const std::string path = std::string(kOutDir) + "/gran_half_applied.v";
        auto r = ch::toVerilogHierarchical(path, dev->instance());
        INFO("error: " << r.error);
        REQUIRE(r.ok);
        const std::string all = read_file(path);
        REQUIRE(count_occurrences(all, "module ") == 3);          // ⇒ RespectDeclarations
        REQUIRE(all.find("module m3_unit (") != std::string::npos);
        REQUIRE(all.find("module l3_unit (") != std::string::npos);
    }

    // ② **正向**：scope **同时覆盖构造与发射** + `ForceInline`
    //    ⇒ ⇒ 产物 MUST 是**单 module**（全部 inline）。
    {
        const std::string path = std::string(kOutDir) + "/gran_force_inline.v";
        std::string all;
        {
            ch::Component::GranularityScope scope(ch::Granularity::ForceInline);
            ch::ch_device<T3> dev;
            auto r = ch::toVerilogHierarchical(path, dev.instance());
            INFO("error: " << r.error);
            REQUIRE(r.ok);
            all = read_file(path);
        }
        REQUIRE(count_occurrences(all, "module ") == 1);
        REQUIRE(all.find("module m3_unit (") == std::string::npos);
    }

    // ③ **正向**：`ForceLayer` 覆盖构造与发射 ⇒ ⇒ 每个组件都成层
    //    （DUT 用**全 inline** 的 `IInline` ⇒ 否则与 `RespectDeclarations` 恒等 ⇒ 判据空转）。
    {
        const std::string path = std::string(kOutDir) + "/gran_force_layer.v";
        std::string all;
        {
            ch::Component::GranularityScope scope(ch::Granularity::ForceLayer);
            ch::ch_device<IInline> dev;
            auto r = ch::toVerilogHierarchical(path, dev.instance());
            INFO("error: " << r.error);
            REQUIRE(r.ok);
            all = read_file(path);
        }
        REQUIRE(count_occurrences(all, "module ") == 3);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// 三层：3 个 module + 2 条实例化语句 · 顶层端口 = 扁平端口
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical emission: port policy decides the diagnostic breadth",
          "[hierarchy][emit][policy]") {
    ch::ch_device<TwoSiblingTop> dev;

    // ① `Reject` ⇒ 只报**第一条**（ADR-060 §2.4.1）
    {
        HierarchyEmitOptions opts;
        opts.port_policy = PortPolicy::Reject;
        const std::string path = std::string(kOutDir) + "/policy_reject.v";
        auto r = ch::toVerilogHierarchical(path, dev.instance(), opts);
        INFO("error: " << r.error);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.error.find("[1]") != std::string::npos);
        REQUIRE(r.error.find("[2]") == std::string::npos);
        REQUIRE_FALSE(file_exists(path));   // ⇒ fail-closed：不留产物
    }

    // ② `SynthesizeFatal` ⇒ 报**全部**（⇒ ⇒ 诊断 MUST 含每一条被合成的边）
    {
        HierarchyEmitOptions opts;
        opts.port_policy = PortPolicy::SynthesizeFatal;
        const std::string path = std::string(kOutDir) + "/policy_fatal.v";
        auto r = ch::toVerilogHierarchical(path, dev.instance(), opts);
        INFO("error: " << r.error);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.error.find("[1]") != std::string::npos);
        REQUIRE(r.error.find("[2]") != std::string::npos);
        REQUIRE_FALSE(file_exists(path));
    }

    // ③ `SynthesizeWarn`（**默认**）⇒ 同样报全部
    //    ⚠️ **中间态**：隐式端口**合成本身**属 B2b ⇒ ⇒ 本批一律 fail-closed
    //    ⇒ ⇒ 差异**只在诊断广度**（B2b 之后 `SynthesizeWarn` 才会真的「合成 + 产出」）。
    {
        const std::string path = std::string(kOutDir) + "/policy_warn.v";
        auto r = ch::toVerilogHierarchical(path, dev.instance());
        INFO("error: " << r.error);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.error.find("[1]") != std::string::npos);
        REQUIRE(r.error.find("[2]") != std::string::npos);
        REQUIRE_FALSE(file_exists(path));
    }
}

TEST_CASE("hierarchical emission: row-2 implicit output port is synthesized",
          "[hierarchy][emit][implicit]") {
    ch::ch_device<OuterRow2> dev;
    const std::string path = std::string(kOutDir) + "/row2_implicit.v";

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    // ⚠️ **本批之前**：这类形态是 fail-closed（响亮拒绝）⇒ ⇒ 「能产出」本身就是判别力
    REQUIRE(r.ok);
    REQUIRE(r.files.size() == 1);

    const std::string all = read_file(path);
    REQUIRE_FALSE(all.empty());

    const std::string child = module_text(all, "inner_row2");
    const std::string parent = module_text(all, "top");
    REQUIRE_FALSE(child.empty());
    REQUIRE_FALSE(parent.empty());

    // ① 子 module 有一个**合成 output 端口** `__x_0`（ADR-060 §2.2：合成名只在非顶层）
    REQUIRE(child.find("output [7:0] __x_0") != std::string::npos);
    // ② 子 module 体内：合成端口由**子侧节点**驱动
    REQUIRE(child.find("assign __x_0 = ") != std::string::npos);
    // ③ 父 module 的实例端口映射接上它（值由实例端口映射提供 ⇒ 不是跨 module assign）
    REQUIRE(parent.find(".__x_0(") != std::string::npos);
    // ④ **零跨 module assign**：父层不得引用子 module 的**内部信号**
    //    （⚠️ 端口名 `top_inner_unnamed_input` 出现在实例映射里是**合法**的 ⇒ ⇒ 只查内部信号）
    REQUIRE(parent.find("top_inner_reg") == std::string::npos);
    REQUIRE(parent.find("top_inner__reg") == std::string::npos);
    // ⑤ 合成名**只**出现在非顶层 module（顶层端口名 MUST NOT 带框架前缀）
    //    ⚠️ 合成名**合法地**出现在父层的**实例映射**里（`. __x_0(...)` ⇒ ⇒ 那是子端口名）
    //    ⇒ ⇒ 判据 MUST 只查父层的**端口声明**（module 头）⇒ ⇒ 不查 body
    const std::string parent_header = parent.substr(0, parent.find(");"));
    REQUIRE(parent_header.find("__x_") == std::string::npos);
    REQUIRE(parent_header.find("__if_") == std::string::npos);
    // ⑥ 未被子层消费的 output 端口 MUST 以**空连接**出现（`.port()`）
    //    （否则 Verilator 报 PINMISSING ⇒ ⇒ lint 失败）
    REQUIRE(parent.find(".top_inner_unnamed_output()") != std::string::npos);
}

TEST_CASE("hierarchical emission: row-1 implicit input port is synthesized",
          "[hierarchy][emit][implicit]") {
    ch::ch_device<OuterRow1> dev;
    const std::string path = std::string(kOutDir) + "/row1_implicit.v";

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    // ⚠️ **本批之前**：这类形态是 fail-closed ⇒ ⇒ 「能产出」本身就是判别力
    REQUIRE(r.ok);

    const std::string all = read_file(path);
    const std::string child = module_text(all, "inner_row1");
    const std::string parent = module_text(all, "top");
    REQUIRE_FALSE(child.empty());
    REQUIRE_FALSE(parent.empty());

    // ① 子 module 有一个**合成 input 端口** `__if_0`（值由父层提供）
    REQUIRE(child.find("input [7:0] __if_0") != std::string::npos);
    // ② ⭐ **驱动重定向**：子内的寄存器 next **改由该端口驱动**
    //    （⚠️ 不重定向 ⇒ ⇒ 该寄存器退化成「保持」⇒ ⇒ **静默丢驱动**）
    REQUIRE(child.find("__if_0;") != std::string::npos);
    REQUIRE(child.find("Register update (no next)") == std::string::npos);
    // ③ 父 module 的实例端口映射把**父层驱动**接到它
    REQUIRE(parent.find(".__if_0(") != std::string::npos);
    // ④ 子 module 内**不**出现父层的驱动信号（零跨 module 引用）
    REQUIRE(child.find("top_unnamed_input") == std::string::npos);
    // ⑤ 合成名**只**出现在非顶层 module 的**端口声明**里
    const std::string parent_header = parent.substr(0, parent.find(");"));
    REQUIRE(parent_header.find("__if_") == std::string::npos);
    REQUIRE(parent_header.find("__x_") == std::string::npos);
}

TEST_CASE("hierarchical emission: row-1 proxy sub-form is synthesized",
          "[hierarchy][emit][implicit]") {
    ch::ch_device<OuterProxy> dev;
    const std::string path = std::string(kOutDir) + "/proxy_implicit.v";

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    // ⚠️ **本批之前**：proxy 子形态是 fail-closed ⇒ ⇒ 「能产出」本身就是判别力
    REQUIRE(r.ok);

    const std::string all = read_file(path);
    const std::string child = module_text(all, "inner_proxy");
    const std::string parent = module_text(all, "top");
    REQUIRE_FALSE(child.empty());
    REQUIRE_FALSE(parent.empty());

    // ① 子 module 有**合成 input 端口**（值由父层提供）
    REQUIRE(child.find("input [7:0] __if_0") != std::string::npos);
    // ② ⭐ **proxy 的驱动改由合成端口提供** —— 这是 oracle 指出的「漏接 ⇒ 静默读 X」
    //    的失败模式：默认 warning 集**不查** `UNDRIVEN` ⇒ lint **看不见**漏接
    //    ⇒ ⇒ 这条结构断言是本形态的**唯一**本地判别力
    REQUIRE(child.find("= __if_0;") != std::string::npos);
    // ③ 该 proxy **有 `logic` 声明**（否则 `assign` 引用未声明信号 ⇒ 非法的 Verilog）
    REQUIRE(child.find("logic [7:0] top_inner_payload") != std::string::npos);
    // ④ 父 module 的实例端口映射接上它
    REQUIRE(parent.find(".__if_0(") != std::string::npos);
    // ⑤ 子 module 内**不**出现父层的驱动信号（零跨 module 引用）
    REQUIRE(child.find("top_unnamed_input") == std::string::npos);
    // ⑥ 合成名只在非顶层 module 的**端口声明**里
    const std::string parent_header = parent.substr(0, parent.find(");"));
    REQUIRE(parent_header.find("__if_") == std::string::npos);
    REQUIRE(parent_header.find("__x_") == std::string::npos);
}

TEST_CASE("hierarchical emission: matrix-out forms are rejected loudly",
          "[hierarchy][emit][implicit][reject]") {
    // ⚠️ **第七格**（父层改写子内部运算操作数）⇒ ⇒ **MUST 拒绝**（不是静默产出错产物）
    {
        ch::ch_device<OuterOp> dev;
        auto r = ch::toVerilogHierarchical(std::string(kOutDir) + "/op_implicit.v",
                                           dev.instance());
        INFO("error: " << r.error);
        REQUIRE_FALSE(r.ok);
        // 诊断 MUST 点名该形态（否则用户无法定位）
        REQUIRE(r.error.find("implicit-port") != std::string::npos);
    }
    // ⚠️ **第八格**（父层驱动子 `ch_out`）⇒ ⇒ **MUST 拒绝**
    //    （合法 Verilog 里实例的 output 只能由实例内部驱动）
    {
        ch::ch_device<OuterOutDrv> dev;
        auto r = ch::toVerilogHierarchical(std::string(kOutDir) + "/outdrv_implicit.v",
                                           dev.instance());
        INFO("error: " << r.error);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.error.find("implicit-port") != std::string::npos);
    }
}

TEST_CASE("hierarchical emission: synthesized names never collide with user nodes",
          "[hierarchy][emit][implicit][naming]") {
    // ⚠️ 合成名 `__if_<n>` / `__x_<n>` **不经过**全局命名表的去重
    //    ⇒ ⇒ 用户节点恰好同名 ⇒ **重复声明** ⇒ ⇒ 发射前 MUST 顺延
    //    （本判据用「产物里没有重复声明」+「端口名唯一」来观测）
    ch::ch_device<OuterProxy> dev;
    const std::string path = std::string(kOutDir) + "/proxy_implicit.v";
    auto r = ch::toVerilogHierarchical(path, dev.instance());
    REQUIRE(r.ok);
    const std::string all = read_file(path);
    const std::string child = module_text(all, "inner_proxy");
    // 端口声明里的合成名**只出现一次**（不重复声明）
    const std::string child_header = child.substr(0, child.find(");"));
    REQUIRE(count_occurrences(child_header, "__if_0") == 1);
    // 且产物里没有「同名两次声明」的迹象：端口表里每个 `__if_` 名字唯一
    REQUIRE(count_occurrences(child_header, "__if_") ==
            count_occurrences(child_header, "input [7:0] __if_"));
}

TEST_CASE("hierarchical emission: inline descendant output is a body wire "
          "in a non-top module", "[hierarchy][emit][scope]") {
    ch::ch_device<F3Top> dev;
    const std::string path = std::string(kOutDir) + "/f3_scope.v";

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);

    const std::string all = read_file(path);
    const std::string mid = module_text(all, "f3_mid");
    const std::string top = module_text(all, "top");
    REQUIRE_FALSE(mid.empty());
    REQUIRE_FALSE(top.empty());
    const std::string mid_header = mid.substr(0, mid.find(");"));

    // ① ⚭� **mid（非顶层成层 module）的端口表 MUST NOT 含 inline 后代的 `ch_out`**
    //    （ADR-061 决策② 两作用域表 · 下格 ⇒ 它是**体内普通信号**）
    REQUIRE(mid_header.find("_leaf_unnamed_output") == std::string::npos);
    // ② 但它**在 mid 体内** ⇒ 有 `logic` 声明 + `assign`
    //    （⚠️ 缺声明 ⇒ `assign` 引用未声明信号 ⇒ Verilator 报 undeclared ⇒ lint 失败）
    REQUIRE(mid.find("logic [7:0] top_mid_leaf_unnamed_output;") != std::string::npos);
    // ⚠️ **必须断具体的 assign 行**（断「体内出现该名字」**太弱** ⇒ ⇒ 缺 assign 也不会红）
    REQUIRE(mid.find("assign top_mid_leaf_unnamed_output = ") != std::string::npos);
    // ③ mid **自身**的 `ch_out` 仍是端口（角色① 不受影响）
    REQUIRE(mid_header.find("output [7:0] top_mid_unnamed_output") != std::string::npos);
    // ⑤ ⭐ **产物确定性**（同一设计两次生成**逐字节相同**）
    //    ⚠️ 这是**常驻防线**：实施期曾观察到一次「同源码、跨 build 的两次运行产物不同」
    //    （缺 `assign top_mid_leaf_unnamed_output = …`）⇒ ⇒ **未能复现**
    //    ⇒ ⇒ 根因未定位 ⇒ ⇒ 故 MUST 有这条判据盯着（若它再出现 ⇒ ⇒ 转红）
    {
        const std::string path2 = std::string(kOutDir) + "/f3_scope_2.v";
        auto r2 = ch::toVerilogHierarchical(path2, dev.instance());
        REQUIRE(r2.ok);
        REQUIRE(read_file(path2) == all);
    }
    // ④ 顶层端口表含 **top 自身**的 `ch_out`（顶层作用域不受影响）
    //    （⚠️ mid 的 `ch_out` 是 **mid 的**端口 ⇒ 经实例端口映射接到顶层信号
    //      ⇒ ⇒ **不在**顶层端口表里）
    REQUIRE(top.find("output [7:0] top_unnamed_output") != std::string::npos);
    REQUIRE(top.find(".top_mid_unnamed_output(top_unnamed_output)") != std::string::npos);
}

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
