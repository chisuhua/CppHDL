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
//   · 错误模型：校验失败 ⇒ `ok == false` + 无新文件；
//   · **G1 命名序号**（ADR-060 v1.12 §2.3）：同类型多实例的 module 名按**实例路径的
//     字节字典序**编号（与组件创建顺序无关）· 同 base **不同类型** ⇒ 编号前拒绝。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "bundle/stream_bundle.h"
#include "codegen_verilog.h"
#include "device.h"
#include "hierarchy_verilog.h"
#include "module.h"

// ⚠️ **内部**判据（G2 的机械形态）：`expected ⊆ actual` + 名字唯一
//    ⇒ ⇒ 单测**直接**喂构造好的端口表（否则「缺端口」这条路径在正确实现下不可达）
#include "../src/hierarchy_port_check.h"

#include <cstdio>
#include <fstream>
#include <functional>
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

// ── DUT ⑦：**同类型多实例**（ADR-060 v1.12 §2.3 的 G1）─────────────────────
//    ⚠️ 两个实例的 module 名 MUST 按**实例路径的字典序**编号（`top.a` ⇒ 首名 ·
//       `top.b` ⇒ `_1`）⇒ ⇒ 与**组件创建顺序无关** ⇒ ⇒ 下面 A 版 / B 版的成员
//       声明顺序**相反**（C++ 按声明顺序构造成员）⇒ ⇒ 产出的（module 名 ↔ 实例名）
//       配对 MUST **逐字相同**。
class SeqLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("seq_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    SeqLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class SeqTopAB : public ch::Component {
public:
    CH_HIERARCHY_LAYER("seq_top");
    __io(ch_in<ch_uint<8>> din;)
    ch::ch_module<SeqLeaf> a{"a"};   // 创建顺序 a → b
    ch::ch_module<SeqLeaf> b{"b"};
    SeqTopAB(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        a.io().din <<= io().din;
        b.io().din <<= io().din;
    }
};

class SeqTopBA : public ch::Component {
public:
    CH_HIERARCHY_LAYER("seq_top");
    __io(ch_in<ch_uint<8>> din;)
    ch::ch_module<SeqLeaf> b{"b"};   // 创建顺序 b → a（**相反**）
    ch::ch_module<SeqLeaf> a{"a"};
    SeqTopBA(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        a.io().din <<= io().din;
        b.io().din <<= io().din;
    }
};

// ── DUT ⑧：**同 base 不同类型**（G1 的「编号前判身份」）───────────────────
//    两个**不同**类型声明了**不同**的层名，但 sanitize 后落到**同一个** base
//    （`-` ⇒ `_`）⇒ ⇒ MUST 响亮拒绝（MUST NOT 静默编成 `x` / `x_1`）。
class DashLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("dash-leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    DashLeaf(ch::Component *p = nullptr, const std::string &n = "dash")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class UnderscoreLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("dash_leaf");   // ⚠️ 与 `dash-leaf` sanitize 后**同 base**
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    UnderscoreLeaf(ch::Component *p = nullptr, const std::string &n = "score")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class CollideTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("collide_top");
    __io(ch_in<ch_uint<8>> din;)
    ch::ch_module<DashLeaf> a{"a"};
    ch::ch_module<UnderscoreLeaf> b{"b"};
    CollideTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        a.io().din <<= io().din;
        b.io().din <<= io().din;
    }
};

// ── DUT ⑩：**两条第七格边**（父层改写子内部运算操作数 ⇒ 不可合成）
//    ⇒ ⇒ 用途：诊断**广度**的判据（`Reject` 只报第一条 / 其余报全部）
//    ⚠️ 兄弟边在 B2b-5 之后**可合成** ⇒ 不能再用它当「不可合成」的样本
class TwoOpTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("two_op_top");
    __io(ch_in<ch_uint<8>> din;)
    ch::ch_module<InnerOp> a{"a"};
    ch::ch_module<InnerOp> b{"b"};
    TwoOpTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        a.io().din <<= io().din;
        a.instance().sum <<= io().din;   // 第七格 ①
        b.io().din <<= io().din;
        b.instance().sum <<= io().din;   // 第七格 ②
    }
};

// ── DUT ⑪：**第三轴 · 端口适配器**（driver 在更上层 ⇒ 逐级同名 input 端口）──
//    ⚠️ mid **没有** din 端口：leaf 的 `ch_in` 由**顶层**直接驱动（跳过 mid）
class AxLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("ax_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    AxLeaf(ch::Component *p = nullptr, const std::string &n = "leaf") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class AxMid : public ch::Component {
public:
    CH_HIERARCHY_LAYER("ax_mid");
    __io(ch_out<ch_uint<8>> dout;)
    ch::ch_module<AxLeaf> leaf{"leaf"};
    AxMid(ch::Component *p = nullptr, const std::string &n = "mid") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= leaf.io().dout; }
};

class AxTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("ax_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<AxMid> mid{"mid"};
    AxTop(ch::Component *p = nullptr, const std::string &n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        // ⚠️⚠️ **第三轴**：顶层直接驱动**孙层**的 `ch_in`
        mid.instance().leaf.io().din <<= io().din;
        io().dout <<= mid.io().dout;
    }
};

// ── DUT ⑫：**第三轴 · 节点适配器**（叶端点不是端口 ⇒ 合成端口 + 驱动重定向）──
class AxRegLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("ax_reg_leaf");
    __io(ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> acc{0_d};
    AxRegLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= acc; }
};

class AxRegMid : public ch::Component {
public:
    CH_HIERARCHY_LAYER("ax_reg_mid");
    __io(ch_out<ch_uint<8>> dout;)
    ch::ch_module<AxRegLeaf> leaf{"leaf"};
    AxRegMid(ch::Component *p = nullptr, const std::string &n = "mid") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= leaf.io().dout; }
};

class AxRegTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("ax_reg_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<AxRegMid> mid{"mid"};
    AxRegTop(ch::Component *p = nullptr, const std::string &n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        // ⚠️⚠️ **第三轴 + reg 叶端点**：顶层直接改**孙层寄存器**的 next
        mid.instance().leaf.instance().acc->next = io().din;
        io().dout <<= mid.io().dout;
    }
};

