// tests/test_hierarchy_equivalence.cpp
//
// T3-005 · **等价性 harness**（ADR-058 §2.2 判据②③④ + capability
// `rtl-hierarchical-output` 的 ADDED Requirement「层次化产物 MUST 与扁平产物等价（可机械判定）」）。
//
// ⚠️ **比较的是两份 Verilog**（扁平 vs 层次化）⇒ ⇒ 不是「C++ 解释器 vs Verilog」
//    ⇒ ⇒ 双后端 parity **抓不到**这一族缺陷（两后端由**同一份 C++** 生成 ⇒
//    同构缺陷会被 parity 盖章 ⇒ 这正是本仓反复出现的失败模式）。
//
// ⚠️ **观测集合 `O` 手写**（不猜）—— ADR-058 §2.2 ② + §4 风险 6：
//    `O` MUST 非空 · MUST 至少含一个**随激励变化**的信号 · MUST ⊆ 两侧顶层输出集。
//
// ⚠️ harness 放在 `tests/`（`tests/hier_equivalence.h`）⇒ **不暴露公共 API**
//    （P8 独立立项；本 change 的 `include/` 只动 `hierarchy_verilog.h`）。

#include <unistd.h>
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "bundle/stream_bundle.h"
#include "codegen_naming.h"
#include "hierarchy_verilog.h"
#include "hier_equivalence.h"
#include "simulator.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ── DUT ①：**全 inline**（叶子**不**声明成层）⇒ ⇒ 层次化产物 MUST 与扁平**逐字节相同**
class EqInlineLeaf : public ch::Component {
public:
    // ⚠️ **不**写 CH_HIERARCHY_LAYER ⇒ 默认 inline ⇒ 全部落进根 module
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> acc{0_d};
    EqInlineLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        acc->next = io().din;
        io().dout <<= acc;
    }
};

class EqInlineTop : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<EqInlineLeaf> leaf{"leaf"};
    EqInlineTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().din;
        io().dout <<= leaf.io().dout;
    }
};

// ── DUT ②：**成层**（叶子声明成层）⇒ ⇒ 顶层端口集 = 扁平的真子集
//    ⇒ ⇒ 父层读**子成员的内部寄存器**（ADR-060 §2.1 **行 2**）⇒ ⇒ 合成**子侧 output**
//    隐式端口 ⇒ ⇒ 判别力自证需要「改一个隐式端口方向」这一形态 ⇒ ⇒ 必须有隐式端口。
class EqLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("eq_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> acc{0_d};
    EqLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        acc->next = io().din;
        io().dout <<= acc;
    }
};

class EqTop : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<EqLeaf> leaf{"leaf"};
    EqTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().din;
        // ⚠️⚠️ 父层读**子成员的内部寄存器** ⇒ ⇒ 行 2 ⇒ ⇒ 子侧 **output** 隐式端口
        io().dout <<= leaf.instance().acc;
    }
};

std::string make_temp_dir(const char *suffix) {
    char tmpl[] = "/tmp/cpphdl_eq_XXXXXX";
    char *dir = ::mkdtemp(tmpl);
    if (!dir) {
        return {};
    }
    std::string base(dir);
    std::string path = base + suffix;
    ::mkdir(path.c_str(), 0755);
    return path;
}

std::string read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

void write_file(const std::string &path, const std::string &text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}

/// 把 `text` 里的 `needle` 换成 `replacement`（找不到 ⇒ 返回 false）。
bool replace_once(std::string &text, const std::string &needle,
                  const std::string &replacement) {
    const auto at = text.find(needle);
    if (at == std::string::npos) {
        return false;
    }
    text.replace(at, needle.size(), replacement);
    return true;
}

/// **一个被观测信号**（带 `node_id` ⇒ ⇒ 可与**映射表**对账 —— F2/F3 的机械载体）。
///
/// ⚠️ `node_id` = 该信号的 ctx 私有编号 ⇒ ⇒ 调用方 MUST 填（`port.impl()->id()`）
///    ⇒ ⇒ 判据才能把「被观测的信号」与**映射表**对起来（delta Requirement ④/⑤：
///    「被观测信号 MUST 出现在映射表里」+「不在观测面内的 `ch_out` MUST 出现在映射表里
///    且 `hier_top_port_name` 为空」⇒ ⇒ 两侧都要机械断言 ⇒ 缺一侧 = 契约条目无载体）。
ch_test::hier::Observation make_obs(const std::string &name, std::uint32_t node_id,
                                     std::function<std::uint64_t(ch::Simulator &)> read) {
    ch_test::hier::Observation o;
    o.name = name;
    o.node_id = node_id;
    o.read = std::move(read);
    return o;
}

/// DUT 的**全部 `ch_out`**（`node_id` + 名字）⇒ ⇒ 用来算「不在观测面内」的那部分。
using ChOutList = std::vector<std::pair<std::uint32_t, std::string>>;

/// **观测面模式**（delta Requirement ④/⑤ · 2026-11-06 口径）
///
/// ⚠️ **为什么有两种模式**：「`O` MUST ⊇ 全部 `ch_out`」按字面**不可能成立** ——
///    成层子树的 `ch_out` 在层次化顶层**不可观测**（ADR-058 §4 风险 6 的盲区）
///    ⇒ ⇒ 于是 `O` 的定义必须**按比较场景**分两种：
///    · **单粒度比较** ⇒ `O` = 该粒度下**全部可见**的 `ch_out`（最强的覆盖纪律）
///    · **跨粒度比较** ⇒ `O` = **两粒度都可见**的公共子集（否则读不到的一侧会被
///      误判成分歧）⇒ ⇒ 「可见但未观测」者 MUST **显式豁免**（机械载体）
enum class ObsMode {
    /// **单粒度全覆盖**：`O` MUST = 该粒度下**全部可见**的 `ch_out`
    /// ⇒ ⇒ 未观测者 MUST 全部**不可见**（= 真盲区）⇒ ⇒ 盲区侧断言成立
    FullCoverage,
    /// **跨粒度公共子集**：`O` MUST ⊆ **该粒度与基准粒度都可见**的 `ch_out`
    /// ⇒ ⇒ 「可见但未观测」者 MUST 进 `exempt`（**机械载体** · MUST NOT 仅注释）
    PublicSubset,
};

/// 一次等价性运行的**观测面计划**（`O` **手写** · delta Requirement ④）。
struct ObsPlan {
    /// `O`（**手写**声明 ⇒ MUST NOT 由产物反推 ⇒ 否则「实际集 − 实际集」的循环定义）
    std::vector<ch_test::hier::Observation> obs;
    /// DUT 的**全部** `ch_out`（盲区侧的计算基准）
    ChOutList all_ch_outs;
    /// DUT 的**顶层 `ch_in`**（独立锚排除 input 用 ⇒ ⇒ 与 `all_ch_outs` **分别手写**
    /// ⇒ ⇒ 两者**不同源** ⇒ ⇒ 「`O` 与 out 清单同时漏项」会被独立锚抓住）
    ChOutList top_ins;
    /// 模式（见 `ObsMode`）
    ObsMode mode = ObsMode::FullCoverage;
    /// 「该粒度下可见、但本次未观测」的**显式豁免**（仅 `PublicSubset` 模式非空）
    ChOutList exempt;
};

