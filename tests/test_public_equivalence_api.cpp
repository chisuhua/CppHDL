// tests/test_public_equivalence_api.cpp
//
// **公共 helper 的仓外视角消费者**（P8 · capability
// `hierarchical-equivalence-public-helper` 的 Requirement ①/②/③）。
//
// ⚠️ **为什么这个文件存在**（★ 它是本 change 最重要的判据）：
//   仓内测试能拿到的 include 路径与**仓外**不同 ⇒ ⇒ 仓内测试**证明不了**
//   「公共面真的够用」⇒ ⇒ ★ metis 第 1 轮 finding #3：
//   「`-fsyntax-only` 只证明语法 ⇒ ⇒ 不证明链接 / 不证明默认参数可用 / 不证明能跑」
//   ⇒ ⇒ 本文件**真编译 + 真链接 + 真运行** ⇒ ⇒ 且它**只** include 公共头
//   （★ 不 include `ch.hpp` · 不 include `hierarchy_verilog.h` · 不 include
//   `tests/hier_equivalence.h` ⇒ ⇒ ⇒ ⇒ **公共面自足**的机械证据）。
//
// ⚠️ **它同时是文档示例的来源**（★ tasks.md 7.8：文档 MUST 引用真实测试源文件
//   ⇒ ⇒ 杜绝文档腐烂）。
//
// ⚠️ **覆盖全部公共符号**（★ tasks.md 4.3 的**能力判据**）：
//   `emit` · `compare` · `ProductSet` · `Result` · `DrivenInput` · `Observation`
//   · `make_observation` ⇒ ⇒ 少任一项 ⇒ ⇒ **红**。

#include "hierarchy_equivalence.h"

#include "catch_amalgamated.hpp"

#include "component.h"
#include "core/context.h"
#include "device.h"
#include "module.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ── DUT：**两层**（根 + 一个**声明成层**的叶子）⇒ ⇒ 层次化产物真有多个 module ──
class PublicLeaf : public Component {
public:
    CH_HIERARCHY_LAYER("public_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    PublicLeaf(Component *p = nullptr, const std::string &n = "leaf")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class PublicTop : public Component {
public:
    CH_HIERARCHY_LAYER("public_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_module<PublicLeaf> leaf{"leaf"};
    PublicTop(Component *p = nullptr, const std::string &n = "top")
        : Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().din;
        io().dout <<= leaf.io().dout;
    }
};

/// 仓外视角的**工作目录**（★ 仓外用户**没有** `make_temp_dir` ⇒ ⇒ 自己造一个）。
std::string make_work_dir(const char *tag) {
    const char *base = std::getenv("TMPDIR");
    std::string dir = std::string(base != nullptr ? base : "/tmp") + "/cpphdl_pub_api_" + tag;
    std::string cmd = "rm -rf '" + dir + "' && mkdir -p '" + dir + "'";
    if (std::system(cmd.c_str()) != 0) {
        return {};
    }
    return dir;
}

}  // namespace

// ===========================================================================
// ① 公共面**自足**：只 include `hierarchy_equivalence.h` 就能用全部符号
// ===========================================================================

TEST_CASE("公共 helper：只 include 公共头就能用全部公共符号（仓外视角）",
          "[hierarchy][public-api][surface]") {
    // ★ 本文件**只** include `hierarchy_equivalence.h`（+ Catch2 与组件基类）
    //   ⇒ ⇒ ⇒ 「公共面自足」由**编译本身**证明。

    // ① `emit` + `ProductSet`
    const std::string dir = make_work_dir("surface");
    REQUIRE_FALSE(dir.empty());

    ch_device<PublicTop> dev;
    const ch::hier_eq::ProductSet products =
        ch::hier_eq::emit(dev.instance(), dir, ch::Granularity::RespectDeclarations);
    INFO("emit error: " << products.error);
    REQUIRE(products.ok);
    // ⚠️ `ok == true` ⇒ **无**错误信息（★ 错误串只在失败时非空）
    REQUIRE(products.error.empty());
    REQUIRE_FALSE(products.hier_path.empty());
    // ⚠️ 端口集合 MUST **非空** ⇒ ⇒ 「由产物派生」这件事发生了
    REQUIRE_FALSE(products.ports.empty());
    // ⚠️ 映射表 MUST **非空** ⇒ ⇒ 「由产物派生」这件事发生了
    REQUIRE_FALSE(products.signal_map.empty());

    // ② `DrivenInput`
    std::vector<ch::hier_eq::DrivenInput> inputs;
    inputs.push_back(ch::hier_eq::DrivenInput{
        "top.din", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().din, v);
        }});

    // ③ `Observation` + `make_observation`（★ 用户 MUST NOT 手填 `node_id`）
    std::vector<ch::hier_eq::Observation> observations;
    observations.push_back(ch::hier_eq::make_observation(
        "top.dout", dev.instance().io().dout,
        [&dev](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(dev.instance().io().dout));
        }));
    // ⚠️ 工厂 MUST 自己填了 `node_id`（★ 否则对账无从做起）
    REQUIRE(observations[0].node_id != 0u);

    // ④ `compare` + `Result`
    const std::vector<std::vector<std::uint64_t>> stim = {{0x11}, {0x22}, {0x33}, {0x44}};
    const ch::hier_eq::Result result =
        ch::hier_eq::compare(dev.context(), dir, products.hier_path, products.ports, inputs,
                              observations, stim);
    INFO("compare error: " << result.error);
    INFO("first mismatch: " << result.first_mismatch);
    REQUIRE(result.ok);
    REQUIRE(result.equal);
    REQUIRE(result.mismatch_count == 0u);
    REQUIRE(result.first_mismatch.empty());
    // ⚠️ `flat_trace` MUST 与激励**逐拍**对应（★ 字段名 MUST 体现「扁平侧」）
    REQUIRE(result.flat_trace.size() == stim.size());
    for (const auto &row : result.flat_trace) {
        REQUIRE(row.size() == observations.size());
    }
    // ⚠️ 两份产物文本 MUST 非空（★ 供用户自行 diff）
    REQUIRE_FALSE(result.flat_product.empty());
    REQUIRE_FALSE(result.hier_product.empty());
}