// ── DUT ⑬：**行 1 · 宽度不匹配（9 → 8 截断）· 寄存器 next** ──────────────
//    ⚠️ ADR-060 W1：合成端口宽度只能取子侧 ⇒ 父侧更宽 ⇒ Verilog 静默截断
//    ⇒ ⇒ MUST 在**合成之前**拒绝（产物关掉了 WIDTHTRUNC ⇒ lint 也看不见）
class WidthRegInner : public ch::Component {
public:
    CH_HIERARCHY_LAYER("width_reg_inner");
    __io(ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> acc{0_d};
    WidthRegInner(ch::Component *p = nullptr, const std::string &n = "inner")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= acc; }
};

class WidthRegTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("width_row1_reg");
    __io(ch_in<ch_uint<9>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<WidthRegInner> inner{"inner"};
    WidthRegTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        inner.instance().acc->next = io().din;   // ⚠️ 9-bit ⇒ 8-bit
        io().dout <<= inner.io().dout;
    }
};

// ── DUT ⑭：**行 1 · 宽度不匹配 · proxy（bundle 字段）** ─────────────────
class WidthProxyInner : public ch::Component {
public:
    CH_HIERARCHY_LAYER("width_proxy_inner");
    __io(ch_out<ch_uint<8>> dout;)
    ch_stream<ch_uint<8>> strm;
    WidthProxyInner(ch::Component *p = nullptr, const std::string &n = "inner")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        strm.as_master();
        io().dout <<= strm.payload;
    }
};

class WidthProxyTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("width_row1_proxy");
    __io(ch_in<ch_uint<9>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<WidthProxyInner> inner{"inner"};
    WidthProxyTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        inner.instance().strm.payload <<= io().din;   // ⚠️ 9-bit ⇒ 8-bit
        io().dout <<= inner.io().dout;
    }
};

// ── DUT ⑮：**行 2 · 宽度不匹配（8 → 9 扩展）· 子寄存器** ────────────────
class WidthRow2Top : public ch::Component {
public:
    CH_HIERARCHY_LAYER("width_row2_reg");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<9>> dout;)
    ch::ch_module<InnerRow2> inner{"inner"};
    WidthRow2Top(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        inner.io().din <<= io().din;
        io().dout <<= inner.instance().acc;   // ⚠️ 8-bit ⇒ 9-bit
    }
};

// ── DUT ⑯：**行 2 · 宽度不匹配 · 子 proxy（bundle 字段）** ───────────────
class WidthProxyRow2Top : public ch::Component {
public:
    CH_HIERARCHY_LAYER("width_row2_proxy");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<9>> dout;)
    ch::ch_module<WidthProxyInner> inner{"inner"};
    WidthProxyRow2Top(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        inner.instance().strm.payload <<= io().din;
        io().dout <<= inner.instance().strm.payload;   // ⚠️ 8-bit ⇒ 9-bit
    }
};

// ── DUT ⑰：**被重定向的命名 proxy 驱动 inline 子组件的 `ch_in`** ────────
//    ⚠️ 专门为 `print_input` 的 driver walk 而造（R2 的第二条路径）：
//    该 proxy **有名字**（`top_mid_payload`）且**被重定向**（`assign proxy = __if_0;`）
//    ⇒ ⇒ walk MUST **停在它自己**（读本地 wire 名）；若继续穿透到 `src(0)`
//    ⇒ ⇒ 会引用**父层**的 `top_unnamed_input`（本 module 内未声明）⇒ 非法 Verilog 且零告警。
class ProxyDinLeaf : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ProxyDinLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class ProxyDinMid : public ch::Component {
public:
    CH_HIERARCHY_LAYER("proxy_din_mid");
    __io(ch_out<ch_uint<8>> dout;)
    ch_stream<ch_uint<8>> strm;
    ch::ch_module<ProxyDinLeaf> leaf{"leaf"};
    ProxyDinMid(ch::Component *p = nullptr, const std::string &n = "mid")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        strm.as_master();
        leaf.io().din <<= strm.payload;   // ⚠️ 命名 proxy 驱动 inline 子组件的输入
        io().dout <<= leaf.io().dout;
    }
};

class ProxyDinTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("proxy_din_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<ProxyDinMid> mid{"mid"};
    ProxyDinTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mid.instance().strm.payload <<= io().din;   // 父层驱动子 bundle 字段 ⇒ 重定向
        io().dout <<= mid.io().dout;
    }
};

// ── DUT ⑱：**三层 · 叶层的 `ch_in` 悬空**（F2 · 冒泡）────────────────────
//    ⚠️ 中间层**没有** din 端口 ⇒ 叶的 `ch_in` 必须逐级冒泡到顶层
//    ⇒ ⇒ 顶层可驱动 + 每层同名 + **逐跳实连**（G3-2：连接也由分析层生成）
class BubbleLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("bubble_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    BubbleLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class BubbleMid : public ch::Component {
public:
    CH_HIERARCHY_LAYER("bubble_mid");
    __io(ch_out<ch_uint<8>> dout;)
    ch::ch_module<BubbleLeaf> leaf{"leaf"};
    BubbleMid(ch::Component *p = nullptr, const std::string &n = "mid")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= leaf.io().dout; }   // leaf.din 悬空
};

class BubbleTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("bubble_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<BubbleMid> mid{"mid"};
    BubbleTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= mid.io().dout; }
};

// ── DUT ⑲：**两个实例各有一个悬空 `ch_in`**（诊断广度「全部」vs「首条」）──
class BubbleTwoTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("bubble_two_top");
    __io(ch_out<ch_uint<8>> dout;)
    ch::ch_module<BubbleLeaf> a{"a"};
    ch::ch_module<BubbleLeaf> b{"b"};
    BubbleTwoTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= a.io().dout; }   // a/b 的 din 都悬空
};

// ── DUT ⑳：**不属「隐式悬空」的两类**（`Reject` 下 MUST 仍产出）────────────
//    ① 根自己的 `ch_in`；② inline 折叠进根的悬空 `ch_in`
class InlineUndriven : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    InlineUndriven(ch::Component *p = nullptr, const std::string &n = "u")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class RootUndrivenTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("root_undriven_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<InlineUndriven> u{"u"};
    RootUndrivenTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= u.io().dout; }
};

