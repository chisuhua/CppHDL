// tests/test_hierarchy_friendliness_diagnosis.cpp
//
// 层次友好度诊断 API（capability `hierarchy-friendliness-diagnosis`）的判据。
//
// 契约要点：
// · 入口**只读**（调用 MUST NOT 改图 / 改 ctx / 改产物）
// · 报告给 5 项**可判定**事实（隐式边数 / 合成端口数 / 悬空输入数 /
//   未消费子输出数 / 跨模块 mem 数）+ 各层规模
// · 「未测得」与「实测为 0」**结构上可区分**（5 项全是 `std::optional`）
// · 与发射器**同源**（合成端口数 MUST 等于发射产物里的隐式合成端口数）
// · 快照**确定性**（两次诊断逐字节相同）
//
// ⚠️ 本文件钉的是**实施期实测出来的**三条关键性质：
//   ① 「边」与「端口」是**两个不同的量**（一个子内节点驱动两个父侧端点
//      ⇒ 边 2 / 端口 1）⇒ ⇒ 「隐式端口数 = pending_implicit_edges.size()」是错的；
//   ② `node->name()` 对所有 `ch_in` 恒为 `"unnamed_input"` ⇒ ⇒ 明细 MUST 用
//      **Verilog 名**（否则两条明细不可区分 ⇒ 「断言子串」的判据照样全绿）；
//   ③ verilator 的 `isPrimaryIO` 只标**顶层** ⇒ ⇒ 判「非根 module 的端口」
//      MUST 用 `--module-ports` 那种「一级 VAR + direction」口径（⇒ 恒绿假绿）。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "device.h"
#include "hierarchy_analysis.h"
#include "hierarchy_verilog.h"
#include "module.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ── DUT ①：父层读**子成员的内部寄存器** ⇒ ⇒ 隐式边（`__x_` 形态）─────────
class XChild : public Component {
public:
    CH_HIERARCHY_LAYER("x_child");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> acc{0_d};      // ⚠️ **内部**寄存器（不是端口）
    XChild(Component *p = nullptr, const std::string &n = "child")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        acc->next = io().din;
        io().dout <<= acc;
    }
};

class XTop : public Component {
public:
    CH_HIERARCHY_LAYER("x_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_module<XChild> child{"child"};
    XTop(Component *p = nullptr, const std::string &n = "top")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        child.io().din <<= io().din;
        io().dout <<= child.instance().acc;     // ⚠️ 父层读**子内部寄存器**
    }
};

// ── DUT ②：**一个**子内节点驱动**两个**父侧端点 ⇒ ⇒ 边 2 / 端口 1 ─────────
class TwoTargetTop : public Component {
public:
    CH_HIERARCHY_LAYER("two_target_top");
    // ⚠️ **两个**父侧输出端口 ⇒ ⇒ 同一个子内节点可驱动**两个不同的父侧 proxy**
    //    ⇒ ⇒ 两条隐式边（acc→dout · acc→dout2）
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout; ch_out<ch_uint<8>> dout2;)
    ch_module<XChild> child{"child"};
    TwoTargetTop(Component *p = nullptr, const std::string &n = "top")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        child.io().din <<= io().din;
        // ⚠️ 同一个子内节点（`acc`）驱动**两个**父侧 proxy ⇒ ⇒ 两条隐式边
        //    ⇒ ⇒ 但合成端口按 (子 module, 子侧节点) **去重** ⇒ ⇒ **只合成 1 个**
        //    ⚠️ **MUST 是两个不同的父侧 proxy** ⇒ 写两次同一个 proxy 会被
        //    **last-wins 覆盖** ⇒ ⇒ 只剩**一条**边（实测）
        io().dout <<= child.instance().acc;
        io().dout2 <<= child.instance().acc;
    }
};

// ── DUT ③：子 `ch_out` **父层不消费** ⇒ ⇒ 未消费子输出 ────────────────────
class UnconsumedChild : public Component {
public:
    CH_HIERARCHY_LAYER("unconsumed_child");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    UnconsumedChild(Component *p = nullptr, const std::string &n = "child")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class UnconsumedTop : public Component {
public:
    CH_HIERARCHY_LAYER("unconsumed_top");
    __io(ch_in<ch_uint<8>> din;)
    ch_module<UnconsumedChild> child{"child"};
    UnconsumedTop(Component *p = nullptr, const std::string &n = "top")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        child.io().din <<= io().din;
        // ⚠️ **不**读 `child.io().dout` ⇒ ⇒ 子输出无人消费
    }
};