/// **观测面 ↔ 映射表**的对账（delta Requirement ④/⑤ · 2026-11-06 口径）
///
/// 六段断言：
/// ① **被观测侧**：每个被观测信号 ⇒ `node_id` ∈ 映射表 ∧ `hier_top_port_name` **非空**
///    （非空 = 它在层次化顶层 ⇒ ⇒ 可被观测 ✓；为空 = 选了读不到的信号 ⇒ 必红）
/// ② `O` ⊆ DUT 的**全部** `ch_out`（防手写 `O` 时写进不存在的信号）
/// ③ **豁免侧**：每个豁免项 ⇒ ∈ 映射表 ∧ `hier_top_port_name` **非空**
///    （= 它**确实可见**，只是本次没选进 `O` ⇒ ⇒ 豁免清单不是「垃圾桶」）
/// ④ **盲区侧**：未观测 ∧ 未豁免 ⇒ ∈ 映射表 ∧ `hier_top_port_name` **为空**
///    （= 它**不在**层次化顶层 ⇒ ⇒ 顶层观测不到 ⇒ ⇒ 它就是被登记的盲区 ✓）
/// ⑤ **独立锚**（F12）：从**映射表**（emitter 产物）导出「双顶层可见」的 output 候选
///    ⇒ ⇒ 这一步**不依赖**手写的 `all_ch_outs` ⇒ ⇒ 打破「`O` 与手写清单同时漏项 ⇒ 恒绿」
/// ⑥ **顶层端口计数**交叉校验（独立锚的第二道）：扁平侧顶层端口数
///    == 顶层 `ch_in` 数 + `ch_out` 数 + **2**（根 module 无条件发射 clock + reset ·
///    ADR-058 §5）⇒ ⇒ 抓「out 清单漏项」的**计数级**后果
void check_observation_surface(const ObsPlan &plan,
                               const std::vector<ch::SignalMapEntry> &map) {
    std::map<std::uint32_t, const ch::SignalMapEntry *> by_id;
    for (const auto &e : map) {
        by_id[e.node_id] = &e;
    }
    REQUIRE_FALSE(by_id.empty());

    std::set<std::uint32_t> observed;
    std::set<std::uint32_t> exempt_ids;
    std::set<std::uint32_t> top_in_ids;
    for (const auto &o : plan.obs) {
        observed.insert(o.node_id);
    }
    for (const auto &kv : plan.exempt) {
        exempt_ids.insert(kv.first);
    }
    for (const auto &kv : plan.top_ins) {
        top_in_ids.insert(kv.first);
    }

    // ① 被观测侧
    for (const auto &o : plan.obs) {
        INFO("observed signal: " << o.name << " (node " << o.node_id << ")");
        REQUIRE(by_id.count(o.node_id) == 1);
        REQUIRE_FALSE(by_id[o.node_id]->hier_top_port_name.empty());
    }
    // ② `O` ⊆ DUT 的全部 `ch_out`
    for (const auto &o : plan.obs) {
        bool found = false;
        for (const auto &kv : plan.all_ch_outs) {
            if (kv.first == o.node_id) {
                found = true;
            }
        }
        INFO("observed signal: " << o.name);
        REQUIRE(found);
    }
    // ③ 豁免侧：豁免项 MUST **确实可见**
    for (const auto &kv : plan.exempt) {
        INFO("exempt signal: " << kv.second << " (node " << kv.first << ")");
        REQUIRE(by_id.count(kv.first) == 1);
        REQUIRE_FALSE(by_id[kv.first]->hier_top_port_name.empty());
    }
    // ④ 盲区侧：未观测 ∧ 未豁免 ⇒ MUST 不可见
    for (const auto &kv : plan.all_ch_outs) {
        if (observed.count(kv.first) != 0) {
            continue;
        }
        if (exempt_ids.count(kv.first) != 0) {
            continue;   // 已**显式**豁免 ⇒ 不是「静默漏测」
        }
        INFO("blind-spot signal: " << kv.second << " (node " << kv.first << ")");
        REQUIRE(by_id.count(kv.first) == 1);
        REQUIRE(by_id[kv.first]->hier_top_port_name.empty());
    }
    // ⑤ **独立锚**：从**映射表**导出「双顶层可见」的 output 候选 ⇒ MUST 全被 `O` ∪ `exempt` 覆盖
    //    ⚠️ 排除框架 clock/reset（它们也是顶层端口，但不是 `ch_out`）与顶层 `ch_in`
    for (const auto &e : map) {
        if (!e.flat.is_port) {
            continue;   // 扁平侧不是顶层端口 ⇒ 非候选
        }
        if (e.hier_top_port_name.empty()) {
            continue;   // 层次化侧不可见 ⇒ 是盲区（④ 已管）
        }
        if (e.hier_top_port_name == "default_clock" ||
            e.hier_top_port_name == "default_reset") {
            continue;   // 框架时序基础设施（根 module 无条件发射 · ADR-058 §5）
        }
        if (top_in_ids.count(e.node_id) != 0) {
            continue;   // 顶层 `ch_in`（不是 `ch_out`）
        }
        INFO("visible output not covered: " << e.signal << " (node " << e.node_id << ")");
        // ⇒ 「被观测」或「显式豁免」⇒ ⇒ MUST 至少满足其一
        //    ⚠️ Catch2 的 `REQUIRE` MUST NOT 含 `||`（表达式分解器不支持）⇒ ⇒ 拆成两条
        if (observed.count(e.node_id) != 1) {
            REQUIRE(exempt_ids.count(e.node_id) == 1);
        }
    }
    // ⑥ `FullCoverage` 模式下 MUST NOT 有豁免（豁免只对公共子集模式有意义）
    //    ⚠️ **刻意不做**「顶层端口计数」交叉校验：扁平侧 = 单 module ⇒ 顶层端口 =
    //    **所有** `ch_in` + **所有** `ch_out` + clock/reset ⇒ 只列「顶层 in」的清单
    //    算不出 ⇒ 会假红 ⇒ ⇒ 独立锚由 ⑤ 承担（它逐项点名，不依赖计数）
    if (plan.mode == ObsMode::FullCoverage) {
        REQUIRE(plan.exempt.empty());
    }
}

/// **`base` 的语义**（delta Requirement ④/⑤ · 2026-11-06 口径）
///
/// · **缺省（`nullopt`）= 单粒度比较** ⇒ `base := g` ⇒ `mode = FullCoverage`
///   ⇒ ⇒ `O` MUST = 该粒度下**全部可见**的 `ch_out`（「可见但未观测」在此模式下**不合法**）
/// · **显式传入 = 跨粒度比较** ⇒ `mode = PublicSubset` ⇒ `O` MUST ⊆ 公共子集，
///   「可见但未观测」者 MUST 进 `exempt`（机械载体）
///
/// ⚠️ 缺省**必须**是单粒度：C++ 的默认实参写不出 `base = g` ⇒ 若沿用某个固定粒度当缺省，
///    「单粒度 + 粒度 ≠ 该缺省值」的调用点会被**静默**当成跨粒度 ⇒ ⇒ 可见输出被推去
///    `exempt` ⇒ `O` 缩水却不报错（实测：`deep` 的 `ForceInline` 单粒度曾因此只剩 1 个
///    观测量 ⇒ `REQUIRE(inline_obs.size() == 4)` 转红）。
///
/// ⚠️ `g` 与 `base` 是**同一份计划**的两个输入 ⇒ 跨粒度调用点 MUST 显式传 `base`
///    （粒度矩阵 B6-⑤ 即如此），单粒度调用点 MUST 走缺省。
///
/// @param base 跨粒度比较的**基准粒度**；`nullopt` = 单粒度（`base = g`）。
using PlanBase = std::optional<ch::Granularity>;

/// **DUT ①（`EqInlineTop`）** 的观测面计划
///
/// ⚠️ 叶子**不**声明成层 ⇒ ⇒ 三种粒度下**全部** inline ⇒ ⇒ 可见集恒定
///    = {`top.dout`, `leaf.dout`} ⇒ ⇒ 顶层端口 = 扁平端口 ⇒ 产物**逐字节相同**。
ObsPlan plan_for(EqInlineTop &root, ch::Granularity g, PlanBase base = std::nullopt) {
    ObsPlan p;
    p.all_ch_outs = {{root.io().dout.impl()->id(), "top.dout"},
                      {root.leaf.io().dout.impl()->id(), "leaf.dout"}};
    p.top_ins = {{root.io().din.impl()->id(), "top.din"}};
    p.obs.push_back(make_obs("top.dout", root.io().dout.impl()->id(),
                              [&root](ch::Simulator &s) -> std::uint64_t {
                                  return static_cast<std::uint64_t>(
                                      s.get_port_value(root.io().dout));
                              }));
    p.obs.push_back(make_obs("leaf.dout", root.leaf.io().dout.impl()->id(),
                              [&root](ch::Simulator &s) -> std::uint64_t {
                                  return static_cast<std::uint64_t>(
                                      s.get_port_value(root.leaf.io().dout));
                              }));
    return p;
}

