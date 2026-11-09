// tests/test_mem_port_arg_cross_layer_values.cpp
//
// `ch_mem` 端口实参跨层（ADR-066 决策 ⑧）· **AC3 的产物级值判据**。
//
// ⚠️ **为什么必须单独一个文件**（而不是并入 `test_mem_port_arg_cross_layer.cpp`）：
//   本文件的判据 MUST 在**层次化产物**上跑（verilator 编译产物 + 逐拍仿真）⇒ ⇒ 需要
//   verilator ⇒ ⇒ 进 `verilator_e2e` 标签 ⇒ ⇒ **不能**与纯产物级/解释器级用例同 target。
//
// ⚠️ **判别力（核心）**：`test_mem_port_arg_cross_layer.cpp` 的值判据跑在**解释器**上
// ⇒ ⇒ **抓不到**「产物里父侧网缺失 ⇒ Verilog 当 implicit wire（1-bit）⇒ 宽度截断 ⇒ 值错」
// 这一族缺陷（解释器不看 Verilog 产物）⇒ ⇒ **本文件**才是那一族缺陷的判据面。
//
// ⚠️ **值判据 MUST 手推**（AGENTS.md：禁只抄仿真输出）⇒ ⇒ 本文件用**可枚举的初值表**
// ⇒ ⇒ 期望值 = **查表**（不是抄输出）⇒ ⇒ 「非 0」只是下界 ⇒ ⇒ 断言的是**确定值**。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "hierarchy_verilog.h"
#include "hier_equivalence.h"
#include "module.h"
#include "simulator.h"

#include <cstdint>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

using namespace ch;
using namespace ch::core;

namespace {

// ── DUT：**(ii) 形态** —— mem 本体在**父** · 实参视线**跨层**（取自子的 `ch_out`）──
// ⚠️ 与 `test_mem_port_arg_cross_layer.cpp` 的 DUT **同形态**（刻意）⇒ ⇒ 两个文件是
// **同一形态的两个判据面**（产物级 / 行为级）⇒ ⇒ 任一面缺失 ⇒ ⇒ 那一族缺陷测不出。
class VArgSrc : public ch::Component {
public:
    CH_HIERARCHY_LAYER("v_arg_src");
    __io(ch_in<ch_uint<4>> addr_in; ch_out<ch_uint<4>> addr;)
    VArgSrc(ch::Component *p = nullptr, const std::string &n = "v_arg_src")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().addr <<= io().addr_in; }
};

class VMemArgTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("v_mem_arg_top");
    __io(ch_in<ch_uint<4>> addr_in; ch_out<ch_uint<8>> rdata;)
    /// ⚠️ **初值表**（手推基准）：地址 k ⇒ 值 `0xD0 + k`
    ch_mem<ch_uint<8>, 16> mem{std::vector<std::uint64_t>{
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
        0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF}, "mem"};
    ch::ch_module<VArgSrc> src{"src"};
    VMemArgTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        src.io().addr_in <<= io().addr_in;
        // ★ **(ii) 形态**：mem 端口的 `addr` 实参槽取自**子**的 `ch_out`
        io().rdata <<= mem.aread(src.io().addr);
    }
};

std::string make_temp_dir(const char *suffix) {
    char tmpl[] = "/tmp/cpphdl_memarg_XXXXXX";
    char *dir = ::mkdtemp(tmpl);
    if (!dir) {
        return {};
    }
    std::string base(dir);
    std::string path = base + suffix;
    ::mkdir(path.c_str(), 0755);
    return path;
}

}   // namespace