// ── DUT ④：**多实例**（同一 module 两个实例）· 只有一个实例的输出被消费 ────
class MultiInstTop : public Component {
public:
    CH_HIERARCHY_LAYER("multi_inst_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_module<UnconsumedChild> a{"a"};
    ch_module<UnconsumedChild> b{"b"};
    MultiInstTop(Component *p = nullptr, const std::string &n = "top")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        a.io().din <<= io().din;
        b.io().din <<= io().din;
        io().dout <<= a.io().dout;      // ⚠️ 只消费**实例 a** 的输出
        // ⇒ 实例 b 的输出**无人消费** ⇒ ⇒ 按实例判 ⇒ MUST 算「未消费」
    }
};

// ── DUT ⑤：**两个**悬空 `ch_in` ⇒ ⇒ 两条明细的节点名 MUST 互不相同 ────────
class DanglingLeaf : public Component {
public:
    CH_HIERARCHY_LAYER("dangling_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    DanglingLeaf(Component *p = nullptr, const std::string &n = "leaf")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class DanglingMid : public Component {
public:
    CH_HIERARCHY_LAYER("dangling_mid");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_module<DanglingLeaf> leaf{"leaf"};
    DanglingMid(Component *p = nullptr, const std::string &n = "mid")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        // ⚠️ `leaf.io().din` **不驱动** ⇒ ⇒ 悬空 ⇒ ⇒ 逐级冒泡
        io().dout <<= leaf.io().dout;
    }
};

class DanglingTop : public Component {
public:
    CH_HIERARCHY_LAYER("dangling_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_module<DanglingMid> mid{"mid"};
    DanglingTop(Component *p = nullptr, const std::string &n = "top")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mid.io().din <<= io().din;
        io().dout <<= mid.io().dout;
    }
};

// ── DUT ⑤-b：**两个**不同名的悬空 `ch_in` ⇒ ⇒ 明细 MUST 互不相同 ──────────
//    ⚠️ **必须有两个** ⇒ 只用一个时「名字互不相同」的比较循环**空转** ⇒ ⇒ 恒绿
//    ⇒ ⇒ 那是**假绿**（变异 M5 实测：把明细换成 `node->name()` 仍绿）
class TwoDanglingMid : public Component {
public:
    CH_HIERARCHY_LAYER("two_dangling_mid");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_module<DanglingLeaf> leaf{"leaf"};
    TwoDanglingMid(Component *p = nullptr, const std::string &n = "mid")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        // ⚠️ `leaf.io().din` **不驱动** ⇒ ⇒ 叶子层悬空 ⇒ ⇒ 跨 module 边界冒泡
        io().dout <<= leaf.io().dout;
    }
};

class TwoDanglingTop : public Component {
public:
    CH_HIERARCHY_LAYER("two_dangling_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_module<TwoDanglingMid> m1{"m1"};
    ch_module<TwoDanglingMid> m2{"m2"};
    TwoDanglingTop(Component *p = nullptr, const std::string &n = "top")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        m1.io().din <<= io().din;
        m2.io().din <<= io().din;
        io().dout <<= m1.io().dout;
    }
};

// ── DUT ⑦：**第三轴**（祖父层驱动孙层 reg）⇒ ⇒ 非相邻隐式边 ──────────────
//    ⚠️ 这类边的 `child_instance_name` **恒空** ⇒ ⇒ 发射器走「第三轴链」分支
//    ⇒ ⇒ **不**合成 `__if_` / `__x_` 端口 ⇒ ⇒ 诊断 MUST NOT 计入端口数
//    （★ critic BLOCKER 1 的判别点：修复前诊断计 1 / 发射器计 0 ⇒ T4.1 转红）
class ThirdAxisLeaf : public Component {
public:
    CH_HIERARCHY_LAYER("third_axis_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> acc{0_d};
    ThirdAxisLeaf(Component *p = nullptr, const std::string &n = "leaf")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        acc->next = io().din;
        io().dout <<= acc;
    }
};

class ThirdAxisMid : public Component {
public:
    CH_HIERARCHY_LAYER("third_axis_mid");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_module<ThirdAxisLeaf> leaf{"leaf"};
    ThirdAxisMid(Component *p = nullptr, const std::string &n = "mid")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().din;
        io().dout <<= leaf.io().dout;
    }
};