/// **DUT ②（`EqTop`）** 的观测面计划（叶子声明成层 ⇒ `leaf.dout` 的可见性随粒度变）
///
/// ⚠️ **可见**= 该信号在**层次化顶层端口表**里 ⇔ 映射表 `hier_top_port_name` 非空
///    ⇒ ⇒ 叶子成层 ⇒ 它的 `ch_out` **不在**顶层 ⇒ **不可观测**（盲区）
ObsPlan plan_for(EqTop &root, ch::Granularity g, PlanBase base = std::nullopt) {
    const ch::Granularity b = base.value_or(g);
    ObsPlan p;
    p.all_ch_outs = {{root.io().dout.impl()->id(), "top.dout"},
                      {root.leaf.io().dout.impl()->id(), "leaf.dout"}};
    p.top_ins = {{root.io().din.impl()->id(), "top.din"}};
    p.obs.push_back(make_obs("top.dout", root.io().dout.impl()->id(),
                              [&root](ch::Simulator &s) -> std::uint64_t {
                                  return static_cast<std::uint64_t>(
                                      s.get_port_value(root.io().dout));
                              }));
    // 叶子的 `ch_out` 只在 **ForceInline** 下可见（全 inline ⇒ 落进根 module）
    const bool leaf_visible = (g == ch::Granularity::ForceInline);
    const bool leaf_visible_base = (b == ch::Granularity::ForceInline);
    const bool single = (g == b);
    // `O` = 该粒度与基准粒度**都可见**者（单粒度时基准 = 自身 ⇒ 即「全部可见」）
    if (leaf_visible && (leaf_visible_base || single)) {
        p.obs.push_back(make_obs("leaf.dout", root.leaf.io().dout.impl()->id(),
                                  [&root](ch::Simulator &s) -> std::uint64_t {
                                      return static_cast<std::uint64_t>(
                                          s.get_port_value(root.leaf.io().dout));
                                  }));
    } else if (leaf_visible) {
        // 可见但本次未选入 `O` ⇒ **显式豁免**（机械载体）
        p.exempt.push_back({root.leaf.io().dout.impl()->id(), "leaf.dout"});
    }
    p.mode = single ? ObsMode::FullCoverage : ObsMode::PublicSubset;
    return p;
}

/// 受驱动的顶层输入（**只**顶层 `din`）。
template <typename Dut>
std::vector<ch_test::hier::DrivenInput> make_inputs(Dut &root) {
    std::vector<ch_test::hier::DrivenInput> in;
    in.push_back({"top.din", [&root](ch::Simulator &sim, std::uint64_t v) {
                      // ⚠️ MUST 用 set_input_value（`<<=` 会把端口变成内部驱动 ⇒ 假分歧）
                      sim.set_input_value(root.io().din, v);
                  }});
    return in;
}

}   // namespace

// ═════════════════════════════════════════════════════════════════════════
// ① 默认（全 inline）⇒ 层次化产物与扁平产物**逐字节一致**（`cmp` 退出码 0）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical equivalence - all-inline product is byte-identical to the flat product",
          "[hierarchy][equivalence][inline]") {
    const std::string dir = make_temp_dir("_inline");
    REQUIRE_FALSE(dir.empty());

    ch::ch_device<EqInlineTop> dev;
    auto products = ch_test::hier::emit(dev.instance(), dir);
    INFO("emit error: " << products.error);
    REQUIRE(products.ok);
    REQUIRE_FALSE(products.ports.empty());

    const auto plan = plan_for(dev.instance(), ch::Granularity::RespectDeclarations);
    const auto &obs = plan.obs;
    const auto inputs = make_inputs(dev.instance());
    const std::vector<std::vector<std::uint64_t>> stim = {{0x11}, {0x22}, {0x33}};

    auto res = ch_test::hier::compare(dev.context(), dir, products.hier_path,
                                      products.ports, inputs, obs, stim);
    INFO("compare error: " << res.error);
    REQUIRE(res.ok);
    REQUIRE_FALSE(res.flat_product.empty());
    REQUIRE_FALSE(res.hier_product.empty());

    // ① 逐字节一致（= `cmp` 退出码 0 ⇒ 允许差异 = 空集）
    INFO("flat size=" << res.flat_product.size()
                      << " hier size=" << res.hier_product.size());
    REQUIRE(res.flat_product == res.hier_product);

    // ② **负向对照**：在层次化产物里加一行注释 ⇒ MUST 检出差异
    //    （⇒ 判据不是「恒绿」：一个字节的差别就能被抓到）
    const std::string mutated = res.hier_product + "// injected comment\n";
    REQUIRE(mutated != res.flat_product);

    // ③ 观测面**手写**且 ⊇ 全部 `ch_out`（任务 2.2）：全 inline ⇒ 叶子的 `ch_out`
    //    也在顶层 ⇒ MUST 可观测（两侧都读得到）
    REQUIRE(obs.size() == 2);
    REQUIRE(res.trace.size() == stim.size());
    for (const auto &row : res.trace) {
        REQUIRE(row.size() == obs.size());
    }
    // ④ **观测面 ↔ 映射表**的对账（delta Requirement ④/⑤：MUST 有机械载体）
    check_observation_surface(plan, products.signal_map);
}

// ═════════════════════════════════════════════════════════════════════════
// ② 成层 DUT ⇒ 逐拍等价（观测集合 `O`）+ 「随激励变化」的机械判据
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical equivalence - layered product matches the flat product cycle by cycle",
          "[hierarchy][equivalence][layered]") {
    const std::string dir = make_temp_dir("_layered");
    REQUIRE_FALSE(dir.empty());

    ch::ch_device<EqTop> dev;
    auto products = ch_test::hier::emit(dev.instance(), dir);
    INFO("emit error: " << products.error);
    REQUIRE(products.ok);
    REQUIRE_FALSE(products.ports.empty());

    const auto plan = plan_for(dev.instance(), ch::Granularity::RespectDeclarations);
    const auto &obs = plan.obs;
    const auto inputs = make_inputs(dev.instance());

    // 两组**不同**的激励（多拍序列 ⇒ 寄存器通路被真正走通）
    const std::vector<std::vector<std::uint64_t>> stim_a = {{0x11}, {0x22}, {0x33}};
    const std::vector<std::vector<std::uint64_t>> stim_b = {{0x44}, {0x55}, {0x66}};

    auto res_a = ch_test::hier::compare(dev.context(), dir, products.hier_path,
                                         products.ports, inputs, obs, stim_a);
    INFO("compare(A) error: " << res_a.error);
    REQUIRE(res_a.ok);
    // ① 逐拍全等
    INFO("first mismatch: " << res_a.first_mismatch);
    REQUIRE(res_a.equal);
    REQUIRE(res_a.mismatch_count == 0);
    REQUIRE(res_a.trace.size() == stim_a.size());

    auto res_b = ch_test::hier::compare(dev.context(), dir, products.hier_path,
                                         products.ports, inputs, obs, stim_b);
    INFO("compare(B) error: " << res_b.error);
    REQUIRE(res_b.ok);
    REQUIRE(res_b.equal);

    // ④ **观测面 ↔ 映射表**的对账 + **盲区确实存在**的显式声明
    //    （成层 DUT 的叶子 `ch_out` 在层次化顶层不可观测 ⇒ MUST 登记在映射表且
    //      `hier_top_port_name` 为空 ⇒ MUST NOT 声称「全部输出已比对」）
    check_observation_surface(plan, products.signal_map);
    REQUIRE(plan.all_ch_outs.size() > plan.obs.size());

    // ③ **同一 `node_id` 在两条路径的名字 MUST 逐字相同**（任务 3.4）
    //    ⇒ 按映射表的 `node_id` **逐项**比对（MUST NOT 只比「名字集合交集」）
    //    ⇒ 扁平侧的权威名字表 = ctx 级全局名表（命名权威两侧相同 ⇒ 无需翻译）
    {
        const auto name_table =
            ch::build_verilog_name_table(dev.context()->get_eval_list());
        std::map<std::uint32_t, std::string> name_by_id;
        for (const auto &kv : name_table) {
            name_by_id[kv.first->id()] = kv.second;
        }
        REQUIRE_FALSE(products.signal_map.empty());
        for (const auto &e : products.signal_map) {
            INFO("node " << e.node_id << " signal=" << e.signal);
            REQUIRE(name_by_id.count(e.node_id) == 1);
            REQUIRE(name_by_id[e.node_id] == e.signal);
        }
    }

    // ② 「至少一个随激励变化」的**机械判据**：两组激励下，某观测信号的取值集合不同
    //    （⇒ 否则 `O` 里全是恒 0 信号 ⇒ 「逐拍全等」恒绿 ⇒ 空洞判据）
    bool any_varies = false;
    for (std::size_t o = 0; o < obs.size() && !any_varies; ++o) {
        std::set<std::uint64_t> set_a;
        std::set<std::uint64_t> set_b;
        for (const auto &row : res_a.trace) {
            set_a.insert(row[o]);
        }
        for (const auto &row : res_b.trace) {
            set_b.insert(row[o]);
        }
        if (set_a != set_b) {
            any_varies = true;
        }
    }
    REQUIRE(any_varies);
}