// ===========================================================================
// ② fail-closed：退化输入 MUST 被**拒绝**（而不是恒绿）
// ===========================================================================

TEST_CASE("公共 helper：退化观测面 MUST 被拒绝（fail-closed）",
          "[hierarchy][public-api][fail-closed]") {
    const std::string dir = make_work_dir("failclosed");
    REQUIRE_FALSE(dir.empty());

    ch_device<PublicTop> dev;
    const ch::hier_eq::ProductSet products =
        ch::hier_eq::emit(dev.instance(), dir, ch::Granularity::RespectDeclarations);
    REQUIRE(products.ok);

    std::vector<ch::hier_eq::DrivenInput> inputs;
    inputs.push_back(ch::hier_eq::DrivenInput{
        "top.din", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().din, v);
        }});

    // ① **空**观测面 ⇒ MUST 拒绝（★ 恒绿 ⇒ 空洞判据）
    {
        const std::vector<std::vector<std::uint64_t>> stim = {{0x11}, {0x22}};
        const ch::hier_eq::Result result =
            ch::hier_eq::compare(dev.context(), dir, products.hier_path, products.ports,
                                  inputs, {}, stim);
        INFO("error: " << result.error);
        REQUIRE_FALSE(result.ok);
        REQUIRE_FALSE(result.equal);
        REQUIRE(result.error.rfind("empty-observations", 0) == 0);
    }

    // ② **空**激励 ⇒ MUST 拒绝
    {
        std::vector<ch::hier_eq::Observation> observations;
        observations.push_back(ch::hier_eq::make_observation(
            "top.dout", dev.instance().io().dout,
            [&dev](ch::Simulator &s) -> std::uint64_t {
                return static_cast<std::uint64_t>(s.get_port_value(dev.instance().io().dout));
            }));
        const ch::hier_eq::Result result =
            ch::hier_eq::compare(dev.context(), dir, products.hier_path, products.ports,
                                  inputs, observations, {});
        INFO("error: " << result.error);
        REQUIRE_FALSE(result.ok);
        REQUIRE_FALSE(result.equal);
        REQUIRE(result.error.rfind("empty-stimulus", 0) == 0);
    }
}

// ===========================================================================
// ②-bis 恒定激励 ⇒ 观测面**不携带信息** ⇒ MUST 被拒绝（★ 空洞判据的防线）
// ===========================================================================

TEST_CASE("公共 helper：恒定激励下观测面不携带信息 ⇒ MUST 被拒绝",
          "[hierarchy][public-api][fail-closed][no-varying]") {
    const std::string dir = make_work_dir("novarying");
    REQUIRE_FALSE(dir.empty());

    ch_device<PublicTop> dev;
    const ch::hier_eq::ProductSet products =
        ch::hier_eq::emit(dev.instance(), dir, ch::Granularity::RespectDeclarations);
    REQUIRE(products.ok);

    std::vector<ch::hier_eq::DrivenInput> inputs;
    inputs.push_back(ch::hier_eq::DrivenInput{
        "top.din", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().din, v);
        }});
    std::vector<ch::hier_eq::Observation> observations;
    observations.push_back(ch::hier_eq::make_observation(
        "top.dout", dev.instance().io().dout,
        [&dev](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(dev.instance().io().dout));
        }));

    // ⚠️ **恒定激励** ⇒ ⇒ 观测值逐拍**恒定** ⇒ ⇒ 「逐拍全等」对**任何**恒定产物
    //    都成立 ⇒ ⇒ 判据**无判别力** ⇒ ⇒ MUST 被拒绝。
    const std::vector<std::vector<std::uint64_t>> const_stim = {{0x11}, {0x11}, {0x11}};
    const ch::hier_eq::Result result =
        ch::hier_eq::compare(dev.context(), dir, products.hier_path, products.ports, inputs,
                              observations, const_stim);
    INFO("error: " << result.error);
    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.equal);
    REQUIRE(result.error.rfind("no-varying-signal", 0) == 0);
}