class ThirdAxisTop : public Component {
public:
    CH_HIERARCHY_LAYER("third_axis_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_module<ThirdAxisMid> mid{"mid"};
    ThirdAxisTop(Component *p = nullptr, const std::string &n = "top")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mid.io().din <<= io().din;
        // ⚠️⚠️ **祖父层**（top）直接改**孙层**（leaf）的 reg ⇒ ⇒ 跨 module 边
        //    top → leaf ⇒ ⇒ **非相邻**（top 的直接子是 mid）⇒ ⇒ pending 边
        //    ⇒ ⇒ `child_instance_name` 恒空 ⇒ ⇒ 发射器走「第三轴链」
        mid.instance().leaf.instance().acc->next = io().din;
        io().dout <<= mid.io().dout;
    }
};

// ── DUT ⑧：**向上 pass-through**（顶层读孙层的 reg）⇒ ⇒ `__up_` 链 ────────
//    ⚠️ 这类边**也**是 `child_instance_name` 恒空 ⇒ ⇒ 发射器走「向上链」⇒ ⇒
//    产的 `__up_` 端口按 delta 明确**排除** ⇒ ⇒ 诊断 MUST NOT 计入端口数
class UpwardTop : public Component {
public:
    CH_HIERARCHY_LAYER("upward_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_module<ThirdAxisMid> mid{"mid"};
    UpwardTop(Component *p = nullptr, const std::string &n = "top")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mid.io().din <<= io().din;
        // ⚠️ 顶层读**孙层**的内部寄存器 ⇒ ⇒ 向上 pass-through（非直接父子）
        io().dout <<= mid.instance().leaf.instance().acc;
    }
};

// ── DUT ⑥：跨模块 `ch_mem` ⇒ ⇒ 分析层**永远抛** ⇒ ⇒ 不可分析 ────────────
class MemChild : public Component {
public:
    CH_HIERARCHY_LAYER("mem_child");
    __io(ch_in<ch_uint<4>> addr; ch_out<ch_uint<8>> rdata;)
    ch_mem<ch_uint<8>, 16> mem{"mem"};
    MemChild(Component *p = nullptr, const std::string &n = "child")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().rdata <<= mem.aread(io().addr); }
};

class MemParent : public Component {
public:
    CH_HIERARCHY_LAYER("mem_parent");
    __io(ch_in<ch_uint<4>> addr; ch_out<ch_uint<8>> rdata;)
    ch_module<MemChild> child{"child"};
    MemParent(Component *p = nullptr, const std::string &n = "top")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        child.io().addr <<= io().addr;
        io().rdata <<= child.instance().mem.aread(io().addr);   // ⚠️ 跨模块 mem
    }
};

/// 节点集合快照（★ 「只读」判据：MUST 抓「改写既有节点的 src」，而不只是
/// 「新建节点」⇒ ⇒ 只比 `get_eval_list().size()` 抓不到）
std::string node_snapshot(context *ctx) {
    std::string out;
    for (const auto &owned : ctx->get_nodes()) {
        const lnodeimpl *n = owned.get();
        out += n->name();
        out += "#";
        out += std::to_string(n->num_srcs());
        out += ":";
        for (uint32_t i = 0; i < n->num_srcs(); ++i) {
            out += n->src(i) ? n->src(i)->name() : "<null>";
            out += ",";
        }
        out += ";";
    }
    return out;
}

std::size_t count_details(const HierarchyDiagnosis &d, const std::string &category) {
    std::size_t n = 0;
    for (const auto &det : d.details) {
        if (det.category == category) {
            ++n;
        }
    }
    return n;
}

}   // namespace

// ===========================================================================
// ① 隐式边 vs 合成端口：**两个不同的量**
// ===========================================================================

TEST_CASE("诊断：隐式**边**数与合成**端口**数 MUST 是两个不同的量",
          "[hierarchy][diagnose][counts]") {
    ch_device<TwoTargetTop> dev;
    const auto d = diagnose_hierarchy(dev.instance());

    REQUIRE(d.verdict == HierarchyFriendliness::kNeedsWork);
    REQUIRE(d.implicit_edge_count.has_value());
    REQUIRE(d.synthesized_port_count.has_value());
    // ⚠️ 一个子内节点驱动**两个**父侧端点 ⇒ ⇒ 两条边
    REQUIRE(*d.implicit_edge_count == 2u);
    // ⚠️ 但合成端口按 (子 module, 子侧节点) **去重** ⇒ ⇒ 只合成 1 个
    REQUIRE(*d.synthesized_port_count == 1u);
    // ⚠️ ⇒ 这正是「边数 ≠ 端口数」⇒ ⇒ 「隐式端口数 == edges.size()」是错的
}