// ═════════════════════════════════════════════════════════════════════════
// ③ 判别力自证：故意破坏层次化产物 ⇒ 等价性判据 MUST 转红 ⇒ 还原复绿
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical equivalence - corrupting the layered product turns the judgement red",
          "[hierarchy][equivalence][mutation]") {
    const std::string dir = make_temp_dir("_mut");
    REQUIRE_FALSE(dir.empty());

    ch::ch_device<EqTop> dev;
    auto products = ch_test::hier::emit(dev.instance(), dir);
    INFO("emit error: " << products.error);
    REQUIRE(products.ok);

    const auto plan = plan_for(dev.instance(), ch::Granularity::RespectDeclarations);
    const auto &obs = plan.obs;
    const auto inputs = make_inputs(dev.instance());
    const std::vector<std::vector<std::uint64_t>> stim = {{0x11}, {0x22}, {0x33}};

    // ── 基线：未篡改 ⇒ 全等
    auto base = ch_test::hier::compare(dev.context(), dir, products.hier_path,
                                       products.ports, inputs, obs, stim);
    INFO("base error: " << base.error);
    REQUIRE(base.ok);
    REQUIRE(base.equal);

    const std::string original = base.hier_product;
    REQUIRE_FALSE(original.empty());

    // ── 变异 ①：**删一条连线**（实例化语句里的 `.top_leaf_unnamed_input(...)`）
    {
        std::string broken = original;
        const bool patched = replace_once(
            broken, ".top_leaf_unnamed_input(top_unnamed_input),\n", "");
        REQUIRE(patched);   // ⇒ 夹具形态变了 ⇒ 变异就落空了 ⇒ 必须立刻暴露
        const std::string path = dir + "/broken_wire.v";
        write_file(path, broken);
        auto res = ch_test::hier::compare(dev.context(), dir, path, products.ports,
                                          inputs, obs, stim);
        INFO("mut-1 error: " << res.error);
        INFO("mut-1 first mismatch: " << res.first_mismatch);
        // ⇒ MUST 转红（要么值分歧，要么编译/初始化失败 ⇒ **绝不能**「全等」）
        REQUIRE_FALSE((res.ok && res.equal));
    }

    // ── 变异 ②：**改一个隐式端口方向**（子侧 output `__x_0` ⇒ input）
    {
        std::string broken = original;
        const bool patched = replace_once(broken, "output [7:0] __x_0",
                                          "input [7:0] __x_0");
        REQUIRE(patched);
        const std::string path = dir + "/broken_dir.v";
        write_file(path, broken);
        auto res = ch_test::hier::compare(dev.context(), dir, path, products.ports,
                                          inputs, obs, stim);
        INFO("mut-2 error: " << res.error);
        INFO("mut-2 first mismatch: " << res.first_mismatch);
        REQUIRE_FALSE((res.ok && res.equal));
    }

    // ── 变异 ③：**改一条连线的表达式**（连上常量）⇒ ⇒ 产物**仍能编译**
    //    ⇒ ⇒ 失败 MUST 来自**值分歧**（而不是编译期）⇒ 证明「逐拍全等」这条判据
    //    本身有判别力（变异 ①② 的红是**编译期**的 ⇒ 只能证明「harness 会发现异常」）
    {
        std::string broken = original;
        const bool patched = replace_once(
            broken, ".top_leaf_unnamed_input(top_unnamed_input)",
            ".top_leaf_unnamed_input(8'h00)");
        REQUIRE(patched);
        const std::string path = dir + "/broken_value.v";
        write_file(path, broken);
        auto res = ch_test::hier::compare(dev.context(), dir, path, products.ports,
                                          inputs, obs, stim);
        INFO("mut-3 error: " << res.error);
        INFO("mut-3 first mismatch: " << res.first_mismatch);
        // ⇒ MUST 是「运行成功但值分歧」⇒ 证明红来自**值比较**
        REQUIRE(res.ok);
        REQUIRE_FALSE(res.equal);
        REQUIRE(res.mismatch_count > 0);
    }

    // ── 还原 ⇒ 复绿（变异是**判别力自证**，不是「永久破坏」）
    {
        const std::string path = dir + "/restored.v";
        write_file(path, original);
        auto res = ch_test::hier::compare(dev.context(), dir, path, products.ports,
                                          inputs, obs, stim);
        INFO("restored error: " << res.error);
        REQUIRE(res.ok);
        REQUIRE(res.equal);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// B6 · DUT 矩阵（其余 4 个）+ 粒度矩阵
// ⚠️ 粒度裁决见 design.md §6.2：**含 `ch_mem` 的 DUT 与深设计只跑 `RespectDeclarations`**
//    （ForceLayer 会把 mem 持有者成层 ⇒ 跨模块 `ch_mem` 被合法拒绝 ⇒ 该变体无可执行对象）。
// ═════════════════════════════════════════════════════════════════════════

namespace {

// ── DUT ③：**三层 + 兄弟连线**（`a` 的输出驱动兄弟 `b` 的输入）─────────────
class EqDeepLeaf : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> acc{0_d};
    EqDeepLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        acc->next = io().din;
        io().dout <<= acc;
    }
};

class EqDeepMid : public ch::Component {
public:
    // ⚠️⚠️ **成层声明**（2026-11-06 · F11）：此前 `EqDeepTop`/`EqDeepMid`/`EqDeepLeaf`
    //    **均无**声明 ⇒ ⇒ `RespectDeclarations` 下**全 inline** ⇒ ⇒ 该变体与
    //    `ForceInline` **完全等价** ⇒ ⇒ **零增量**（⇒ 等价性判据白跑一格）。
    //    ⇒ ⇒ 声明后 `RespectDeclarations` = 「**中间拓扑**」（top + mid 两个 module，
    //    a/b inline 进 mid）⇒ ⇒ 成为**三粒度矩阵里唯一没人覆盖**的结构。
    CH_HIERARCHY_LAYER("eq_deep_mid");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<EqDeepLeaf> a{"a"};
    ch::ch_module<EqDeepLeaf> b{"b"};
    EqDeepMid(ch::Component *p = nullptr, const std::string &n = "mid")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        a.io().din <<= io().din;
        b.io().din <<= a.io().dout;   // ⚠️ **兄弟边**（a → b）
        io().dout <<= b.io().dout;
    }
};

class EqDeepTop : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<EqDeepMid> mid{"mid"};
    EqDeepTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mid.io().din <<= io().din;
        io().dout <<= mid.io().dout;
    }
};