// ── DUT ㉑：**inline 子组件的悬空 `ch_in` 折进非根成层 module** ────────────
//    ⚠️ 这是**存量缺口**（metis 指出）：原实现只给祖先登记端口，**不生成连接**
//    ⇒ ⇒ 中间层空连接 ⇒ 值传到中途断线且零报错。F2 修复 MUST 一并覆盖它。
class InlineBubbleLeaf : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    InlineBubbleLeaf(ch::Component *p = nullptr, const std::string &n = "u")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class InlineBubbleMid : public ch::Component {
public:
    CH_HIERARCHY_LAYER("inline_bubble_mid");
    __io(ch_out<ch_uint<8>> dout;)
    ch::ch_module<InlineBubbleLeaf> u{"u"};
    InlineBubbleMid(ch::Component *p = nullptr, const std::string &n = "mid")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= u.io().dout; }   // u.din 悬空（inline 折叠）
};

class InlineBubbleTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("inline_bubble_top");
    __io(ch_out<ch_uint<8>> dout;)
    ch::ch_module<InlineBubbleMid> mid{"mid"};
    InlineBubbleTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= mid.io().dout; }
};

// ── DUT ㉒：**向上 pass-through**（顶层读**孙层**的内部寄存器 ⇒ 非直接父子）──
//    ⚠️ 每级合成 **output** 端口（值向上传）· 底端是 node ⇒ 体内 `assign`
//    ⚠️ 端口名 MUST 是**合成名**（用驱动节点名 ⇒ 在驱动端 module 内重复声明）
class UpLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("up_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> acc{0_d};
    UpLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        acc->next = io().din;
        io().dout <<= acc;
    }
};

class UpMid : public ch::Component {
public:
    CH_HIERARCHY_LAYER("up_mid");
    __io(ch_out<ch_uint<8>> dout;)
    ch::ch_module<UpLeaf> leaf{"leaf"};
    UpMid(ch::Component *p = nullptr, const std::string &n = "mid")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= ch_uint<8>(1_d);
        io().dout <<= leaf.io().dout;
    }
};

class UpTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("up_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<UpMid> mid{"mid"};
    UpTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        // ⚠️⚠️ 顶层读**孙层**的内部寄存器 ⇒ ⇒ 向上 pass-through（非直接父子）
        io().dout <<= mid.instance().leaf.instance().acc;
    }
};

// ── DUT ㉓：**端口来源五类**（provenance）──────────────────────────────────
//    ① Self（kid 的 din / dout）· ② ClockReset（根无条件）· ③ InlineFolded（根读 inline 后代 out）
class InlineOutKid : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    InlineOutKid(ch::Component *p = nullptr, const std::string &n = "kid")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class InlineOutTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("inline_out_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<InlineOutKid> kid{"kid"};
    InlineOutTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        kid.io().din <<= io().din;
        io().dout <<= kid.io().dout;   // ⚠️ 根读 **inline 后代**的 ch_out ⇒ InlineFolded
    }
};

// ── DUT ㉔：**跨模块 `ch_mem`**（父层读子层持有的 mem）⇒ 拒绝 + 两条替代路径 ──
class MemChild : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_child");
    __io(ch_in<ch_uint<4>> addr; ch_out<ch_uint<8>> rdata;)
    ch_mem<ch_uint<8>, 16> mem{"mem"};
    MemChild(ch::Component *p = nullptr, const std::string &n = "child")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().rdata <<= mem.aread(io().addr); }
};

class MemParent : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_parent");
    __io(ch_in<ch_uint<4>> addr; ch_out<ch_uint<8>> rdata;)
    ch::ch_module<MemChild> child{"child"};
    MemParent(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        child.io().addr <<= io().addr;
        // ⚠️⚠️ 父层读**子层的 ch_mem** ⇒ ⇒ 跨模块 mem ⇒ MUST 拒绝
        io().rdata <<= child.instance().mem.aread(io().addr);
    }
};

// ── DUT ㉕：**成层组件自己驱动自己的 `ch_in`** ⇒ 拒绝（点名端口）──
class SelfDrivenLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("self_driven_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    SelfDrivenLeaf(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        io().din <<= ch_uint<8>(0x11_h);   // ⚠️ 子 module 内对 input 赋值 = 非法
        io().dout <<= io().din;
    }
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
    // ⚠️ 样本 = **两条第七格边**（父层改写子内部运算操作数 ⇒ **不可合成**）
    //    （兄弟边在 B2b-5 之后可合成 ⇒ 不能再用它当「不可合成」的样本）
    ch::ch_device<TwoOpTop> dev;

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

TEST_CASE("hierarchical emission: sibling edges go through a parent-module wire",
          "[hierarchy][emit][implicit][sibling]") {
    // ⚠️ 兄弟边（design §7.2.3）：共同祖先内一根**中转线** + 两处实例端口映射
    //    ⇒ ⇒ **零**隐式端口（兄弟之间本来就不该有端口）
    ch::ch_device<TwoSiblingTop> dev;
    const std::string path = std::string(kOutDir) + "/sibling_wire.v";

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    // ⚠️ **本批之前**：这是 fail-closed ⇒ ⇒ 「能产出」本身就是判别力
    REQUIRE(r.ok);

    const std::string all = read_file(path);
    const std::string top = module_text(all, "top");
    REQUIRE_FALSE(top.empty());

    // ① 两条兄弟边 ⇒ **恰好两根**中转线（每根**只声明一次**）
    REQUIRE(count_occurrences(top, "logic [7:0] top_a_unnamed_output;") == 1);
    REQUIRE(count_occurrences(top, "logic [7:0] top_c_unnamed_output;") == 1);
    // ② 驱动端（子 **output** 端口）接上该线
    REQUIRE(top.find(".top_a_unnamed_output(top_a_unnamed_output)") != std::string::npos);
    REQUIRE(top.find(".top_c_unnamed_output(top_c_unnamed_output)") != std::string::npos);
    // ③ 被驱动端（子 **input** 端口）从该线取
    REQUIRE(top.find(".top_b_unnamed_input(top_a_unnamed_output)") != std::string::npos);
    REQUIRE(top.find(".top_d_unnamed_input(top_c_unnamed_output)") != std::string::npos);
    // ④ 兄弟边**不**合成隐式端口（`__if_` / `__x_` 一个都不该出现）
    REQUIRE(top.find("__if_") == std::string::npos);
    REQUIRE(top.find("__x_") == std::string::npos);
    // ⑤ 顶层端口表不变（不因兄弟边多出任何端口）
    const std::string top_header = top.substr(0, top.find(");"));
    REQUIRE(top_header.find("__if_") == std::string::npos);
    REQUIRE(top_header.find("__x_") == std::string::npos);
}