TEST_CASE("诊断：合成端口数 MUST 与发射产物里的隐式合成端口数**相等**",
          "[hierarchy][diagnose][parity]") {
    ch_device<XTop> dev;
    const auto d = diagnose_hierarchy(dev.instance());
    REQUIRE(d.synthesized_port_count.has_value());
    REQUIRE(*d.synthesized_port_count >= 1u);

    // ⇒ 发射 ⇒ 数产物里 `origin == Synthesized` 且属**隐式合成**的端口
    const std::string path = "hier_diagnose_parity.v";
    std::remove(path.c_str());
    const auto r = toVerilogHierarchical(path, dev.instance());
    REQUIRE(r.ok);

    std::size_t emitted = 0;
    for (const auto &pp : r.port_provenance) {
        // ⚠️ 按**端口名前缀**分别计数 ⇒ ⇒ 排除向上 pass-through（`__up_`）
        //    （★ 它的数量 = 链长 ⇒ 不可由边数推导 ⇒ 不在本项口径内）
        const bool is_implicit_synth =
            pp.origin == PortProvenanceOrigin::Synthesized &&
            (pp.port.rfind("__x_", 0) == 0 || pp.port.rfind("__if_", 0) == 0);
        if (is_implicit_synth) {
            ++emitted;
        }
    }
    REQUIRE(emitted == *d.synthesized_port_count);
}

TEST_CASE("诊断：合成端口数 MUST NOT 计入**非相邻**隐式边（第三轴 / 向上链）",
          "[hierarchy][diagnose][parity][non-adjacent]") {
    // ⚠️ **critic BLOCKER 1 的判别点**：非相邻边（`child_instance_name` 恒空）
    //    ⇒ 发射器走「第三轴链」/「向上链」⇒ ⇒ **不**合成 `__if_` / `__x_` 端口
    //    ⇒ ⇒ 诊断若把它们计入 ⇒ 与发射器**不等**⇒ ⇒ 本用例 MUST 转红。
    //    （★ 修复前：诊断计 1 / 发射器计 0 ⇒ 转红；修复后：两侧都 0 ⇒ 绿）

    // ① 第三轴（祖父层驱动孙层 reg）
    {
        ch_device<ThirdAxisTop> dev;
        const auto d = diagnose_hierarchy(dev.instance());
        REQUIRE(d.implicit_edge_count.has_value());
        REQUIRE(*d.implicit_edge_count >= 1u);          // ★ 确有非相邻隐式边
        REQUIRE(d.synthesized_port_count.has_value());
        REQUIRE(*d.synthesized_port_count == 0u);        // ★ 但**不**合成端口

        const std::string path = "hier_diagnose_third_axis.v";
        std::remove(path.c_str());
        const auto r = toVerilogHierarchical(path, dev.instance());
        REQUIRE(r.ok);
        std::size_t emitted = 0;
        for (const auto &pp : r.port_provenance) {
            const bool is_implicit_synth =
                pp.origin == PortProvenanceOrigin::Synthesized &&
                (pp.port.rfind("__x_", 0) == 0 || pp.port.rfind("__if_", 0) == 0);
            if (is_implicit_synth) {
                ++emitted;
            }
        }
        REQUIRE(emitted == *d.synthesized_port_count);  // ★ 两侧相等
    }

    // ② 向上 pass-through（顶层读孙层 reg ⇒ `__up_` 链 ⇒ 按 delta **排除**）
    {
        ch_device<UpwardTop> dev;
        const auto d = diagnose_hierarchy(dev.instance());
        REQUIRE(d.implicit_edge_count.has_value());
        REQUIRE(*d.implicit_edge_count >= 1u);
        REQUIRE(d.synthesized_port_count.has_value());
        REQUIRE(*d.synthesized_port_count == 0u);        // ★ `__up_` 不计入
    }
}

// ===========================================================================
// ② 悬空输入 + 明细的**可区分性**
// ===========================================================================

