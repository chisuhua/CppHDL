// tests/test_product_check.cpp
//
// **产物级自检的产品无关规则**的判别力测试（`example-file-boundary-2026-11` **F2**）。
//
// ⚠️ **本测试测的是「规则」**（`ch::rtl_emit::product_modules` · `tamper_hier` ·
//   `check_products`）⇒ ⇒ example 侧的**值**（期望 module 集 / 端口数 / 激励面 /
//   观测面 / 阈值）**不在这里** ⇒ ⇒ 本测试用**自带的**最小设计 + 自带值。
//
// ⚠️ **逐判据判别力**（AC18「4 项判据各自注入错期望 ⇒ 必红」）：本文件**捕获
//   `check_products` 的 stdout** ⇒ ⇒ 断言**具体那条 CHECK 行**转 FAIL
//   （而不是只看聚合退出码 —— 那样任何一项红都能满足，判别力为空）。
//
// ⚠️ 本测试**不依赖** verilator：CHECK 4 在无 verilator 时按失败计（fail-closed）
//   ⇒ ⇒ 断言只落在「本用例注入的那一项」，不假设环境。

#include "rtl_emit/product_check.h"

#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "hierarchy_equivalence.h"
#include "simulator.h"

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace ch;
using namespace ch::rtl_emit;

namespace {

/// 测试用的最小顶层（1 in + 1 out ⇒ 端口表 2 ⇒ 够跑「端口面」判据）
class TinyTop : public Component {
public:
    __io(
        ch::core::ch_in<ch::core::ch_uint<8>> in;
        ch::core::ch_out<ch::core::ch_uint<8>> out;
    );

    TinyTop() : Component(nullptr, "top") {}
    void describe() override {
        io().out <<= io().in;
    }
};

std::unique_ptr<Component> make_tiny(ch::core::context *) {
    return std::make_unique<TinyTop>();
}

std::string write_temp(const std::string &dir, const std::string &name,
                        const std::string &content) {
    std::filesystem::create_directories(dir);
    const std::string path = dir + "/" + name;
    std::ofstream f(path);
    f << content;
    return path;
}

/// 跑 `check_products` 并把 **stdout** 捞出来（⇒ 可对**具体 CHECK 行**断言）
struct Run {
    int rc = 0;
    std::string out;
};

Run run_check(const ProductCheckSpec &spec, const ProductCheckHooks &hooks,
              const DesignFactory &factory = make_tiny) {
    std::ostringstream oss;
    std::streambuf *old = std::cout.rdbuf(oss.rdbuf());
    Run r;
    r.rc = check_products(spec, hooks, factory);
    std::cout.rdbuf(old);
    r.out = oss.str();
    return r;
}

ProductCheckHooks empty_hooks() {
    ProductCheckHooks hooks;
    hooks.make_inputs = [](Component &) { return std::vector<hier_eq::DrivenInput>{}; };
    hooks.make_observations = [](Component &) { return std::vector<hier_eq::Observation>{}; };
    hooks.make_stimulus = []() { return std::vector<std::vector<std::uint64_t>>{}; };
    return hooks;
}

/// 基线 spec：**CHECK 1 与 CHECK 2 都通过**（⇒ 注入单项错误时能断言「只有该项红」）
ProductCheckSpec base_spec(const std::string &dir, const std::string &modules_src) {
    ProductCheckSpec spec;
    spec.verilog_path = write_temp(dir, "top.v", modules_src);
    spec.work_dir = dir;
    spec.expected_modules = {"top"};
    spec.expected_top_ports = 2;                 // TinyTop = in + out
    spec.min_varying_observations = 1;
    spec.lint = [](const std::string &) { return true; };
    return spec;
}

}   // namespace

TEST_CASE("product_modules：只认行首的 module 声明", "[rtl_emit][product_check]") {
    const std::string path =
        write_temp("build/test_product_check/pm", "top.v",
                   "module top (\n"
                   "  input wire a,\n"
                   "  output wire b\n"
                   ");\n"
                   "  assign b = a;\n"
                   "endmodule\n"
                   "module child (\n"
                   ");\n"
                   "endmodule\n"
                   // ⚠️ **行首不是 `module `** ⇒ MUST NOT 计入
                   "  // module commented_out (\n"
                   "endmodule\n");

    const auto mods = product_modules(path);
    REQUIRE(mods.size() == 2);
    REQUIRE(mods.count("top") == 1);
    REQUIRE(mods.count("child") == 1);
    REQUIRE(mods.count("commented_out") == 0);   // ⇒ 注释里的 module 不得计入
}

TEST_CASE("product_modules：文件不存在 ⇒ 空集（不得崩）", "[rtl_emit][product_check]") {
    REQUIRE(product_modules("build/test_product_check/missing.v").empty());
}

TEST_CASE("tamper_hier：空观测面 ⇒ 返回空串", "[rtl_emit][product_check]") {
    const std::string path = write_temp("build/test_product_check/th", "hier.v",
                                        "module top (\n);\nendmodule\n");
    REQUIRE(tamper_hier(path, {}, {}, "top").empty());
}