TEST_CASE("hierarchical emission: third-axis pass-through uses port adapter",
          "[hierarchy][emit][implicit][third_axis]") {
    // ⚠️ 第三轴（ADR-060 §2.1）：driver 在**更上层** ⇒ 沿祖先链**逐级同名** input 端口
    //    ⇒ ⇒ 每一级的端口名 = 驱动节点的 **ctx 级全局名**（⇒ 顶层与扁平产物**逐字相同**）
    ch::ch_device<AxTop> dev;
    const std::string path = std::string(kOutDir) + "/third_axis_port.v";

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    // ⚠️ **本批之前**：fail-closed ⇒ ⇒ 「能产出」本身就是判别力
    REQUIRE(r.ok);

    const std::string all = read_file(path);
    const std::string mid = module_text(all, "ax_mid");
    const std::string leaf = module_text(all, "ax_leaf");
    const std::string top = module_text(all, "top");
    REQUIRE_FALSE(mid.empty());
    REQUIRE_FALSE(leaf.empty());
    REQUIRE_FALSE(top.empty());

    // ① 中间层有**同名** pass-through input 端口（名字 = 驱动节点的 ctx 级全局名）
    REQUIRE(mid.find("input [7:0] top_unnamed_input") != std::string::npos);
    // ② 顶层把自己的**同名端口**接进去（⇒ 顶层端口表 = 扁平 ⇒ 外部在同一处驱动）
    REQUIRE(top.find(".top_unnamed_input(top_unnamed_input)") != std::string::npos);
    // ③ 叶层：**port adapter** ⇒ mid 用同名端口驱动 leaf **自己的** input 端口
    REQUIRE(mid.find(".top_mid_leaf_unnamed_input(top_unnamed_input)") != std::string::npos);
    // ④ 叶 module 体内**不**引用父层信号（零跨 module 引用）
    REQUIRE(leaf.find("top_unnamed_input") == std::string::npos);
    // ⑤ 顶层端口表**不**多出框架端口
    const std::string top_header = top.substr(0, top.find(");"));
    REQUIRE(top_header.find("__if_") == std::string::npos);
    REQUIRE(top_header.find("__x_") == std::string::npos);

    // ⑥ **确定性**（同一设计两次生成逐字节相同）
    const std::string path2 = std::string(kOutDir) + "/third_axis_port_2.v";
    auto r2 = ch::toVerilogHierarchical(path2, dev.instance());
    REQUIRE(r2.ok);
    REQUIRE(read_file(path2) == all);
}

TEST_CASE("hierarchical emission: third-axis pass-through uses node adapter",
          "[hierarchy][emit][implicit][third_axis]") {
    // ⚠️ 第三轴 + **reg 叶端点**（叶端点不是端口 ⇒ 合成端口 + **驱动重定向**）
    //    ⇒ ⇒ 不重定向 ⇒ 该寄存器退化成「保持」⇒ ⇒ **静默丢驱动**
    ch::ch_device<AxRegTop> dev;
    const std::string path = std::string(kOutDir) + "/third_axis_reg.v";

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);

    const std::string all = read_file(path);
    const std::string mid = module_text(all, "ax_reg_mid");
    const std::string leaf = module_text(all, "ax_reg_leaf");
    const std::string top = module_text(all, "top");
    REQUIRE_FALSE(mid.empty());
    REQUIRE_FALSE(leaf.empty());
    REQUIRE_FALSE(top.empty());

    // ① 叶层有**同名** input 端口（node adapter 在叶层合成）
    REQUIRE(leaf.find("input [7:0] top_unnamed_input") != std::string::npos);
    // ② ⭐ **驱动重定向**：寄存器 next 改由该合成端口提供
    REQUIRE(leaf.find("top_mid_leaf_reg <= top_unnamed_input;") != std::string::npos);
    REQUIRE(leaf.find("Register update (no next)") == std::string::npos);
    // ③ 中间层同样有同名端口，并由上一层驱动
    REQUIRE(mid.find("input [7:0] top_unnamed_input") != std::string::npos);
    REQUIRE(mid.find(".top_unnamed_input(top_unnamed_input)") != std::string::npos);
    // ④ 顶层接自己的同名端口
    REQUIRE(top.find(".top_unnamed_input(top_unnamed_input)") != std::string::npos);
}

TEST_CASE("hierarchical emission: driver walk stops at the redirected named proxy",
          "[hierarchy][emit][implicit][walk]") {
    // ⚠️ R2 的第二条 `src(0)` 路径（`print_input` 的 driver walk）：
    //    守卫 = 「只穿透**未命名** proxy」⇒ 被重定向的 proxy **有名字** ⇒ 天然停下。
    //    ⚠️ 若不守：穿透到 `src(0)` ⇒ 父层信号名 ⇒ 本 module 内**未声明** ⇒ 非法 Verilog。
    ch::ch_device<ProxyDinTop> dev;
    const std::string path = std::string(kOutDir) + "/walk_named_proxy.v";
    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);

    const std::string all = read_file(path);
    const std::string mid = module_text(all, "proxy_din_mid");
    REQUIRE_FALSE(mid.empty());

    // ① 重定向本身生效：命名 proxy 由合成端口驱动（本地 wire）
    REQUIRE(mid.find("assign top_mid_payload = __if_0;") != std::string::npos);
    // ② ⭐ inline 子组件的输入**读该 proxy 的本地 wire 名**
    REQUIRE(mid.find("assign top_mid_leaf_unnamed_input = top_mid_payload;") !=
            std::string::npos);
    // ③ 负向：MUST NOT 穿透到父层节点（`top_unnamed_input` 在本 module 内未声明）
    REQUIRE(mid.find("top_unnamed_input") == std::string::npos);
}