TEST_CASE("诊断：悬空输入 MUST 被计数，且明细的节点名 MUST 互不相同",
          "[hierarchy][diagnose][dangling]") {
    ch_device<DanglingTop> dev;
    const auto d = diagnose_hierarchy(dev.instance());

    REQUIRE(d.dangling_input_count.has_value());
    REQUIRE(*d.dangling_input_count >= 1u);
    REQUIRE(count_details(d, "dangling_input") == *d.dangling_input_count);

    // ⚠️ **可区分性**：明细的节点名 MUST NOT 全是 `"unnamed_input"`
    //    （⇒ `node->name()` 恒为它 ⇒ ⇒ 用它 ⇒ 两条明细完全相同 ⇒ 不可区分）
    std::vector<std::string> names;
    for (const auto &det : d.details) {
        if (det.category == "dangling_input") {
            names.push_back(det.node);
            REQUIRE(det.node != "unnamed_input");
        }
    }
    // ⚠️ **去重后**至少两个**不同**的名字（★ 冒泡链会让同一节点出现多次 ⇒ ⇒
    //    MUST 先去重 ⇒ ⇒ 否则「互不相同」的比较会误判）
    std::vector<std::string> uniq;
    for (const auto &n : names) {
        bool seen = false;
        for (const auto &u : uniq) {
            if (u == n) { seen = true; break; }
        }
        if (!seen) { uniq.push_back(n); }
    }
    // ⚠️ 本用例的 DUT 只有**一个**悬空输入 ⇒ ⇒ 「互不相同」的严格判据在
    //    「两个不同名」那个用例里（★ 只用**一个**时比较循环**空转** ⇒ ⇒ 恒绿 ⇒ 假绿）
    REQUIRE(uniq.size() >= 1u);
    for (std::size_t i = 0; i < uniq.size(); ++i) {
        for (std::size_t j = i + 1; j < uniq.size(); ++j) {
            REQUIRE(uniq[i] != uniq[j]);
        }
    }
}

TEST_CASE("诊断：**两个**不同名的悬空输入 ⇒ 明细 MUST 互不相同（可区分性）",
          "[hierarchy][diagnose][dangling][distinct]") {
    ch_device<TwoDanglingTop> dev;
    const auto d = diagnose_hierarchy(dev.instance());

    REQUIRE(d.dangling_input_count.has_value());
    REQUIRE(*d.dangling_input_count >= 2u);

    std::vector<std::string> names;
    for (const auto &det : d.details) {
        if (det.category == "dangling_input") {
            names.push_back(det.node);
        }
    }
    std::vector<std::string> uniq;
    for (const auto &n : names) {
        bool seen = false;
        for (const auto &u : uniq) {
            if (u == n) { seen = true; break; }
        }
        if (!seen) { uniq.push_back(n); }
    }
    // ⚠️ **两个不同的悬空输入** ⇒ 名字 MUST 互不相同（★ 这是「明细可区分」的
    //    真正判据 —— 只有一个悬空输入时比较循环空转 ⇒ ⇒ 恒绿 ⇒ ⇒ 假绿）
    REQUIRE(uniq.size() >= 2u);
    for (std::size_t i = 0; i < uniq.size(); ++i) {
        for (std::size_t j = i + 1; j < uniq.size(); ++j) {
            REQUIRE(uniq[i] != uniq[j]);
        }
    }
}

// ===========================================================================
// ③ 未消费子输出（含**多实例**）
// ===========================================================================

TEST_CASE("诊断：父层不消费的子输出 MUST 被计数", "[hierarchy][diagnose][unconsumed]") {
    ch_device<UnconsumedTop> dev;
    const auto d = diagnose_hierarchy(dev.instance());

    REQUIRE(d.unconsumed_child_output_count.has_value());
    REQUIRE(*d.unconsumed_child_output_count >= 1u);
    REQUIRE(count_details(d, "unconsumed_child_output") >=
            *d.unconsumed_child_output_count);
}

TEST_CASE("诊断：多实例中**只有一个**实例的输出被消费 ⇒ MUST 仍算未消费",
          "[hierarchy][diagnose][unconsumed][multi-instance]") {
    ch_device<MultiInstTop> dev;
    const auto d = diagnose_hierarchy(dev.instance());

    // ⚠️ 按**实例**判 ⇒ ⇒ 实例 b 的输出无人消费 ⇒ ⇒ MUST ≥ 1
    //    （★ 若实现按 module 去重 ⇒ 「a 消费了」⇒ 算已消费 ⇒ **转红**）
    REQUIRE(d.unconsumed_child_output_count.has_value());
    REQUIRE(*d.unconsumed_child_output_count >= 1u);
}

// ===========================================================================
// ④ 不可分析：MUST 返回报告（不抛）+ 未测项 MUST NOT 呈现为 0
// ===========================================================================