// ===========================================================================
// ③ 错误串 MUST **可行动**（★ 仓内已知陷阱：`apt install verilator` 是错的）
// ===========================================================================

TEST_CASE("公共 helper：错误串 MUST **不**建议已知错误的获取方式",
          "[hierarchy][public-api][error-actionable]") {
    const std::string dir = make_work_dir("actionable");
    REQUIRE_FALSE(dir.empty());

    ch_device<PublicTop> dev;
    const ch::hier_eq::ProductSet products =
        ch::hier_eq::emit(dev.instance(), dir, ch::Granularity::RespectDeclarations);
    REQUIRE(products.ok);

    // ⚠️ 构造一个**必然失败**的输入（观测一个**不存在**的顶层端口）
    std::vector<ch::hier_eq::DrivenInput> inputs;
    inputs.push_back(ch::hier_eq::DrivenInput{
        "top.din", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().din, v);
        }});

    std::vector<ch::hier_eq::Observation> observations;
    observations.push_back(ch::hier_eq::Observation{
        "bogus.not.a.port", [](ch::Simulator &) -> std::uint64_t { return 0; }, 0xDEADBEEFu});

    const std::vector<std::vector<std::uint64_t>> stim = {{0x11}, {0x22}};
    const ch::hier_eq::Result result =
        ch::hier_eq::compare(dev.context(), dir, products.hier_path, products.ports, inputs,
                              observations, stim);
    INFO("error: " << result.error);
    REQUIRE_FALSE(result.ok);
    // ⚠️ 稳定前缀 ⇒ ⇒ 用户可机械分类
    REQUIRE(result.error.rfind("observation-not-visible", 0) == 0);
    // ⚠️ **MUST NOT** 建议错误的路（★ `apt install verilator` 在本仓是错的）
    REQUIRE(result.error.find("apt install verilator") == std::string::npos);
}

TEST_CASE("公共 helper：verilator 不可用 ⇒ MUST 失败并给出**正确**的获取方式",
          "[hierarchy][public-api][error-actionable][verilator-missing]") {
    const std::string dir = make_work_dir("noverilator");
    REQUIRE_FALSE(dir.empty());

    ch_device<PublicTop> dev;
    const ch::hier_eq::ProductSet products =
        ch::hier_eq::emit(dev.instance(), dir, ch::Granularity::RespectDeclarations);
    REQUIRE(products.ok);

    std::vector<ch::hier_eq::DrivenInput> inputs;
    inputs.push_back(ch::hier_eq::DrivenInput{
        "top.din", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().din, v);
        }});
    std::vector<ch::hier_eq::Observation> observations;
    observations.push_back(ch::hier_eq::make_observation(
        "top.dout", dev.instance().io().dout,
        [&dev](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(dev.instance().io().dout));
        }));
    const std::vector<std::vector<std::uint64_t>> stim = {{0x11}, {0x22}, {0x33}};

    // ⚠️ **把 PATH 掏空** ⇒ ⇒ `command -v verilator` 失败 ⇒ ⇒ 走
    //    `verilator-unavailable` 分支（★ **真**跑那条路径 ⇒ ⇒ 不是纸面断言）。
    const char *old_path = std::getenv("PATH");
    const std::string saved_path = (old_path != nullptr) ? old_path : "";
    ::setenv("PATH", "/nonexistent-cpphdl-path", 1);

    ch::hier_eq::Result result;
    try {
        result = ch::hier_eq::compare(dev.context(), dir, products.hier_path, products.ports,
                                       inputs, observations, stim);
    } catch (...) {
        ::setenv("PATH", saved_path.c_str(), 1);
        throw;
    }
    ::setenv("PATH", saved_path.c_str(), 1);

    INFO("error: " << result.error);
    // ⚠️ **MUST NOT** 呈现为「比较通过」（★ 那是最危险的静默失效）
    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.equal);
    // ⚠️ 稳定前缀
    REQUIRE(result.error.rfind("verilator-unavailable", 0) == 0);
    // ⚠️ **MUST** 给出**正确**的路
    REQUIRE(result.error.find("cmake --build build --target verilator") != std::string::npos);
    // ⚠️ **MUST NOT** 建议已知错误的路
    REQUIRE(result.error.find("apt install verilator") == std::string::npos);
}