TEST_CASE("hierarchical emission: cross-module width mismatch is rejected before synthesis",
          "[hierarchy][emit][width]") {
    // ⚠️ **W1**（ADR-060 v1.10 · v1.13 覆盖**全部**合成 site）：
    //    合成端口宽度只能取**子侧** ⇒ 两端不一致 ⇒ Verilog 静默扩展/截断
    //    ⇒ ⇒ MUST 在**合成之前**拒绝，且诊断点名两端与两个宽度 + 给出可行动修法。
    //    ⚠️ 四条路径**各一**：行 1 / 行 2 × {寄存器, proxy}，双向（截断 9→8 / 扩展 8→9）。
    struct Case {
        const char *name;
        const char *expect;
        std::function<ch::HierarchyEmitResult(const std::string &)> run;
    };
    const std::vector<Case> cases = {
        {"width_row1_reg", "9-bit driver -> 8-bit target",
         [](const std::string &p) {
             ch::ch_device<WidthRegTop> d;
             return ch::toVerilogHierarchical(p, d.instance());
         }},
        {"width_row1_proxy", "9-bit driver -> 8-bit target",
         [](const std::string &p) {
             ch::ch_device<WidthProxyTop> d;
             return ch::toVerilogHierarchical(p, d.instance());
         }},
        {"width_row2_reg", "8-bit driver -> 9-bit target",
         [](const std::string &p) {
             ch::ch_device<WidthRow2Top> d;
             return ch::toVerilogHierarchical(p, d.instance());
         }},
        {"width_row2_proxy", "8-bit driver -> 9-bit target",
         [](const std::string &p) {
             ch::ch_device<WidthProxyRow2Top> d;
             return ch::toVerilogHierarchical(p, d.instance());
         }},
    };
    for (const auto &c : cases) {
        const std::string path = std::string(kOutDir) + "/" + c.name + ".v";
        std::remove(path.c_str());   // ⚠️ 判据依赖「失败不留文件」⇒ 先清掉上一轮（含变异轮）的残留
        auto r = c.run(path);
        INFO("case: " << c.name << " · error: " << r.error);
        REQUIRE_FALSE(r.ok);
        REQUIRE_FALSE(file_exists(path));   // ⇒ fail-closed：不留产物
        REQUIRE(r.error.find("width mismatch") != std::string::npos);
        REQUIRE(r.error.find(c.expect) != std::string::npos);
        REQUIRE(r.error.find("Remedy:") != std::string::npos);
    }
}

TEST_CASE("hierarchical emission: dangling layered input bubbles to the top with real links",
          "[hierarchy][emit][bubble]") {
    // ⚠️ **F2**（ADR-060 v1.13 G3）：成层组件的悬空 `ch_in` MUST 逐级冒泡
    //    ⇒ ⇒ ① 顶层可驱动 · ② 每级同名 · ③ **逐跳实连**（G3-2，连接也由分析层生成）
    //       · ④ 端口名字 = ctx 级全局名（与扁平逐字相同）· ⑤ 端口旁注释 · ⑥ 每条一条 CHWARN
    ch::ch_device<BubbleTop> dev;
    const std::string path = std::string(kOutDir) + "/bubble.v";
    std::remove(path.c_str());
    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);

    const std::string all = read_file(path);
    const std::string top = module_text(all, "top");
    const std::string mid = module_text(all, "bubble_mid");
    const std::string leaf = module_text(all, "bubble_leaf");
    REQUIRE_FALSE(top.empty());
    REQUIRE_FALSE(mid.empty());
    REQUIRE_FALSE(leaf.empty());

    // ① 顶层有**同名 input**（名字 = ctx 级全局名 ⇒ 与扁平逐字相同）
    const std::string name = "top_mid_leaf_unnamed_input";
    REQUIRE(top.find("input [7:0] " + name) != std::string::npos);
    // ② 中间层也有（逐级同名）
    REQUIRE(mid.find("input [7:0] " + name) != std::string::npos);
    // ③ ⭐ **逐跳实连**：两处实例映射都接上（`.X(X)` 而不是空连接）
    REQUIRE(mid.find("." + name + "(" + name + ")") != std::string::npos);
    REQUIRE(top.find("." + name + "(" + name + ")") != std::string::npos);
    REQUIRE(all.find("." + name + "()") == std::string::npos);
    // ④ 叶 module 体内**不引用**父层信号（值只经实例端口传入）
    REQUIRE(leaf.find("top_unnamed_input") == std::string::npos);
    // ⑤ 端口旁注释（来源可追溯）
    //    ⚠️ 注释里的路径 = **组件全路径**（module key），不是 module_type_name
    REQUIRE(top.find("// implicit: undriven input of top.mid.leaf") != std::string::npos);
    REQUIRE(mid.find("// implicit: undriven input of top.mid.leaf") != std::string::npos);
    // ⑥ 顶层端口顺序：bubbled input MUST 排在 `ch_out` **之前**
    const auto in_pos = top.find("input [7:0] " + name);
    const auto out_pos = top.find("output [7:0] top_unnamed_output");
    REQUIRE(in_pos != std::string::npos);
    REQUIRE(out_pos != std::string::npos);
    REQUIRE(in_pos < out_pos);
    // ⑦ 每条悬空输入**恰好一条**告警（分析层不重复打）
    std::size_t bubble_warnings = 0;
    for (const auto &w : r.warnings) {
        if (w.find(name) != std::string::npos) {
            ++bubble_warnings;
        }
    }
    REQUIRE(bubble_warnings == 1);
}

TEST_CASE("hierarchical emission: inline descendant's dangling input also gets real links",
          "[hierarchy][emit][bubble][inline]") {
    // ⚠️ **存量缺口**（metis · ADR-060 v1.13 G3-2）：inline 组件的悬空 `ch_in`
    //    折进**非根**成层 module 时，原实现只登记端口、**不生成连接**
    //    ⇒ ⇒ 中间层空连接 ⇒ 值传到中途断线，且结构 / lint 都看不见。
    ch::ch_device<InlineBubbleTop> dev;
    const std::string path = std::string(kOutDir) + "/bubble_inline.v";
    std::remove(path.c_str());
    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);

    const std::string all = read_file(path);
    const std::string top = module_text(all, "top");
    const std::string mid = module_text(all, "inline_bubble_mid");
    REQUIRE_FALSE(top.empty());
    REQUIRE_FALSE(mid.empty());

    const std::string name = "top_mid_u_unnamed_input";
    REQUIRE(mid.find("input [7:0] " + name) != std::string::npos);
    REQUIRE(top.find("input [7:0] " + name) != std::string::npos);
    // ⭐ 逐跳实连（这条正是原实现缺失的）
    REQUIRE(top.find("." + name + "(" + name + ")") != std::string::npos);
    REQUIRE(all.find("." + name + "()") == std::string::npos);
    // 告警恰好一条（inline 折叠后的 owner = inline_bubble_mid ⇒ 跨边界 ⇒ 属悬空）
    std::size_t n = 0;
    for (const auto &w : r.warnings) {
        if (w.find(name) != std::string::npos) {
            ++n;
        }
    }
    REQUIRE(n == 1);
}

