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
#include "codegen_naming.h"
#include "hierarchy_verilog.h"
#include "hier_equivalence.h"
#include "simulator.h"

#include <cstdlib>
#include <fstream>
#include <map>
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

/// 观测集合 `O`（**手写**）。
///
/// ⚠️ `O` MUST ⊇ DUT 的**全部 `ch_out`**（任务 2.2）—— ⚠️ 但**成层子树**的 `ch_out`
///    在层次化顶层**不可观测**（ADR-058 §4 风险 6 的盲区）⇒ ⇒ 对**成层** DUT，
///    `O` = 「**两侧顶层都可见**的 `ch_out`」+ 盲区**显式登记**（MUST NOT 声称
///    「全部输出已比对」）。
template <typename Dut>
std::vector<ch_test::hier::Observation> make_observations(Dut &root) {
    std::vector<ch_test::hier::Observation> obs;
    obs.push_back({"top.dout", [&root](ch::Simulator &sim) -> std::uint64_t {
                       return static_cast<std::uint64_t>(
                           sim.get_port_value(root.io().dout));
                   }});
    return obs;
}

/// **全 inline** DUT 的 `O`：MUST ⊇ **全部** `ch_out`（叶子那个也在顶层 ⇒ 可观测）。
template <typename Dut>
std::vector<ch_test::hier::Observation> make_inline_observations(Dut &root) {
    auto obs = make_observations(root);
    obs.push_back({"leaf.dout", [&root](ch::Simulator &sim) -> std::uint64_t {
                       return static_cast<std::uint64_t>(
                           sim.get_port_value(root.leaf.io().dout));
                   }});
    return obs;
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

    const auto obs = make_inline_observations(dev.instance());
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

    const auto obs = make_observations(dev.instance());
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

    const auto obs = make_observations(dev.instance());
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