// ── DUT ④：**bundle 成员跨层**（父层驱动子层的 bundle 字段 ⇒ proxy 子形态）──
//    ⚠️ 字段节点必须先 `as_master()`（否则 `<<=` 落到「另建 assign 节点」的路径）
class EqBundleLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("eq_bundle_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_stream<ch_uint<8>> strm;
    EqBundleLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        strm.as_master();
        strm.payload <<= io().din;
        io().dout <<= strm.payload;
    }
};

class EqBundleTop : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<EqBundleLeaf> leaf{"leaf"};
    EqBundleTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().din;
        // ⚠️⚠️ 父层驱动**子层的 bundle 字段** ⇒ ⇒ 合成隐式端口（proxy 子形态）
        leaf.instance().strm.payload <<= io().din;
        io().dout <<= leaf.io().dout;
    }
};

// ── DUT ⑤：**含 `ch_mem`**（mem 在**成层子 module 内**自用 ⇒ 合法）──────────
//    ⚠️ 父层只读子**端口**（`dout`）⇒ ⇒ **不是**跨模块 mem 读 ⇒ ⇒ 合法
class EqMemLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("eq_mem_leaf");
    __io(ch_in<ch_uint<4>> waddr; ch_in<ch_uint<8>> wdata; ch_out<ch_uint<8>> rdata;)
    ch_mem<ch_uint<8>, 16> mem{"mem"};
    EqMemLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mem.write(io().waddr, io().wdata);        // 每拍写（enable 默认 true）
        io().rdata <<= mem.aread(io().waddr);     // 0-cycle 读 ⇒ 读到**上一拍**写进去的值
    }
};

class EqMemTop : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> wdata; ch_out<ch_uint<8>> rdata;)
    ch::ch_module<EqMemLeaf> leaf{"leaf"};
    EqMemTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().waddr <<= ch_uint<4>(0_d);
        leaf.io().wdata <<= io().wdata;
        io().rdata <<= leaf.io().rdata;
    }
};

// ── DUT ⑥：**真实最深设计**（裁剪版流水线：reg + dmem + **2 个声明成层的流水级**）
//    ⚠️ 顶层端口宽度 **≤ 64**（>64 ⇒ backend 合法拒绝 ⇒ 判据空转）
class EqPipeAlu : public ch::Component {
public:
    CH_HIERARCHY_LAYER("eq_pipe_alu");
    // ⚠️ **两个**输出端口：`y` 喂给兄弟级（memstage）· `state` 回读给根
    //    ⇒ ⇒ `y` **不得**同时被根驱动（否则「同一子端口两个驱动」⇒ 发射器**合法拒绝**）
    __io(ch_in<ch_uint<8>> a; ch_in<ch_uint<8>> b; ch_out<ch_uint<8>> y; ch_out<ch_uint<8>> state;)
    ch_reg<ch_uint<8>> acc{0_d};
    EqPipeAlu(ch::Component *p = nullptr, const std::string &n = "alu")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        acc->next = io().a + io().b;
        io().y <<= acc;
        io().state <<= acc;
    }
};

class EqPipeMem : public ch::Component {
public:
    CH_HIERARCHY_LAYER("eq_pipe_mem");
    __io(ch_in<ch_uint<4>> waddr; ch_in<ch_uint<8>> wdata; ch_out<ch_uint<8>> rdata;)
    ch_mem<ch_uint<8>, 16> mem{"mem"};
    EqPipeMem(ch::Component *p = nullptr, const std::string &n = "memstage")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mem.write(io().waddr, io().wdata);
        io().rdata <<= mem.aread(io().waddr);
    }
};

class EqPipeTop : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> state; ch_out<ch_uint<8>> memval;)
    ch::ch_module<EqPipeAlu> alu{"alu"};
    ch::ch_module<EqPipeMem> memstage{"memstage"};
    EqPipeTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        alu.io().a <<= io().din;
        alu.io().b <<= ch_uint<8>(1_d);
        memstage.io().waddr <<= ch_uint<4>(0_d);
        memstage.io().wdata <<= alu.io().y;   // ⚠️ **兄弟边**（alu → memstage）
        io().state <<= alu.io().state;       // ⚠️ 回读走**另一个**端口（`y` 已被兄弟边占用）
        io().memval <<= memstage.io().rdata;
    }
};

// ── 观测面计划 / 输入集合（**手写** · 每个 DUT 一份）────────────────────────
//
// ⚠️ **口径**（delta Requirement ④/⑤ · 2026-11-06）：`O` MUST ⊆ **该次运行粒度下可见**的
//    `ch_out` 集（可见 ⇔ 层次化顶层端口表里有它 ⇔ 映射表 `hier_top_port_name` 非空）。
//    ⇒ **单粒度**比较 ⇒ `O` = 该粒度下**全部可见**者（盲区 = 不可见者）
//    ⇒ **跨粒度**比较 ⇒ `O` = **两粒度都可见**的公共子集（「可见但未观测」者 MUST 显式豁免）
// ⚠️ `O` 的内容**手写** ⇒ MUST NOT 由产物反推（那是「实际集 − 实际集」的循环定义 ⇒ 恒绿）。

/// **DUT ③（三层 + 兄弟连线 · `EqDeepTop`）** 的观测面计划
///
/// ⚠️ `EqDeepMid` **声明成层**（F11）⇒ ⇒ `RespectDeclarations` = 「中间拓扑」
///    （top + mid 两个 module · a/b inline 进 mid）⇒ ⇒ 它是**三粒度矩阵里唯一没人覆盖**的结构。
/// ⚠️ 可见性：`mid` 成层 ⇒ `mid.dout` **不在**顶层 ⇒ 不可见；`a`/`b` inline 进 `mid`
///    ⇒ 它们的 `ch_out` 是 `mid` 体内的线网 ⇒ 也不在顶层 ⇒ 只有 `ForceInline` 下全可见。
/// ⚠️ **恒同值登记**：`a.dout` 与 `b.dout` 逐拍恒等（兄弟串联 `a → b`）⇒ ⇒ 选进 `O`
///    不增加**增量**判别力（只增加覆盖面）⇒ ⇒ 如实登记，不假装它「独立判别」。
ObsPlan plan_for(EqDeepTop &root, ch::Granularity g, PlanBase base = std::nullopt) {
    const ch::Granularity b = base.value_or(g);
    ObsPlan p;
    p.all_ch_outs = {
        {root.io().dout.impl()->id(), "top.dout"},
        {root.mid.io().dout.impl()->id(), "mid.dout"},
        {root.mid.instance().a.instance().io().dout.impl()->id(), "a.dout"},
        {root.mid.instance().b.instance().io().dout.impl()->id(), "b.dout"},
    };
    p.top_ins = {{root.io().din.impl()->id(), "top.din"}};
    p.obs.push_back(make_obs("top.dout", root.io().dout.impl()->id(),
                              [&root](ch::Simulator &s) -> std::uint64_t {
                                  return static_cast<std::uint64_t>(
                                      s.get_port_value(root.io().dout));
                              }));
    const bool all_inline = (g == ch::Granularity::ForceInline);
    const bool all_inline_base = (b == ch::Granularity::ForceInline);
    const bool single = (g == b);
    if (all_inline && (all_inline_base || single)) {
        p.obs.push_back(make_obs("mid.dout", root.mid.io().dout.impl()->id(),
                                  [&root](ch::Simulator &s) -> std::uint64_t {
                                      return static_cast<std::uint64_t>(
                                          s.get_port_value(root.mid.io().dout));
                                  }));
        // ⚠️⚠️ **兄弟叶的真实输出**（此前这条是 `mid.dout` 的**死重** ⇒ 零增量判别力 ⇒
        //    oracle 在实施后审查里抓出 ⇒ ⇒ MUST 真的观测它们，否则「兄弟边」只被间接覆盖）
        p.obs.push_back(make_obs("a.dout",
                                  root.mid.instance().a.instance().io().dout.impl()->id(),
                                  [&root](ch::Simulator &s) -> std::uint64_t {
                                      return static_cast<std::uint64_t>(
                                          s.get_port_value(
                                              root.mid.instance().a.instance().io().dout));
                                  }));
        p.obs.push_back(make_obs("b.dout",
                                  root.mid.instance().b.instance().io().dout.impl()->id(),
                                  [&root](ch::Simulator &s) -> std::uint64_t {
                                      return static_cast<std::uint64_t>(
                                          s.get_port_value(
                                              root.mid.instance().b.instance().io().dout));
                                  }));
    } else if (all_inline) {
        // ForceInline 下可见、但本次未选入 `O` ⇒ **显式豁免**（机械载体）
        p.exempt.push_back({root.mid.io().dout.impl()->id(), "mid.dout"});
        p.exempt.push_back({root.mid.instance().a.instance().io().dout.impl()->id(), "a.dout"});
        p.exempt.push_back({root.mid.instance().b.instance().io().dout.impl()->id(), "b.dout"});
    }
    p.mode = single ? ObsMode::FullCoverage : ObsMode::PublicSubset;
    return p;
}