TEST_CASE("hierarchical emission: upward pass-through synthesizes output ports on each level",
          "[hierarchy][emit][upward]") {
    // ⚠️ ADR-060 v1.13 G3 / G0：有 driver 的**向上** pass-through（子 → 祖先，非直接父子）
    //    ⇒ ⇒ 每级合成 **output** 端口（值向上传），与冒泡**同一套链**（方向相反）。
    ch::ch_device<UpTop> dev;
    const std::string path = std::string(kOutDir) + "/upward.v";
    std::remove(path.c_str());
    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE(r.ok);

    const std::string all = read_file(path);
    const std::string top = module_text(all, "top");
    const std::string mid = module_text(all, "up_mid");
    const std::string leaf = module_text(all, "up_leaf");
    REQUIRE_FALSE(top.empty());
    REQUIRE_FALSE(mid.empty());
    REQUIRE_FALSE(leaf.empty());

    // ① 叶层：合成 **output** 端口 `__up_0` + 体内由驱动节点赋值
    REQUIRE(leaf.find("output [7:0] __up_0") != std::string::npos);
    REQUIRE(leaf.find("assign __up_0 = top_mid_leaf__reg;") != std::string::npos);
    // ② 中间层：同名 output 端口，由**子实例**驱动（⚠️ MUST NOT 出现 `assign X = ;`）
    REQUIRE(mid.find("output [7:0] __up_0") != std::string::npos);
    REQUIRE(mid.find(".__up_0(__up_0)") != std::string::npos);
    REQUIRE(all.find("assign __up_0 = ;") == std::string::npos);
    // ③ 顶层：孙层寄存器**直连**顶层 output（值向上贯通，零跨 module assign）
    REQUIRE(top.find(".__up_0(top_unnamed_output)") != std::string::npos);
    // ④ 叶 module 体内 MUST NOT 引用顶层信号名
    REQUIRE(leaf.find("top_unnamed_output") == std::string::npos);
}

TEST_CASE("hierarchical emission: UndrivenPolicy::Reject lists every dangling input",
          "[hierarchy][emit][bubble][policy]") {
    // ⚠️ ADR-060 v1.13 §2.4.2：`Reject` ⇒ **不产出** + 列出**全部**悬空输入（不是首条）
    //    ⇒ ⇒ 诊断广度用例 MUST 含**两个**实例（否则「全部」与「首条」不可区分）
    {
        ch::ch_device<BubbleTwoTop> dev;
        const std::string path = std::string(kOutDir) + "/bubble_reject_two.v";
        std::remove(path.c_str());
        HierarchyEmitOptions opts;
        opts.undriven_policy = UndrivenPolicy::Reject;
        auto r = ch::toVerilogHierarchical(path, dev.instance(), opts);
        INFO("error: " << r.error);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.error.find("top_a_unnamed_input") != std::string::npos);
        REQUIRE(r.error.find("top_b_unnamed_input") != std::string::npos);
        // 冒泡层级链（点名「冒泡到哪几层」）
        REQUIRE(r.error.find("bubbles: top.a -> top") != std::string::npos);
        REQUIRE(r.error.find("bubbles: top.b -> top") != std::string::npos);
        REQUIRE_FALSE(file_exists(path));   // fail-closed：不留产物
    }
    // 防误拒：根自己的 `ch_in` + inline 折叠进根的悬空 `ch_in` ⇒ **不是**隐式悬空端口
    {
        ch::ch_device<RootUndrivenTop> dev;
        const std::string path = std::string(kOutDir) + "/bubble_reject_root.v";
        std::remove(path.c_str());
        HierarchyEmitOptions opts;
        opts.undriven_policy = UndrivenPolicy::Reject;
        auto r = ch::toVerilogHierarchical(path, dev.instance(), opts);
        INFO("error: " << r.error);
        REQUIRE(r.ok);   // ⚠️ 拒绝它们 = 拒绝合法设计（扁平里它们本就是顶层 input）
        REQUIRE(file_exists(path));
    }
}

