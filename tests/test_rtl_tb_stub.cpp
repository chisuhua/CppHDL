// tests/test_rtl_tb_stub.cpp
//
// **测试台骨架生成**（`examples-rtl-sim-coverage` **E2**）的判据。
//
// ⚠️ **判别力**（本文件 MUST 抓住的具体缺陷）：
//   ① **内部 wire 被当成端口** ⇒ ⇒ 骨架把子组件的「有 driver 的 input」
//      连到 DUT 上 ⇒ ⇒ Verilator 报
//      `Instance attempts to connect to 'X', but it is a variable`（实测踩过）；
//   ② **骨架里混入激励/断言** ⇒ ⇒ 那不是骨架（人补的部分必须留空）；
//   ③ **收尾约定缺失** ⇒ ⇒ 运行护栏无法判红绿（`$finish` 恒返回 0）；
//   ④ **不确定性**（时间戳等）⇒ ⇒ 两次生成必须逐字节相同。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "component.h"
#include "rtl_emit/manifest.h"
#include "rtl_emit/tb_stub.h"

#include <memory>
#include <sstream>
#include <string>

using namespace ch::core;

namespace {

// ── 设计：含**子组件**（它的 input 在扁平化后是内部 wire，不是产物端口）──
class Leaf2 : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din{"din"}; ch_out<ch_uint<8>> dout{"dout"};)
    Leaf2(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class Top2 : public ch::Component {
public:
    __io(ch_in<ch_bool> we{"we"}; ch_in<ch_uint<8>> data{"data"};
         ch_out<ch_uint<8>> result{"result"};)
    ch::ch_module<Leaf2> leaf{"leaf"};
    Top2(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().data;
        io().result <<= leaf.io().dout;
    }
};

std::string make_stub(const ch::rtl_emit::RtlManifest &m) {
    ch::rtl_emit::TbStubOptions o;
    o.dut_module = "top";
    o.example_hint = "unit_test_example";
    std::ostringstream oss;
    ch::rtl_emit::write_tb_stub(oss, m, o);
    return oss.str();
}

} // namespace

// ============================================================================
// 判据 1：顶层端口齐全 · **内部 wire 不得被连线**（判别力 ①）
// ============================================================================
TEST_CASE("tb_stub: 只连**顶层端口**，子组件的 input（内部 wire）不得连线",
          "[rtl][tb_stub]") {
    ch::ch_device<Top2> dev;
    const auto m = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    const std::string s = make_stub(m);

    // 顶层端口 MUST 在实例连接里
    //   ⚠️ 根组件自有端口的 Verilog 名带 `top_` 前缀（`rtl-hierarchical-output`
    //     的口径）⇒ ⇒ 连线是 `.top_we(top_we)`，**不是** `.we(we)`
    REQUIRE(s.find(".top_we(top_we)") != std::string::npos);
    REQUIRE(s.find(".top_data(top_data)") != std::string::npos);
    REQUIRE(s.find(".top_result(top_result)") != std::string::npos);
    // ⚠️ `default_clock` / `default_reset` 由 ctx 提供 ⇒ 也是顶层端口
    REQUIRE(s.find(".default_clock(default_clock)") != std::string::npos);
    REQUIRE(s.find(".default_reset(default_reset)") != std::string::npos);

    // 🔴 **子组件的 input**（名形如 `top_leaf_din`）MUST NOT 出现在**实例连接**里
    //   （它在产物里是内部 wire ⇒ 连上去会让 Verilator 报
    //    `Instance attempts to connect to 'X', but it is a variable`）
    REQUIRE(s.find("top_leaf_din") != std::string::npos);   // 清单注释里点名
    REQUIRE(s.find(".top_leaf_din(") == std::string::npos); // 但不连线
}

// ============================================================================
// 判据 2：骨架**不含**激励与断言（只留 TODO）（判别力 ②）
// ============================================================================
TEST_CASE("tb_stub: 骨架不含激励/断言，只留 TODO 标记", "[rtl][tb_stub]") {
    ch::ch_device<Top2> dev;
    const auto m = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    const std::string s = make_stub(m);

    // TODO 必须存在，且**带上 example 提示**（补 TB 的人知道该写什么）
    REQUIRE(s.find("// ── TODO（unit_test_example）：激励") != std::string::npos);
    REQUIRE(s.find("// ── TODO（unit_test_example）：断言") != std::string::npos);
    // ⚠️ 骨架里 **不得**有任何**实际**的断言语句（那是人补的）
    //   ⇒ ⇒ TODO 注释里给出「怎么写」的示例是允许的 ⇒ ⇒ 只断言「非注释形态」不存在
    REQUIRE(s.find("\n        checks = checks + 1;") == std::string::npos);
    // ⚠️ 也不得出现 `if (fails` 之外的判定……收尾约定里的 `$fatal` 是允许的
    //   ⇒ ⇒ 这里只断言「人补的断言行」不存在
}

// ============================================================================
// 判据 3：收尾约定（运行护栏据此判红绿）（判别力 ③）
// ============================================================================
TEST_CASE("tb_stub: 收尾 MUST 打印 RESULT 行（运行护栏的唯一红绿判据）",
          "[rtl][tb_stub]") {
    ch::ch_device<Top2> dev;
    const auto m = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    const std::string s = make_stub(m);

    REQUIRE(s.find("$display(\"RESULT: checks=%0d fails=%0d\", checks, fails)") !=
            std::string::npos);
    REQUIRE(s.find("$fatal(1") != std::string::npos);
    // `checks` / `fails` 两个计数器 MUST 被声明（否则收尾约定编译不过）
    REQUIRE(s.find("integer checks = 0;") != std::string::npos);
    REQUIRE(s.find("integer fails = 0;") != std::string::npos);
}

// ============================================================================
// 判据 4：时钟/复位骨架 + VCD 开关
// ============================================================================
TEST_CASE("tb_stub: 有时钟/复位驱动骨架与 VCD 开关", "[rtl][tb_stub]") {
    ch::ch_device<Top2> dev;
    const auto m = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    const std::string s = make_stub(m);

    REQUIRE(s.find("always #5 default_clock = ~default_clock;") != std::string::npos);
    REQUIRE(s.find("default_reset = 1;") != std::string::npos);
    REQUIRE(s.find("`ifdef DUMP_VCD") != std::string::npos);
    REQUIRE(s.find("$dumpvars(0, tb_top_stub);") != std::string::npos);
    // module 名与 DUT 名正确
    REQUIRE(s.find("module tb_top_stub;") != std::string::npos);
    REQUIRE(s.find("    top dut (") != std::string::npos);
}

// ============================================================================
// 判据 5：**确定性**（两次生成逐字节相同 · 无时间戳）
// ============================================================================
TEST_CASE("tb_stub: 两次生成逐字节相同（无时间戳）", "[rtl][tb_stub]") {
    std::string a, b;
    {
        ch::ch_device<Top2> d;
        a = make_stub(ch::rtl_emit::build_rtl_manifest(d.instance(), d.context()));
    }
    {
        ch::ch_device<Top2> d;
        b = make_stub(ch::rtl_emit::build_rtl_manifest(d.instance(), d.context()));
    }
    REQUIRE(a == b);
    // 不得含年份 / 时间形态
    REQUIRE(a.find("2026") == std::string::npos);
}