TEST_CASE("tamper_hier：命中观测端口 ⇒ 产出取反后的副本", "[rtl_emit][product_check]") {
    const std::string path = write_temp(
        "build/test_product_check/th2", "hier.v",
        "module top (\n"
        "  output wire top_out\n"
        ");\n"
        "  child u_child (\n"
        "    .sig(top_out)\n"       // 连接形如 `.child_port(top_port)`
        "  );\n"
        "  assign sig = 1'b0;\n"   // 被取反的 `assign <child_port> = <rhs>;`
        "endmodule\n");

    hier_eq::Observation obs;
    obs.name = "top.out";
    obs.node_id = 42;

    ch::SignalMapEntry e;
    e.node_id = 42;
    e.hier_top_port_name = "top_out";

    const std::string out = tamper_hier(path, {e}, {obs}, "top");
    REQUIRE_FALSE(out.empty());

    std::ifstream f(out);
    const std::string text((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
    REQUIRE(text.find("assign sig = ~(1'b0);") != std::string::npos);
}

TEST_CASE("check_products：用法错误 ⇒ 在进入判据前 fail-closed", "[rtl_emit][product_check]") {
    ProductCheckSpec spec;
    // ① 工厂为空
    {
        const Run r = run_check(spec, empty_hooks(), DesignFactory{});
        REQUIRE(r.rc == 1);
        REQUIRE(r.out.find("CHECK 1") == std::string::npos);   // ⇒ 未进入判据
    }
    // ② 激励面回调缺失
    {
        ProductCheckHooks hooks;
        hooks.make_inputs = [](Component &) { return std::vector<hier_eq::DrivenInput>{}; };
        const Run r = run_check(spec, hooks);
        REQUIRE(r.rc == 1);
        REQUIRE(r.out.find("CHECK 1") == std::string::npos);
    }
}

TEST_CASE("check_products：基线（CHECK 1/2 都过）", "[rtl_emit][product_check]") {
    const auto spec = base_spec("build/test_product_check/base", "module top (\n);\nendmodule\n");
    const Run r = run_check(spec, empty_hooks());
    REQUIRE(r.out.find("CHECK 1 module 集: PASS") != std::string::npos);
    REQUIRE(r.out.find("CHECK 2 顶层端口: PASS") != std::string::npos);
    REQUIRE(r.out.find("CHECK 3 lint: PASS") != std::string::npos);
}

TEST_CASE("check_products：① module 集注入错期望 ⇒ CHECK 1 必红", "[rtl_emit][product_check]") {
    auto spec = base_spec("build/test_product_check/p1", "module top (\n);\nendmodule\n");
    spec.expected_modules = {"top", "ghost"};   // ⇒ 故意多一个
    const Run r = run_check(spec, empty_hooks());
    REQUIRE(r.out.find("CHECK 1 module 集: FAIL") != std::string::npos);
    REQUIRE(r.out.find("CHECK 2 顶层端口: PASS") != std::string::npos);   // 只有 ① 红
    REQUIRE(r.rc == 1);
}

TEST_CASE("check_products：② 端口数注入错期望 ⇒ CHECK 2 必红", "[rtl_emit][product_check]") {
    auto spec = base_spec("build/test_product_check/p2", "module top (\n);\nendmodule\n");
    spec.expected_top_ports = 99;   // ⇒ 故意错
    const Run r = run_check(spec, empty_hooks());
    REQUIRE(r.out.find("CHECK 2 顶层端口: FAIL") != std::string::npos);
    REQUIRE(r.out.find("CHECK 1 module 集: PASS") != std::string::npos);   // 只有 ② 红
    REQUIRE(r.rc == 1);
}

TEST_CASE("check_products：③ linter 缺失 / 报脏 ⇒ CHECK 3 必红", "[rtl_emit][product_check]") {
    // 缺失 ⇒ 「未验证」按失败计（fail-closed）
    {
        auto spec = base_spec("build/test_product_check/p3a", "module top (\n);\nendmodule\n");
        spec.lint = {};
        const Run r = run_check(spec, empty_hooks());
        REQUIRE(r.out.find("CHECK 3 lint: FAIL") != std::string::npos);
        REQUIRE(r.out.find("linter 不可用") != std::string::npos);
    }
    // 报了脏 ⇒ 按失败计
    {
        auto spec = base_spec("build/test_product_check/p3b", "module top (\n);\nendmodule\n");
        spec.lint = [](const std::string &) { return false; };
        const Run r = run_check(spec, empty_hooks());
        REQUIRE(r.out.find("CHECK 3 lint: FAIL") != std::string::npos);
        REQUIRE(r.out.find("linter 不可用") == std::string::npos);
    }
}

TEST_CASE("check_products：④ 观测面注入空集 ⇒ CHECK 4 必红", "[rtl_emit][product_check]") {
    const auto spec = base_spec("build/test_product_check/p4", "module top (\n);\nendmodule\n");
    const Run r = run_check(spec, empty_hooks());   // empty_hooks ⇒ 空观测面
    REQUIRE(r.out.find("CHECK 4 双形态等价: FAIL") != std::string::npos);
    REQUIRE(r.rc == 1);
}
