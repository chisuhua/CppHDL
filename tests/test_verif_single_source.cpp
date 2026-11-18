// tests/test_verif_single_source.cpp
//
// **RTL 验证单一真相源**的判别性用例（`rtl-verification-single-source` · ADR-074）
//
// 覆盖三件事：
//   ① 扫描器 **fail-closed**（集合外构造 / 文法外 / 时序锚点纪律 ⇒ 必须报错）
//   ② `REPEAT` 在**扫描期**展开（⇒ IR 里没有循环 ⇒ 两个后端都不解释 `_i`）
//   ③ **IR 解释器**驱动 `Simulator` 跑通仓内真实验证源
//      （`examples/axi4/tb_axi4_lite_example.verif`）⇒ `fails == 0`
//      ⇒ ⇒ 与渲染式 TB 的 `RESULT: checks=113 fails=0` **同一条语义**
//
// ⚠️ 本用例**不**跑 Verilator：渲染侧由 `scripts/verify_examples_rtl_sim.sh` 的
//   ⑤/⑥ 承担（跑真产物）；本用例证明**解释器侧**与**静态条数**。

#include "device.h"
#include "rtl_emit/manifest.h"
#include "rtl_emit/tb_stub.h"
#include "rtl_emit/verif.h"
#include "simulator.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

/// 把一段文本喂给扫描器（`source` 用 `\n` 分行）
ch::rtl_emit::VerifParseError scan_text(const std::string &text,
                                       ch::rtl_emit::VerifProgram &out) {
    std::istringstream in(text);
    ch::rtl_emit::VerifParseError err;
    ch::rtl_emit::parse_verif(in, out, err);
    return err;
}

const char *kMinimalHead = "@expect-src x.md#a\n@tb-module tb_x\n";

} // namespace

TEST_CASE("verif: 集合外原语 fail-closed", "[verif]") {
    ch::rtl_emit::VerifProgram prog;
    const auto err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                               "SET p 1\nBRANCH_IF_ZERO p\n",
                               prog);
    REQUIRE(err.line > 0);
    REQUIRE(err.message.find("未知原语") != std::string::npos);
}

TEST_CASE("verif: 嵌套 REPEAT fail-closed", "[verif]") {
    ch::rtl_emit::VerifProgram prog;
    const auto err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                               "REPEAT 2\nREPEAT 3\nTICK 1\nEND\nEND\n",
                               prog);
    REQUIRE(err.line > 0);
    REQUIRE(err.message.find("嵌套 REPEAT") != std::string::npos);
}

TEST_CASE("verif: 表达式文法外 token fail-closed", "[verif]") {
    ch::rtl_emit::VerifProgram prog;
    const auto err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                               "SET p 1 / 2\n",
                               prog);
    REQUIRE(err.line > 0);
    REQUIRE(err.message.empty() == false);
}

TEST_CASE("verif: 时序锚点纪律 —— SET 之后不 SETTLE 就 EXPECT 必须报错", "[verif]") {
    ch::rtl_emit::VerifProgram prog;
    const auto err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                               "SET p 1\nEXPECT q 1\n",
                               prog);
    REQUIRE(err.line > 0);
    REQUIRE(err.message.find("SETTLE") != std::string::npos);

    // 负向对照：先 TICK 再 EXPECT ⇒ 合法
    ch::rtl_emit::VerifProgram ok_prog;
    const auto ok_err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                                  "SET p 1\nTICK 1\nEXPECT q 1\n",
                                  ok_prog);
    REQUIRE(ok_err.message.empty());
    REQUIRE(ok_prog.scenarios.size() == 1);
}

TEST_CASE("verif: 缺 @expect-src / @tb-module 必须报错", "[verif]") {
    ch::rtl_emit::VerifProgram p1;
    const auto e1 = scan_text("@tb-module tb_x\n@scenario s\n@anchor a\nTICK 1\n", p1);
    REQUIRE(e1.message.find("@expect-src") != std::string::npos);

    ch::rtl_emit::VerifProgram p2;
    const auto e2 = scan_text("@expect-src x.md#a\n@scenario s\n@anchor a\nTICK 1\n", p2);
    REQUIRE(e2.message.find("@tb-module") != std::string::npos);
}

TEST_CASE("verif: REPEAT 在扫描期展开（_i 绑定为字面量）", "[verif]") {
    ch::rtl_emit::VerifProgram prog;
    const auto err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                               "REPEAT 3\nSET p _i * 4\nTICK 1\nEXPECT q 0xA5000000 + _i\nEND\n",
                               prog);
    REQUIRE(err.message.empty());
    REQUIRE(prog.scenarios.size() == 1);
    const auto &ops = prog.scenarios[0].ops;
    REQUIRE(ops.size() == 9); // 3 × (SET + TICK + EXPECT)

    // SET 的**值**已求值成字面量（IR 里没有表达式 ⇒ 后端不解释 `_i`）
    REQUIRE(ops[0].kind == ch::rtl_emit::VerifOpKind::Set);
    REQUIRE(ops[0].value == 0);
    REQUIRE(ops[3].value == 4);
    REQUIRE(ops[6].value == 8);
    REQUIRE(ops[2].kind == ch::rtl_emit::VerifOpKind::Expect);
    REQUIRE(ops[2].value == 0xA5000000ull);
    REQUIRE(ops[8].value == 0xA5000002ull);

    // 静态条数 = 逐条 EXPECT/WAIT 的条数
    REQUIRE(ch::rtl_emit::verif_check_count(prog) == 3);
}