std::vector<ch_test::hier::DrivenInput> deep_inputs(EqDeepTop &root) {
    return {{"top.din", [&root](ch::Simulator &s, std::uint64_t v) {
                 s.set_input_value(root.io().din, v);
             }}};
}

/// **DUT ④（bundle 成员跨层 · `EqBundleTop`）** 的观测面计划
ObsPlan plan_for(EqBundleTop &root, ch::Granularity g, PlanBase base = std::nullopt) {
    const ch::Granularity b = base.value_or(g);
    ObsPlan p;
    p.all_ch_outs = {{root.io().dout.impl()->id(), "top.dout"},
                      {root.leaf.io().dout.impl()->id(), "leaf.dout"}};
    p.top_ins = {{root.io().din.impl()->id(), "top.din"}};
    p.obs.push_back(make_obs("top.dout", root.io().dout.impl()->id(),
                              [&root](ch::Simulator &s) -> std::uint64_t {
                                  return static_cast<std::uint64_t>(
                                      s.get_port_value(root.io().dout));
                              }));
    const bool leaf_visible = (g == ch::Granularity::ForceInline);
    const bool leaf_visible_base = (b == ch::Granularity::ForceInline);
    const bool single = (g == b);
    if (leaf_visible && (leaf_visible_base || single)) {
        p.obs.push_back(make_obs("leaf.dout", root.leaf.io().dout.impl()->id(),
                                  [&root](ch::Simulator &s) -> std::uint64_t {
                                      return static_cast<std::uint64_t>(
                                          s.get_port_value(root.leaf.io().dout));
                                  }));
    } else if (leaf_visible) {
        p.exempt.push_back({root.leaf.io().dout.impl()->id(), "leaf.dout"});
    }
    p.mode = single ? ObsMode::FullCoverage : ObsMode::PublicSubset;
    return p;
}

std::vector<ch_test::hier::DrivenInput> bundle_inputs(EqBundleTop &root) {
    return {{"top.din", [&root](ch::Simulator &s, std::uint64_t v) {
                 s.set_input_value(root.io().din, v);
             }}};
}

/// **DUT ⑤（含 `ch_mem` · `EqMemTop`）** 的观测面计划
ObsPlan plan_for(EqMemTop &root, ch::Granularity g, PlanBase base = std::nullopt) {
    const ch::Granularity b = base.value_or(g);
    ObsPlan p;
    p.all_ch_outs = {{root.io().rdata.impl()->id(), "top.rdata"},
                      {root.leaf.io().rdata.impl()->id(), "leaf.rdata"}};
    p.top_ins = {{root.io().wdata.impl()->id(), "top.wdata"}};
    p.obs.push_back(make_obs("top.rdata", root.io().rdata.impl()->id(),
                              [&root](ch::Simulator &s) -> std::uint64_t {
                                  return static_cast<std::uint64_t>(
                                      s.get_port_value(root.io().rdata));
                              }));
    const bool leaf_visible = (g == ch::Granularity::ForceInline);
    const bool leaf_visible_base = (b == ch::Granularity::ForceInline);
    const bool single = (g == b);
    if (leaf_visible && (leaf_visible_base || single)) {
        p.obs.push_back(make_obs("leaf.rdata", root.leaf.io().rdata.impl()->id(),
                                  [&root](ch::Simulator &s) -> std::uint64_t {
                                      return static_cast<std::uint64_t>(
                                          s.get_port_value(root.leaf.io().rdata));
                                  }));
    } else if (leaf_visible) {
        p.exempt.push_back({root.leaf.io().rdata.impl()->id(), "leaf.rdata"});
    }
    p.mode = single ? ObsMode::FullCoverage : ObsMode::PublicSubset;
    return p;
}

std::vector<ch_test::hier::DrivenInput> mem_inputs(EqMemTop &root) {
    return {{"top.wdata", [&root](ch::Simulator &s, std::uint64_t v) {
                 s.set_input_value(root.io().wdata, v);
             }}};
}

/// **DUT ⑥（真实最深设计 · `EqPipeTop`）** 的观测面计划
///
/// ⚠️ `alu` / `memstage` **都声明成层** ⇒ ⇒ 它们的 `ch_out` 在层次化顶层**不可见**
///    ⇒ ⇒ 只有 `ForceInline` 下全部可见（5 个 `ch_out`）。
/// ⚠️ **恒同值登记**：`alu.y` 与 `alu.state` 都 `<= acc` ⇒ 逐拍恒等；`top.state` 亦然
///    ⇒ ⇒ 它们增加**覆盖面**而不增加**增量**判别力 ⇒ ⇒ 如实登记。
ObsPlan plan_for(EqPipeTop &root, ch::Granularity g, PlanBase base = std::nullopt) {
    const ch::Granularity b = base.value_or(g);
    ObsPlan p;
    p.all_ch_outs = {
        {root.io().state.impl()->id(), "top.state"},
        {root.io().memval.impl()->id(), "top.memval"},
        {root.alu.io().y.impl()->id(), "alu.y"},
        {root.alu.io().state.impl()->id(), "alu.state"},
        {root.memstage.io().rdata.impl()->id(), "memstage.rdata"},
    };
    p.top_ins = {{root.io().din.impl()->id(), "top.din"}};
    p.obs.push_back(make_obs("top.state", root.io().state.impl()->id(),
                              [&root](ch::Simulator &s) -> std::uint64_t {
                                  return static_cast<std::uint64_t>(
                                      s.get_port_value(root.io().state));
                              }));
    p.obs.push_back(make_obs("top.memval", root.io().memval.impl()->id(),
                              [&root](ch::Simulator &s) -> std::uint64_t {
                                  return static_cast<std::uint64_t>(
                                      s.get_port_value(root.io().memval));
                              }));
    const bool all_inline = (g == ch::Granularity::ForceInline);
    const bool all_inline_base = (b == ch::Granularity::ForceInline);
    const bool single = (g == b);
    if (all_inline && (all_inline_base || single)) {
        p.obs.push_back(make_obs("alu.y", root.alu.io().y.impl()->id(),
                                  [&root](ch::Simulator &s) -> std::uint64_t {
                                      return static_cast<std::uint64_t>(
                                          s.get_port_value(root.alu.io().y));
                                  }));
        p.obs.push_back(make_obs("alu.state", root.alu.io().state.impl()->id(),
                                  [&root](ch::Simulator &s) -> std::uint64_t {
                                      return static_cast<std::uint64_t>(
                                          s.get_port_value(root.alu.io().state));
                                  }));
        p.obs.push_back(make_obs("memstage.rdata", root.memstage.io().rdata.impl()->id(),
                                  [&root](ch::Simulator &s) -> std::uint64_t {
                                      return static_cast<std::uint64_t>(
                                          s.get_port_value(root.memstage.io().rdata));
                                  }));
    } else if (all_inline) {
        p.exempt.push_back({root.alu.io().y.impl()->id(), "alu.y"});
        p.exempt.push_back({root.alu.io().state.impl()->id(), "alu.state"});
        p.exempt.push_back({root.memstage.io().rdata.impl()->id(), "memstage.rdata"});
    }
    p.mode = single ? ObsMode::FullCoverage : ObsMode::PublicSubset;
    return p;
}

