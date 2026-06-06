// tests/test_verilog_arith.cpp
//
// Verilog code-generation tests dedicated to the six arithmetic ch_op
// mappings: add, sub, mul, div, mod, neg. Each TEST_CASE builds a tiny
// design that exercises exactly one operator and asserts the emitted
// Verilog contains the correct Verilog operator symbol. This complements
// tests/test_verilog_gen.cpp (which covers reduce / extend / rotate / bit
// selection ops) and tests/test_operation_results.cpp (which is a
// simulator-level test, not a codegen test).
//
// Pattern reference: tests/test_verilog_gen.cpp::VerilogGen - AndReduce
// (and the rest of the Wave-3 block at lines ~177-397). The same
// helper, ctx-swap, and REQUIRE() style is used here.

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "core/context.h"
#include <memory>
#include <sstream>
#include <string>

using namespace ch;
using namespace ch::core;
using namespace ch::core::literals;

// Helper: capture the entire Verilog module as a string.
static std::string generateVerilogToString(context *ctx) {
    std::ostringstream oss;
    verilogwriter writer(ctx);
    writer.print(oss);
    return oss.str();
}

// Shared structural assertions for every arithmetic test. The op-specific
// assertion is the one that proves the operator mapped correctly; the
// structural assertions catch context / module regressions so the test
// fails for a clear reason if the module frame is broken.
static void requireBasicModuleFrame(const std::string &verilog,
                                    const std::string &op_name) {
    INFO("op=" << op_name);
    REQUIRE(verilog.find("module top") != std::string::npos);
    REQUIRE(verilog.find("endmodule") != std::string::npos);
    // All 6 tests wire through a single 8-bit output port named "io".
    REQUIRE(verilog.find("output [7:0] io") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 1. add : ch_op::add -> "lhs + rhs" in assign body.
//    print_binary_op (src/codegen_verilog.cpp:622) emits
//        "    assign <NAME> = <lhs> + <rhs>;\n"
//    so we look for " + " (space + plus + space) to disambiguate from
//    any other occurrence of '+' in the surrounding Verilog frame.
// ---------------------------------------------------------------------------
TEST_CASE("VerilogGen - ArithAdd", "[verilog][gen][arith][add]") {
    auto ctx = std::make_unique<ch::core::context>("arith_add_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_in<ch_uint<8>> b("b");
    ch_uint<8> sum = a + b; // ch_op::add
    ch_out<ch_uint<8>> out_port("io");
    out_port = sum;

    std::string verilog = generateVerilogToString(ctx.get());
    requireBasicModuleFrame(verilog, "add");

    // Binary add: lhs <space>+<space> rhs.
    REQUIRE(verilog.find(" + ") != std::string::npos);
    // Inputs must be declared.
    REQUIRE(verilog.find("input [7:0] a") != std::string::npos);
    REQUIRE(verilog.find("input [7:0] b") != std::string::npos);
    // Negative check: the unary-minus pattern from the neg test must
    // not appear here (catches a regression where add falls through to
    // unary neg emission).
    REQUIRE(verilog.find("=-a") == std::string::npos);
}

// ---------------------------------------------------------------------------
// 2. sub : ch_op::sub -> "lhs - rhs" in assign body.
//    Same print_binary_op path as add. The space-padded assertion
//    " - " rules out the assign keyword's "=" being mistaken for
//    a sub operator.
// ---------------------------------------------------------------------------
TEST_CASE("VerilogGen - ArithSub", "[verilog][gen][arith][sub]") {
    auto ctx = std::make_unique<ch::core::context>("arith_sub_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_in<ch_uint<8>> b("b");
    ch_uint<8> diff = a - b; // ch_op::sub
    ch_out<ch_uint<8>> out_port("io");
    out_port = diff;

    std::string verilog = generateVerilogToString(ctx.get());
    requireBasicModuleFrame(verilog, "sub");

    // Binary sub: lhs <space>-<space> rhs.
    REQUIRE(verilog.find(" - ") != std::string::npos);
    REQUIRE(verilog.find("input [7:0] a") != std::string::npos);
    REQUIRE(verilog.find("input [7:0] b") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 3. mul : ch_op::mul -> "lhs * rhs" in assign body.
// ---------------------------------------------------------------------------
TEST_CASE("VerilogGen - ArithMul", "[verilog][gen][arith][mul]") {
    auto ctx = std::make_unique<ch::core::context>("arith_mul_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_in<ch_uint<8>> b("b");
    ch_uint<8> prod = a * b; // ch_op::mul
    ch_out<ch_uint<8>> out_port("io");
    out_port = prod;

    std::string verilog = generateVerilogToString(ctx.get());
    requireBasicModuleFrame(verilog, "mul");

    REQUIRE(verilog.find(" * ") != std::string::npos);
    REQUIRE(verilog.find("input [7:0] a") != std::string::npos);
    REQUIRE(verilog.find("input [7:0] b") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 4. div : ch_op::div -> "lhs / rhs" in assign body. Verilog integer
//    division truncates toward zero (matches CppHDL's bv division).
// ---------------------------------------------------------------------------
TEST_CASE("VerilogGen - ArithDiv", "[verilog][gen][arith][div]") {
    auto ctx = std::make_unique<ch::core::context>("arith_div_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_in<ch_uint<8>> b("b");
    ch_uint<8> quot = a / b; // ch_op::div
    ch_out<ch_uint<8>> out_port("io");
    out_port = quot;

    std::string verilog = generateVerilogToString(ctx.get());
    requireBasicModuleFrame(verilog, "div");

    REQUIRE(verilog.find(" / ") != std::string::npos);
    REQUIRE(verilog.find("input [7:0] a") != std::string::npos);
    REQUIRE(verilog.find("input [7:0] b") != std::string::npos);
    // "/" must NOT be commented out or replaced with another symbol.
    REQUIRE(verilog.find("//") == std::string::npos);
}

// ---------------------------------------------------------------------------
// 5. mod : ch_op::mod -> "lhs % rhs" in assign body. Verilog's
//    remainder operator is "%" (IEEE 1364-2005 §5.1.10).
// ---------------------------------------------------------------------------
TEST_CASE("VerilogGen - ArithMod", "[verilog][gen][arith][mod]") {
    auto ctx = std::make_unique<ch::core::context>("arith_mod_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_in<ch_uint<8>> b("b");
    ch_uint<8> rem = a % b; // ch_op::mod
    ch_out<ch_uint<8>> out_port("io");
    out_port = rem;

    std::string verilog = generateVerilogToString(ctx.get());
    requireBasicModuleFrame(verilog, "mod");

    REQUIRE(verilog.find(" % ") != std::string::npos);
    REQUIRE(verilog.find("input [7:0] a") != std::string::npos);
    REQUIRE(verilog.find("input [7:0] b") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 6. neg : ch_op::neg -> "= -<src>;" (unary minus after the assign '=').
//    print_unary_op (src/codegen_verilog.cpp:579,605-606) emits
//        out << "    assign " << <node_name> << " = ";
//        ...
//        out << "-";
//        out << <src_name>;
//    so the actual emitted token sequence is "= -<src_name>;" (single
//    space between '=' and '-'). The "= -" assertion is the unary
//    fingerprint: it disambiguates from the binary sub form "a - b"
//    which is "<lhs> <space>-<space> <rhs>" (always 2+ characters on
//    each side of the '-'). Note: do NOT assert "=-" (no space) — the
//    codegen always emits a space between '=' and the operator.
// ---------------------------------------------------------------------------
TEST_CASE("VerilogGen - ArithNeg", "[verilog][gen][arith][neg]") {
    auto ctx = std::make_unique<ch::core::context>("arith_neg_test");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_in<ch_uint<8>> a("a");
    ch_uint<8> neg_a = -a; // ch_op::neg
    ch_out<ch_uint<8>> out_port("io");
    out_port = neg_a;

    std::string verilog = generateVerilogToString(ctx.get());
    requireBasicModuleFrame(verilog, "neg");

    // The op node's auto-generated name starts with "neg" (from
    // ch_op::neg), so the emitted line begins with "    assign neg".
    REQUIRE(verilog.find("assign neg") != std::string::npos);
    // Unary-minus fingerprint: '= -' (assign emits "= " then "-").
    REQUIRE(verilog.find("= -") != std::string::npos);
    // Source wire 'a' is referenced directly after the '-'.
    REQUIRE(verilog.find("-a;") != std::string::npos);
    // Binary form "a - <something>;" must NOT appear because neg
    // takes a single source and there is no second operand in this
    // design.
    REQUIRE(verilog.find("a -") == std::string::npos);
    REQUIRE(verilog.find("input [7:0] a") != std::string::npos);
}