TEST_CASE("hierarchical emission: port provenance covers all five origins",
          "[hierarchy][emit][provenance]") {
    // ⚠️ G2（ADR-058 v1.12 §2.3）：端口清单 = **value-only**，覆盖五类来源，
    //    且每个条目 MUST 能在产物里找到（expected ⊆ actual 的外部可观测形态）。
    auto width_text = [](std::uint32_t w) {
        return w <= 1 ? std::string() : ("[" + std::to_string(w - 1) + ":0] ");
    };
    auto check_entries = [&](const ch::HierarchyEmitResult &r, const std::string &path) {
        const std::string all = read_file(path);
        REQUIRE_FALSE(all.empty());
        for (const auto &p : r.port_provenance) {
            const std::string mod = module_text(all, p.module);
            INFO("entry: " << p.module << " / " << p.port << " / " << p.width);
            REQUIRE_FALSE(mod.empty());
            const std::string decl =
                std::string(p.is_input ? "input " : "output ") + width_text(p.width) + p.port;
            REQUIRE(mod.find(decl) != std::string::npos);
        }
    };
    auto origins_of = [](const ch::HierarchyEmitResult &r) {
        std::set<ch::PortProvenanceOrigin> s;
        for (const auto &p : r.port_provenance) {
            s.insert(p.origin);
        }
        return s;
    };

    // ① 自身 + clock/reset + inline 折叠
    {
        ch::ch_device<InlineOutTop> dev;
        const std::string path = std::string(kOutDir) + "/prov_inline.v";
        std::remove(path.c_str());
        auto r = ch::toVerilogHierarchical(path, dev.instance());
        INFO("error: " << r.error);
        REQUIRE(r.ok);
        const auto o = origins_of(r);
        REQUIRE(o.count(ch::PortProvenanceOrigin::Self) == 1);
        REQUIRE(o.count(ch::PortProvenanceOrigin::ClockReset) == 1);
        REQUIRE(o.count(ch::PortProvenanceOrigin::InlineFolded) == 1);
        check_entries(r, path);
    }

    // ② 冒泡（跨 module 边界）+ 确定性
    {
        ch::ch_device<BubbleTop> dev;
        const std::string path = std::string(kOutDir) + "/prov_bubble.v";
        const std::string path2 = std::string(kOutDir) + "/prov_bubble_2.v";
        std::remove(path.c_str());
        std::remove(path2.c_str());
        auto r = ch::toVerilogHierarchical(path, dev.instance());
        auto r2 = ch::toVerilogHierarchical(path2, dev.instance());
        INFO("error: " << r.error);
        REQUIRE(r.ok);
        REQUIRE(r2.ok);
        REQUIRE(origins_of(r).count(ch::PortProvenanceOrigin::Bubbled) == 1);
        // 确定性：两次生成的清单逐项相同（顺序也相同）
        REQUIRE(r.port_provenance.size() == r2.port_provenance.size());
        for (std::size_t i = 0; i < r.port_provenance.size(); ++i) {
            REQUIRE(r.port_provenance[i].module == r2.port_provenance[i].module);
            REQUIRE(r.port_provenance[i].port == r2.port_provenance[i].port);
            REQUIRE(r.port_provenance[i].is_input == r2.port_provenance[i].is_input);
            REQUIRE(r.port_provenance[i].width == r2.port_provenance[i].width);
            REQUIRE(r.port_provenance[i].origin == r2.port_provenance[i].origin);
        }
        check_entries(r, path);
    }

    // ③ 合成端口（行 1 的 `__if_<n>`）
    {
        ch::ch_device<OuterRow1> dev;
        const std::string path = std::string(kOutDir) + "/prov_synth.v";
        std::remove(path.c_str());
        auto r = ch::toVerilogHierarchical(path, dev.instance());
        INFO("error: " << r.error);
        REQUIRE(r.ok);
        REQUIRE(origins_of(r).count(ch::PortProvenanceOrigin::Synthesized) == 1);
        bool named = false;
        for (const auto &p : r.port_provenance) {
            if (p.origin == ch::PortProvenanceOrigin::Synthesized) {
                REQUIRE(p.port.rfind("__if_", 0) == 0);
                named = true;
            }
        }
        REQUIRE(named);
        check_entries(r, path);
    }
}

TEST_CASE("hierarchical emission: port table guard rejects missing/duplicate/mismatched ports",
          "[hierarchy][emit][provenance][guard]") {
    // ⚠️ G2 的**机械判据**单测：正确实现下「缺端口」这条路径从公开 API 不可达
    //    ⇒ ⇒ 直接喂构造好的端口表（否则判据无法证明守卫本身有判别力）
    using ch::PortProvenance;
    using ch::PortProvenanceOrigin;
    const PortProvenance a{"m", "a", true, 8, PortProvenanceOrigin::Self};
    REQUIRE(ch::detail::check_port_table({a}, {a}).empty());
    // ① 缺端口 ⇒ 点名 module + 端口
    const auto missing = ch::detail::check_port_table({a}, {});
    REQUIRE(missing.find("'a'") != std::string::npos);
    REQUIRE(missing.find("'m'") != std::string::npos);
    REQUIRE(missing.find("missing") != std::string::npos);
    // ② 方向 / 宽度不一致 ⇒ 不是「名字对上就算」
    REQUIRE(ch::detail::check_port_table(
                {a}, {PortProvenance{"m", "a", false, 8, PortProvenanceOrigin::Self}})
                .find("inconsistent") != std::string::npos);
    REQUIRE(ch::detail::check_port_table(
                {a}, {PortProvenance{"m", "a", true, 9, PortProvenanceOrigin::Self}})
                .find("inconsistent") != std::string::npos);
    // ③ 同 module 内重名 ⇒ 拒绝
    REQUIRE(ch::detail::check_port_table({a, a}, {a, a}).find("duplicate") != std::string::npos);
}

TEST_CASE("hierarchical emission: interface whitelist decision table",
          "[hierarchy][emit][whitelist]") {
    // ⚠️ ADR-062 §2.6：接口 MUST 限于 `ch_uint` / `ch_bool`（+ 框架供给的 clock/reset）
    //    ⇒ ⇒ 端口节点类型 MUST ∈ {input, output, clock, reset}，其余一律拒绝。
    using ch::core::lnodetype;
    for (auto ty : {lnodetype::type_input, lnodetype::type_output, lnodetype::type_clock,
                    lnodetype::type_reset}) {
        REQUIRE(ch::detail::is_whitelisted_port_node(ty));
    }
    for (auto ty : {lnodetype::type_none, lnodetype::type_lit, lnodetype::type_proxy,
                    lnodetype::type_op, lnodetype::type_reg, lnodetype::type_mem,
                    lnodetype::type_mux, lnodetype::type_mem_read_port,
                    lnodetype::type_mem_write_port, lnodetype::type_bitsupdate}) {
        INFO("type: " << static_cast<int>(ty));
        REQUIRE_FALSE(ch::detail::is_whitelisted_port_node(ty));
    }
    // 拒绝 MUST 点名 module + 端口（ADR-062 §2.6）
    const auto err = ch::detail::check_interface_whitelist(
        "m", {{"bad", lnodetype::type_mem_read_port}});
    REQUIRE_FALSE(err.empty());
    REQUIRE(err.find("'bad'") != std::string::npos);
    REQUIRE(err.find("'m'") != std::string::npos);
    REQUIRE(err.find("ADR-062") != std::string::npos);
}