std::vector<ch_test::hier::DrivenInput> pipe_inputs(EqPipeTop &root) {
    return {{"top.din", [&root](ch::Simulator &s, std::uint64_t v) {
                 s.set_input_value(root.io().din, v);
             }}};
}

/// 跑一次「发射 + 逐拍比对」并断言**全等**（失败时把 mismatch 打出来）。
void require_equivalent(ch::Component &root, ch::core::context *ctx, const std::string &dir,
                        ch::Granularity granularity,
                        const std::vector<ch_test::hier::DrivenInput> &inputs,
                        const std::vector<std::vector<std::uint64_t>> &stim,
                        const ObsPlan &plan) {
    auto products = ch_test::hier::emit(root, dir, granularity);
    INFO("emit error: " << products.error);
    REQUIRE(products.ok);
    REQUIRE_FALSE(products.ports.empty());

    // **观测面 ↔ 映射表**的对账（delta Requirement ④/⑤ ⇒ MUST 有机械载体）
    check_observation_surface(plan, products.signal_map);

    auto res = ch_test::hier::compare(ctx, dir, products.hier_path, products.ports, inputs,
                                      plan.obs, stim);
    INFO("compare error: " << res.error);
    INFO("first mismatch: " << res.first_mismatch);
    REQUIRE(res.ok);
    REQUIRE(res.equal);
    REQUIRE(res.mismatch_count == 0);
}

}   // namespace

// ═════════════════════════════════════════════════════════════════════════
// B6-① 三层 + 兄弟连线
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical equivalence - three levels with a sibling edge",
          "[hierarchy][equivalence][deep]") {
    const std::string dir = make_temp_dir("_deep");
    REQUIRE_FALSE(dir.empty());
    ch::ch_device<EqDeepTop> dev;

    const std::vector<std::vector<std::uint64_t>> stim = {{0x11}, {0x22}, {0x33}, {0x44}};

    // ① **中间拓扑**（`RespectDeclarations`：`mid` **声明成层**（F11）⇒ top + mid 两个
    //    module · a/b inline 进 `mid`）⇒ ⇒ 这是**三粒度矩阵里唯一没人覆盖**的结构
    //    ⇒ ⇒ 观测面 = 该粒度下**全部可见**的 `ch_out` = {`top.dout`}
    //    （`mid.dout` / `a.dout` / `b.dout` 在层次化顶层**不可见** ⇒ 盲区）
    require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::RespectDeclarations,
                       deep_inputs(dev.instance()), stim,
                       plan_for(dev.instance(), ch::Granularity::RespectDeclarations));

    // ② **ForceInline** ⇒ 全部 inline ⇒ 顶层端口 = 扁平端口 ⇒ 观测面可含**全部** `ch_out`
    //    ⇒ ⇒ 顺带验「全 inline ⇒ 与扁平**逐字节相同**」
    {
        auto products = ch_test::hier::emit(dev.instance(), dir, ch::Granularity::ForceInline);
        INFO("emit error: " << products.error);
        REQUIRE(products.ok);
        const auto inline_plan = plan_for(dev.instance(), ch::Granularity::ForceInline);
        const auto &inline_obs = inline_plan.obs;
        // ⚠️ **观测面必须真的含兄弟叶的输出**（此前是 `mid.dout` 的死重 ⇒ 零判别力 ⇒
        //    oracle 在实施后审查里抓出 ⇒ ⇒ 这里钉住条数，防再 slip）
        REQUIRE(inline_obs.size() == 4);
        auto res = ch_test::hier::compare(dev.context(), dir, products.hier_path, products.ports,
                                          deep_inputs(dev.instance()), inline_obs, stim);
        INFO("compare error: " << res.error);
        INFO("first mismatch: " << res.first_mismatch);
        REQUIRE(res.ok);
        REQUIRE(res.equal);
        REQUIRE(res.flat_product == res.hier_product);
        check_observation_surface(inline_plan, products.signal_map);
    }

    // ③ **ForceLayer** ⇒ 每个组件成层 ⇒ 三层 module 树 ⇒ 兄弟边成为**跨 module** 连接
    require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::ForceLayer,
                       deep_inputs(dev.instance()), stim,
                       plan_for(dev.instance(), ch::Granularity::ForceLayer));
}

// ═════════════════════════════════════════════════════════════════════════
// B6-② bundle 成员跨层（proxy 子形态 ⇒ 合成隐式端口）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical equivalence - a bundle field crossing a layer boundary",
          "[hierarchy][equivalence][bundle]") {
    const std::string dir = make_temp_dir("_bundle");
    REQUIRE_FALSE(dir.empty());
    ch::ch_device<EqBundleTop> dev;

    const std::vector<std::vector<std::uint64_t>> stim = {{0x0A}, {0x1B}, {0x2C}};

    // ① **成层**（叶声明成层 ⇒ 父层驱动其 bundle 字段 ⇒ 隐式端口）
    require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::RespectDeclarations,
                       bundle_inputs(dev.instance()), stim,
                       plan_for(dev.instance(), ch::Granularity::RespectDeclarations));

    // ② **ForceInline** ⇒ 全部 inline ⇒ 观测面含**全部** `ch_out`
    require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::ForceInline,
                       bundle_inputs(dev.instance()), stim,
                       plan_for(dev.instance(), ch::Granularity::ForceInline));

    // ③ **ForceLayer** ⇒ 同样成层 ⇒ 隐式端口路径不变
    require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::ForceLayer,
                       bundle_inputs(dev.instance()), stim,
                       plan_for(dev.instance(), ch::Granularity::ForceLayer));
}