// ═════════════════════════════════════════════════════════════════════════
// AC3（**产物级**值判据）：层次化产物 MUST 读出**手推**的确定值
//
// ⚠️ **判别力的来源**：若父侧落点网**未登记** ⇒ ⇒ 父 module 内该名字**无 `logic` 声明**
// ⇒ ⇒ Verilog 当 **implicit wire（1-bit）** ⇒ ⇒ **地址被截断** ⇒ ⇒ 读到的值 ≠ 手推值
// ⇒ ⇒ 逐拍比对**不等** ⇒ ⇒ 本用例**转红**。
//
// ⚠️ **为什么还要断言「手推值」**（而不只是「两侧相等」）：「两侧相等」只证明
// 「层次化 ≡ 扁平」⇒ ⇒ **两侧一起错**时恒绿 ⇒ ⇒ 必须再钉一条**与产物无关**的
// 绝对基准（手推表）⇒ ⇒ 三者成链：层次化 ≡ 扁平 ∧ 扁平 ≡ 手推 ⇒ ⇒ 层次化 ≡ 手推。
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("mem port arg cross layer - the layered product reads the hand-derived value",
          "[hierarchy][mem][cross-layer][value][verilator]") {
    const std::string dir = make_temp_dir("_values");
    REQUIRE_FALSE(dir.empty());
    ch::ch_device<VMemArgTop> dev;

    // ⚠️ **激励**：逐拍不同的地址 ⇒ ⇒ 观测值**随激励变化**（harness 的
    // `no-varying-signal` 判据需要）⇒ ⇒ 且每拍的期望值**可手推**（查表）。
    const std::vector<std::vector<std::uint64_t>> stim = {
        {0x03}, {0x07}, {0x0A}, {0x0F}, {0x02}, {0x05}};

    auto products = ch_test::hier::emit(dev.instance(), dir, ch::Granularity::ForceLayer);
    INFO("emit error: " << products.error);
    REQUIRE(products.ok);
    REQUIRE_FALSE(products.ports.empty());

    // 观测面 = 顶层 `rdata`（**唯一**的可观测输出 ⇒ ⇒ MUST 随激励变化）
    std::vector<ch_test::hier::Observation> obs;
    obs.push_back(ch_test::hier::make_observation(
        "top.rdata", dev.instance().io().rdata,
        [&dev](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(dev.instance().io().rdata));
        }));

    std::vector<ch_test::hier::DrivenInput> inputs;
    inputs.push_back({"top.addr_in", [&dev](ch::Simulator &s, std::uint64_t v) {
                          s.set_input_value(dev.instance().io().addr_in, v);
                      }});

    auto res = ch_test::hier::compare(dev.context(), dir, products.hier_path, products.ports,
                                      inputs, obs, stim);
    INFO("compare error: " << res.error);
    // ⚠️ **verilator 缺席 ⇒ SKIP（不是 FAIL）**：本 target **不带** `verilator_e2e` 标签
    //   （⇒ ⇒ 该标签的口径 = 「**双后端 parity** 覆盖」⇒ ⇒ 本用例是**单后端**层次化比对
    //   ⇒ ⇒ 带标签会污染口径）⇒ ⇒ 因此 `BUILD_VERILATOR=OFF` 下它仍会被 `base` 跑到
    //   ⇒ ⇒ MUST 自行降级为 SKIP（与 `DUAL_BACKEND_TEST` 的「log-and-skip」精神一致）。
    if (!res.ok && res.error.rfind("verilator-unavailable", 0) == 0) {
        SKIP("verilator not available: " + res.error);
    }
    INFO("first mismatch: " << res.first_mismatch);
    REQUIRE(res.ok);
    // ★ **层次化 ≡ 扁平**
    REQUIRE(res.equal);
    REQUIRE(res.mismatch_count == 0);

    // ★ **手推的绝对基准**（查表 ⇒ ⇒ 不抄仿真输出）
    REQUIRE(res.flat_trace.size() == stim.size());
    for (std::size_t k = 0; k < stim.size(); ++k) {
        REQUIRE(res.flat_trace[k].size() == 1);
        const std::uint64_t k_addr = stim[k][0];
        const std::uint64_t k_expected = 0xD0 + k_addr;   // ⚠️ 手推：初值表 `0xD0 + k`
        INFO("cycle " << k << " · addr = " << k_addr << " · expected = 0x" << std::hex
             << k_expected << " · got = 0x" << res.flat_trace[k][0]);
        REQUIRE(res.flat_trace[k][0] == k_expected);
    }

    // ★ **反向判据**（防恒真）：若读到的值**恒为**某个常量 ⇒ ⇒ 上面的逐拍断言会挂
    //   ⇒ ⇒ 这里显式确认「值确实**逐拍不同**」⇒ ⇒ 证明该判据**不是**「非 0 即过」。
    {
        std::set<std::uint64_t> distinct;
        for (const auto &row : res.flat_trace) {
            distinct.insert(row[0]);
        }
        INFO("distinct observed values = " << distinct.size());
        REQUIRE(distinct.size() >= 3);
    }
}