TEST_CASE("hierarchical emission: rejection diagnostics name the offending module and port",
          "[hierarchy][emit][reject][naming]") {
    // ⚠️ B3b：每条拒绝 MUST 在 `HierarchyEmitResult.error` 里**点名**（不是只打 stderr）
    //    ① 跨模块 `ch_mem` ⇒ 点名两端 module + 给出两条「mem 与使用者同层」的替代路径
    {
        ch::ch_device<MemParent> dev;
        const std::string path = std::string(kOutDir) + "/reject_mem.v";
        std::remove(path.c_str());
        auto r = ch::toVerilogHierarchical(path, dev.instance());
        INFO("error: " << r.error);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.error.find("cross-module ch_mem") != std::string::npos);
        // 诊断点名两端 **module key**（= 组件路径；不是 module_type_name）
        REQUIRE(r.error.find("'top.child'") != std::string::npos);
        REQUIRE(r.error.find("'top'") != std::string::npos);
        REQUIRE(r.error.find("(1)") != std::string::npos);
        REQUIRE(r.error.find("(2)") != std::string::npos);
        REQUIRE_FALSE(file_exists(path));
    }
    // ② 成层组件自驱动 `ch_in` ⇒ 点名端口
    {
        ch::ch_device<SelfDrivenLeaf> dev;
        const std::string path = std::string(kOutDir) + "/reject_selfdrv.v";
        std::remove(path.c_str());
        auto r = ch::toVerilogHierarchical(path, dev.instance());
        INFO("error: " << r.error);
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.error.find("drives its own") != std::string::npos);
        REQUIRE(r.error.find("unnamed_input") != std::string::npos);
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
        // ⚠️ B3b：点名**节点 + module key**（MUST NOT 只抓 stderr）
        REQUIRE(r.error.find("top_inner_bits") != std::string::npos);
        REQUIRE(r.error.find("'top.inner'") != std::string::npos);
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
        // ⚠️ B3b：点名**子侧 output 端口 + module key**
        REQUIRE(r.error.find("top_inner_unnamed_output") != std::string::npos);
        REQUIRE(r.error.find("'top.inner'") != std::string::npos);
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

// ═════════════════════════════════════════════════════════════════════════
// G1（ADR-060 v1.12 §2.3）：同类型多实例的序号按**实例路径的字典序**
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical emission: same-type multi-instance numbering follows instance path order",
          "[hierarchy][emit][naming][g1]") {
    // ⚠️ 编号约定（既有 · 与 G1 正交）：组内**首份**用 `<type>`、其后 `<type>_<k>`
    //    （见 `src/hierarchy_verilog.cpp` 的 `numbered_name`）
    const std::string path_ab = std::string(kOutDir) + "/seq_ab.v";
    const std::string path_ba = std::string(kOutDir) + "/seq_ba.v";

    ch::ch_device<SeqTopAB> ab;
    auto r_ab = ch::toVerilogHierarchical(path_ab, ab.instance());
    INFO("error (a→b): " << r_ab.error);
    REQUIRE(r_ab.ok);

    ch::ch_device<SeqTopBA> ba;
    auto r_ba = ch::toVerilogHierarchical(path_ba, ba.instance());
    INFO("error (b→a): " << r_ba.error);
    REQUIRE(r_ba.ok);

    const std::string all_ab = read_file(path_ab);
    const std::string all_ba = read_file(path_ba);

    // ① 两个同类型实例**各发一份** module（2 个 leaf + root）
    REQUIRE(count_occurrences(all_ab, "module ") == 3);
    REQUIRE_FALSE(module_text(all_ab, "seq_leaf").empty());
    REQUIRE_FALSE(module_text(all_ab, "seq_leaf_1").empty());

    // ② 名字 ↔ 实例的绑定按**实例路径**：`top.a` ⇒ `seq_leaf` · `top.b` ⇒ `seq_leaf_1`
    const std::string top_ab = module_text(all_ab, "top");
    REQUIRE(top_ab.find("seq_leaf a (") != std::string::npos);
    REQUIRE(top_ab.find("seq_leaf_1 b (") != std::string::npos);

    // ③ ⭐ **交换组件创建顺序 MUST NOT 改变这两个名字**
    //    （⚠️ 判据要害：按「analysis.modules 的遍历序 = 创建顺序」编号的实现
    //      ⇒ ⇒ B 版会把 `seq_leaf` 给 b ⇒ ⇒ 这两条转红）
    const std::string top_ba = module_text(all_ba, "top");
    REQUIRE(top_ba.find("seq_leaf a (") != std::string::npos);
    REQUIRE(top_ba.find("seq_leaf_1 b (") != std::string::npos);
    REQUIRE(top_ba.find("seq_leaf b (") == std::string::npos);

    // ④ 两份产物的**实例化语句**（module 名 ↔ 实例名）逐字相同 ⇒ ⇒ 名字只由**路径**决定
    auto inst_line = [](const std::string &all, const std::string &inst) {
        const auto at = all.find(inst);
        if (at == std::string::npos) {
            return std::string("<missing>");
        }
        const auto nl = all.rfind('\n', at);
        const std::size_t from = (nl == std::string::npos) ? 0 : nl + 1;
        const auto end = all.find('\n', at);
        return all.substr(from, end - from);
    };
    REQUIRE(inst_line(all_ab, " a (") == inst_line(all_ba, " a ("));
    REQUIRE(inst_line(all_ab, " b (") == inst_line(all_ba, " b ("));
}

TEST_CASE("hierarchical emission: same-base different-type modules are rejected before numbering",
          "[hierarchy][emit][naming][g1][error]") {
    // ⚠️ 两个**不同类型**的层名分别是 `dash-leaf` / `dash_leaf` ⇒ ⇒ sanitize 后**同 base**
    //    ⇒ ⇒ 现状（只查**生成后**重名）会静默编成 `dash_leaf` / `dash_leaf_1`
    //    ⇒ ⇒ 本判据要求**编号前**判身份 ⇒ ⇒ 响亮拒绝并点名两端
    ch::ch_device<CollideTop> dev;
    const std::string path = std::string(kOutDir) + "/collide.v";
    std::remove(path.c_str());

    auto r = ch::toVerilogHierarchical(path, dev.instance());
    INFO("error: " << r.error);
    REQUIRE_FALSE(r.ok);
    REQUIRE(r.error.find("dash_leaf") != std::string::npos);
    // 诊断 MUST 点名**两个** module key（否则用户无法定位是哪两个实例）
    REQUIRE(r.error.find("top.a") != std::string::npos);
    REQUIRE(r.error.find("top.b") != std::string::npos);
    // 校验先于 I/O ⇒ 不留文件
    REQUIRE_FALSE(file_exists(path));
    REQUIRE_FALSE(file_exists(path + ".tmp"));
}