// ═════════════════════════════════════════════════════════════════════════
// B6-③ 含 `ch_mem`（mem 在**成层子 module 内**自用 ⇒ 合法）
// ⚠️ 只跑 `RespectDeclarations`（design §6.2 的裁决）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical equivalence - a ch_mem living inside a layered module",
          "[hierarchy][equivalence][mem]") {
    const std::string dir = make_temp_dir("_mem");
    REQUIRE_FALSE(dir.empty());
    ch::ch_device<EqMemTop> dev;

    // ⚠️ 观测值 = **上一拍**写进 mem 的值（0-cycle 读）⇒ ⇒ 激励逐拍变化 ⇒ 观测值逐拍变化
    const std::vector<std::vector<std::uint64_t>> stim = {{0x11}, {0x22}, {0x33}, {0x44}};

    require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::RespectDeclarations,
                       mem_inputs(dev.instance()), stim,
                       plan_for(dev.instance(), ch::Granularity::RespectDeclarations));

    // 「随激励变化」：观测集合的取值集合 MUST 随激励变化（mem 通路真的走了）
    {
        auto products = ch_test::hier::emit(dev.instance(), dir,
                                            ch::Granularity::RespectDeclarations);
        REQUIRE(products.ok);
        auto res = ch_test::hier::compare(dev.context(), dir, products.hier_path, products.ports,
                                          mem_inputs(dev.instance()),
                                          plan_for(dev.instance(),
                                                   ch::Granularity::RespectDeclarations).obs,
                                          stim);
        REQUIRE(res.ok);
        std::set<std::uint64_t> values;
        for (const auto &row : res.trace) {
            values.insert(row[0]);
        }
        INFO("distinct observed values = " << values.size());
        REQUIRE(values.size() >= 2);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// B6-④ **真实最深设计**（裁剪版流水线：reg + dmem + 2 个**声明成层**的流水级）
// ⚠️ 只跑 `RespectDeclarations`（design §6.2 的裁决）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical equivalence - the deepest design (two layered pipeline stages)",
          "[hierarchy][equivalence][deepcore]") {
    const std::string dir = make_temp_dir("_deepcore");
    REQUIRE_FALSE(dir.empty());
    ch::ch_device<EqPipeTop> dev;

    const std::vector<std::vector<std::uint64_t>> stim = {{0x05}, {0x06}, {0x07}, {0x08}};

    require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::RespectDeclarations,
                       pipe_inputs(dev.instance()), stim,
                       plan_for(dev.instance(), ch::Granularity::RespectDeclarations));

    // 「随激励变化」：两个观测量都 MUST 变（状态 + mem 回读）
    {
        auto products = ch_test::hier::emit(dev.instance(), dir,
                                            ch::Granularity::RespectDeclarations);
        REQUIRE(products.ok);
        auto res = ch_test::hier::compare(dev.context(), dir, products.hier_path, products.ports,
                                          pipe_inputs(dev.instance()),
                                          plan_for(dev.instance(),
                                                   ch::Granularity::RespectDeclarations).obs,
                                          stim);
        REQUIRE(res.ok);
        std::set<std::uint64_t> states;
        std::set<std::uint64_t> memvals;
        for (const auto &row : res.trace) {
            states.insert(row[0]);
            memvals.insert(row[1]);
        }
        INFO("distinct states = " << states.size() << ", distinct memvals = " << memvals.size());
        REQUIRE(states.size() >= 2);
        REQUIRE(memvals.size() >= 2);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// B6-⑤ **粒度矩阵**：`ForceInline` vs `ForceLayer` 的逐拍全等
// ⚠️ 只覆盖**不含跨模块 mem 读**的 DUT（design §6.2）
// ⚠️ 观测面 = **两种粒度下都可见**的那些信号（否则读不到的一侧会被误判为分歧）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical equivalence - ForceInline vs ForceLayer match the flat product",
          "[hierarchy][equivalence][granularity]") {
    const std::vector<std::vector<std::uint64_t>> stim = {{0x11}, {0x22}, {0x33}};

    // ⚠️ **跨粒度比较** ⇒ `O` MUST 取**两粒度都可见**的公共子集
    //    （否则读不到的一侧会被误判成分歧）⇒ ⇒ 「可见但未观测」者 MUST 进**显式豁免**
    //    ⇒ ⇒ `base` = `ForceLayer`（公共子集的基准粒度）⇒ ⇒ `mode` = `PublicSubset`
    // ⚠️ 两层 DUT 与 bundle DUT 的 `RespectDeclarations` ≡ `ForceLayer`（叶子声明成层）
    //    ⇒ ⇒ 加那一格**零增量** ⇒ ⇒ 只给 deep DUT 加**第三粒度**（F13）

    // ① 两层 DUT（B5 的 `EqTop`）
    {
        const std::string dir = make_temp_dir("_gran_2layer");
        ch::ch_device<EqTop> dev;
        require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::ForceInline,
                           make_inputs(dev.instance()), stim,
                           plan_for(dev.instance(), ch::Granularity::ForceInline,
                                    ch::Granularity::ForceLayer));
        require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::ForceLayer,
                           make_inputs(dev.instance()), stim,
                           plan_for(dev.instance(), ch::Granularity::ForceLayer,
                                    ch::Granularity::ForceLayer));
    }

    // ② 三层 + 兄弟连线（**三粒度**：deep 的 `RespectDeclarations` = 中间拓扑 ⇒ 唯一增量格）
    {
        const std::string dir = make_temp_dir("_gran_deep");
        ch::ch_device<EqDeepTop> dev;
        require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::ForceInline,
                           deep_inputs(dev.instance()), stim,
                           plan_for(dev.instance(), ch::Granularity::ForceInline,
                                    ch::Granularity::ForceLayer));
        require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::ForceLayer,
                           deep_inputs(dev.instance()), stim,
                           plan_for(dev.instance(), ch::Granularity::ForceLayer,
                                    ch::Granularity::ForceLayer));
        require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::RespectDeclarations,
                           deep_inputs(dev.instance()), stim,
                           plan_for(dev.instance(), ch::Granularity::RespectDeclarations,
                                    ch::Granularity::ForceLayer));
    }

    // ③ bundle 成员跨层
    {
        const std::string dir = make_temp_dir("_gran_bundle");
        ch::ch_device<EqBundleTop> dev;
        require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::ForceInline,
                           bundle_inputs(dev.instance()), stim,
                           plan_for(dev.instance(), ch::Granularity::ForceInline,
                                    ch::Granularity::ForceLayer));
        require_equivalent(dev.instance(), dev.context(), dir, ch::Granularity::ForceLayer,
                           bundle_inputs(dev.instance()), stim,
                           plan_for(dev.instance(), ch::Granularity::ForceLayer,
                                    ch::Granularity::ForceLayer));
    }
}

// ═════════════════════════════════════════════════════════════════════════
// B7-① **确定性**：同一设计两次发射 ⇒ 产物**逐字节相同** + 映射表**逐项**相同
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchical equivalence - two emissions of the same design are identical",
          "[hierarchy][equivalence][determinism]") {
    const std::string dir = make_temp_dir("_det");
    REQUIRE_FALSE(dir.empty());
    ch::ch_device<EqDeepTop> dev;

    const std::string path_a = dir + "/det_a.v";
    const std::string path_b = dir + "/det_b.v";
    std::remove(path_a.c_str());
    std::remove(path_b.c_str());

    auto ra = toVerilogHierarchical(path_a, dev.instance());
    INFO("emit(a) error: " << ra.error);
    REQUIRE(ra.ok);
    auto rb = toVerilogHierarchical(path_b, dev.instance());
    INFO("emit(b) error: " << rb.error);
    REQUIRE(rb.ok);

    // ① 产物**逐字节**相同（同 ABI 内）
    const std::string a = read_file(path_a);
    const std::string b = read_file(path_b);
    REQUIRE_FALSE(a.empty());
    REQUIRE(a == b);

    // ①' **具名**断言「两次 emit 共享同一 ctx」（delta 的 Scenario 明文要求 harness 断言）
    //    ⇒ 判定 = 比较**两次发射各自记录的来源 ctx**（`HierarchyEmitResult::source_ctx`）
    //    ⇒ ⇒ 该断言**可失败**（MUST NOT 用「同一对象与自身比较」⇒ 恒真 ⇒ 零判别力）
    //    ⚠️ **为什么不能只比 `node_id`**：`node_id` 是 ctx **私有**编号 ⇒ ⇒ 两个 ctx 的
    //    编号空间**可能碰巧重合** ⇒ ⇒ 「`node_id` 逐项相同」**不能证明**同 ctx
    //    ⇒ ⇒ 必须有一个 ctx **身份**锚（`source_ctx`）⇒ ⇒ 这才是真正的身份判定
    INFO("both emissions must record the same source ctx");
    REQUIRE(ra.source_ctx != nullptr);
    REQUIRE(rb.source_ctx != nullptr);
    REQUIRE(ra.source_ctx == rb.source_ctx);
    REQUIRE(ra.source_ctx == dev.context());

    // ② 映射表**逐项**相同（**顺序**也相同 ⇒ 顺序确定 ⇒ 不是「集合相等」的弱判据）
    REQUIRE(ra.signal_map.size() == rb.signal_map.size());
    REQUIRE_FALSE(ra.signal_map.empty());
    for (std::size_t i = 0; i < ra.signal_map.size(); ++i) {
        INFO("index " << i);
        REQUIRE(ra.signal_map[i].node_id == rb.signal_map[i].node_id);
        REQUIRE(ra.signal_map[i].signal == rb.signal_map[i].signal);
        REQUIRE(ra.signal_map[i].hier_top_port_name == rb.signal_map[i].hier_top_port_name);
    }
    // ③ 顺序 MUST 是 `node_id` 升序（确定性的一**可机械判定**的形态）
    for (std::size_t i = 1; i < ra.signal_map.size(); ++i) {
        REQUIRE(ra.signal_map[i - 1].node_id < ra.signal_map[i].node_id);
    }
}