TEST_CASE("诊断：不可分析时 MUST 返回报告（不抛）且未测项 MUST NOT 是 0",
          "[hierarchy][diagnose][not-analyzable]") {
    ch_device<MemParent> dev;
    HierarchyDiagnosis d;
    REQUIRE_NOTHROW(d = diagnose_hierarchy(dev.instance()));

    REQUIRE(d.verdict == HierarchyFriendliness::kNotAnalyzable);
    // ⚠️ **未测得** ⇒ ⇒ `optional` 无值（★ 若实现给 0 ⇒ 「我没问题」⇒ 假绿）
    REQUIRE_FALSE(d.implicit_edge_count.has_value());
    REQUIRE_FALSE(d.synthesized_port_count.has_value());
    REQUIRE_FALSE(d.dangling_input_count.has_value());
    REQUIRE_FALSE(d.unconsumed_child_output_count.has_value());
    // ⚠️ 「不可分析」的原因 MUST 可见
    REQUIRE(count_details(d, "not_analyzable") == 1u);
}

TEST_CASE("诊断：跨模块 mem MUST 在不可分析时也报出**真实**计数（A1）",
          "[hierarchy][diagnose][not-analyzable][mem]") {
    ch_device<MemParent> dev;
    const auto d = diagnose_hierarchy(dev.instance());

    // ⚠️ A1 口径：不可分析时由**独立预扫描**报出真实计数（★ **不是** `nullopt`
    //    ⇒ ⇒ 「有跨模块 mem」不会被静默表达成「0」）
    REQUIRE(d.cross_module_mem_count.has_value());
    REQUIRE(*d.cross_module_mem_count >= 1u);
    REQUIRE(count_details(d, "cross_module_mem") >= 1u);
}

// ===========================================================================
// ⑤ 只读 + 确定性
// ===========================================================================

TEST_CASE("诊断：MUST 只读（不改图 / 不改 ctx）", "[hierarchy][diagnose][readonly]") {
    ch_device<XTop> dev;
    context *ctx = dev.context();
    REQUIRE(ctx != nullptr);
    const std::string before = node_snapshot(ctx);
    const std::size_t nodes_before = ctx->get_nodes().size();

    const auto d = diagnose_hierarchy(dev.instance());
    (void)d;

    // ⚠️ 「只读」判据 = **驱动关系**快照（★ 只比节点数抓不到「改写 src」）
    REQUIRE(node_snapshot(ctx) == before);
    REQUIRE(ctx->get_nodes().size() == nodes_before);
}

TEST_CASE("诊断：快照 MUST 确定性（两次诊断逐字节相同）",
          "[hierarchy][diagnose][determinism]") {
    ch_device<XTop> dev;
    const auto d1 = diagnose_hierarchy(dev.instance());
    const auto d2 = diagnose_hierarchy(dev.instance());
    REQUIRE(d1.to_text() == d2.to_text());
}

TEST_CASE("诊断：快照 MUST 标出「未测得」（≠ 实测 0）",
          "[hierarchy][diagnose][snapshot]") {
    ch_device<MemParent> dev;
    const auto d = diagnose_hierarchy(dev.instance());
    const std::string text = d.to_text();
    REQUIRE(text.find("<unmeasured>") != std::string::npos);
    REQUIRE(text.find("verdict=kNotAnalyzable") != std::string::npos);
}

// ===========================================================================
// ⑥ 各层规模（三个数**分开给**）
// ===========================================================================

TEST_CASE("诊断：各层规模 MUST 给端口 / 实例 / 体内节点三个数",
          "[hierarchy][diagnose][sizes]") {
    ch_device<XTop> dev;
    const auto d = diagnose_hierarchy(dev.instance());

    REQUIRE(d.module_count >= 2u);
    REQUIRE(d.module_sizes.size() == d.module_count);
    bool found_child = false;
    for (const auto &s : d.module_sizes) {
        // ⚠️ Catch2 的 `REQUIRE` **不支持** `||` ⇒ ⇒ 先算再断言
        // ⚠️ module key = **组件实例路径**（★ 不是 layer 名）⇒ 实测 `top` / `top.child`
        const bool is_known = (s.module == "top") || (s.module == "top.child");
        REQUIRE(is_known);
        if (s.module == "top.child") {
            found_child = true;
            REQUIRE(s.port_count >= 1u);   // 端口
            REQUIRE(s.body_count >= 1u);    // 体内节点（内部寄存器）
            REQUIRE(s.instance_count == 0u);
        }
        if (s.module == "top") {
            REQUIRE(s.instance_count >= 1u);   // 根持有子实例
        }
    }
    REQUIRE(found_child);
}
